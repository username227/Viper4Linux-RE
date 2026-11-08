#!/usr/bin/env python3
"""User-configurable age-based retention for ViPER's two session logs.

All new writers reopen the log for each record under a stable sidecar flock.
Cleanup atomically replaces the file under the same lock, so long-lived
PipeWire and monitor processes never append to an outdated inode.
No DSP thread uses this module.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timedelta, timezone
import fcntl
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import time

DEFAULT_DAYS = 7
MAX_DAYS = 3650
LOG_NAMES = {"backend", "output-monitor"}
STAMP = re.compile(rb"^\[(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}[+-]\d{2}:\d{2})\]")
_LAST_CLEANUP = 0.0


def state_dir() -> Path:
    return Path(os.environ.get("XDG_STATE_HOME", str(Path.home() / ".local/state"))) / "viper4linux"


def settings_file() -> Path:
    return Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config"))) / "viper4linux/log-retention.json"


def read_days() -> int:
    try:
        value = json.loads(settings_file().read_text(encoding="utf-8"))["days"]
        if type(value) is int and 0 <= value <= MAX_DAYS:
            return value
    except (OSError, ValueError, KeyError, TypeError):
        pass
    return DEFAULT_DAYS


def save_days(days: int) -> None:
    if type(days) is not int or not 0 <= days <= MAX_DAYS:
        raise ValueError("Log retention must be between 0 and 3650 days")
    path = settings_file()
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".log-retention-", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as out:
            json.dump({"days": days}, out)
            out.write("\n")
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def log_path(name: str) -> Path:
    if name not in LOG_NAMES:
        raise ValueError(f"Unknown ViPER log: {name}")
    return state_dir() / (name + ".log")


@contextmanager
def locked_log(name: str):
    path = log_path(name)
    path.parent.mkdir(parents=True, exist_ok=True)
    # Stable lock-file inode even when log contents are replaced.
    with (path.parent / (path.name + ".lock")).open("a+b") as lock:
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
        try:
            yield path
        finally:
            fcntl.flock(lock.fileno(), fcntl.LOCK_UN)


def append_record(name: str, record: bytes) -> None:
    with locked_log(name) as path:
        with path.open("ab") as out:
            out.write(record)


def append_line(name: str, line: str) -> None:
    append_record(name, (line.rstrip("\n") + "\n").encode("utf-8", errors="replace"))


def line_time(line: bytes):
    match = STAMP.match(line)
    if not match:
        return None
    try:
        return datetime.fromisoformat(match.group(1).decode("ascii")).astimezone(timezone.utc)
    except ValueError:
        return None


def prune_log(name: str, *, now: datetime | None = None, days: int | None = None) -> int:
    keep_days = read_days() if days is None else days
    if not 0 <= keep_days <= MAX_DAYS:
        raise ValueError("Invalid retention days")
    if keep_days == 0:
        return 0  # Explicit "keep indefinitely" option.
    current = now or datetime.now(timezone.utc)
    cutoff = current.astimezone(timezone.utc) - timedelta(days=keep_days)
    with locked_log(name) as path:
        if not path.is_file() or path.stat().st_size == 0:
            return 0
        # Unknown historical prefix predating timestamps uses the first known
        # timestamp as its conservative retention reference.
        with path.open("rb") as source:
            first_stamp = next((stamp for line in source if (stamp := line_time(line))), None)
        last_time = first_stamp or datetime.fromtimestamp(path.stat().st_mtime, timezone.utc)
        fd, temporary = tempfile.mkstemp(prefix=".viper-log-", dir=path.parent)
        removed = 0
        try:
            with os.fdopen(fd, "wb") as output, path.open("rb") as source:
                for line in source:
                    stamp = line_time(line)
                    if stamp is not None:
                        last_time = stamp
                    if last_time >= cutoff:
                        output.write(line)
                    else:
                        removed += 1
            if removed:
                os.replace(temporary, path)
            return removed
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)


def prune_all() -> int:
    return sum(prune_log(name) for name in ("backend", "output-monitor"))


def maybe_prune() -> None:
    global _LAST_CLEANUP
    tick = time.monotonic()
    if _LAST_CLEANUP and tick - _LAST_CLEANUP < 3600:
        return
    _LAST_CLEANUP = tick
    try:
        prune_all()
    except OSError as exc:
        print(f"ViPER: log retention cleanup failed: {exc}", file=sys.stderr)


def stream(name: str) -> int:
    # Pipe reader, outside the audio callback. The original timestamped text
    # is retained verbatim; a direct terminal launch still shows its output.
    display = sys.stdout.isatty()
    for line in sys.stdin.buffer:
        try:
            append_record(name, line)
            maybe_prune()
        except OSError as exc:
            # Continue draining: never cause an EPIPE in the PipeWire client.
            print(f"ViPER: cannot write {name}.log: {exc}", file=sys.stderr)
        if display:
            sys.stdout.buffer.write(line)
            sys.stdout.buffer.flush()
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="ViPER log retention and safe stream writer")
    actions = parser.add_subparsers(dest="action", required=True)
    sender = actions.add_parser("stream")
    sender.add_argument("name", choices=sorted(LOG_NAMES))
    setter = actions.add_parser("set-days")
    setter.add_argument("days", type=int)
    actions.add_parser("get-days")
    actions.add_parser("prune")
    args = parser.parse_args()
    if args.action == "stream":
        return stream(args.name)
    if args.action == "get-days":
        print(read_days())
    elif args.action == "set-days":
        save_days(args.days)
        prune_all()
    elif args.action == "prune":
        prune_all()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"ViPER log retention: {error}", file=sys.stderr)
        sys.exit(1)
