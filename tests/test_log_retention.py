#!/usr/bin/env python3
"""Isolated log-retention tests. Never access real user logs or start audio."""
from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
import viper_log_retention as logs


class LogRetentionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        env = patch.dict(os.environ, {
            "XDG_STATE_HOME": self.temporary.name + "/state",
            "XDG_CONFIG_HOME": self.temporary.name + "/config",
        })
        env.start()
        self.addCleanup(env.stop)

    def test_default_setting_and_keep_forever(self):
        self.assertEqual(logs.read_days(), 7)
        logs.save_days(0)
        self.assertEqual(logs.read_days(), 0)
        logs.append_line("backend", "untimestamped old diagnostic")
        self.assertEqual(logs.prune_log("backend",
            now=datetime.now(timezone.utc) + timedelta(days=500)), 0)
        self.assertIn("untimestamped old diagnostic", logs.log_path("backend").read_text())
        logs.save_days(14)
        self.assertEqual(logs.read_days(), 14)
        self.assertEqual(json.loads(logs.settings_file().read_text())["days"], 14)
        with self.assertRaises(ValueError):
            logs.save_days(3651)
        with self.assertRaises(ValueError):
            logs.save_days(-1)
        logs.settings_file().write_text('{"days":true}')
        self.assertEqual(logs.read_days(), 7)

    def test_age_cutoff_and_unknown_legacy_prefix(self):
        now = datetime(2026, 11, 8, 12, tzinfo=timezone.utc)
        path = logs.log_path("backend")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            "old legacy diagnostic without any timestamp\n"
            "[2026-10-30T21:00:00-04:00] [backend pid=10] expired\n"
            "expired continuation line\n"
            "[2026-11-07T22:55:52-05:00] [backend pid=11] recent\n"
            "new continuation line\n")
        self.assertEqual(logs.prune_log("backend", now=now, days=7), 3)
        self.assertEqual(path.read_text(),
            "[2026-11-07T22:55:52-05:00] [backend pid=11] recent\n"
            "new continuation line\n")
        self.assertEqual(logs.prune_log("backend", now=now, days=7), 0)

    def test_legacy_lines_remain_until_first_known_date_expires(self):
        now = datetime(2026, 11, 8, 12, tzinfo=timezone.utc)
        path = logs.log_path("output-monitor")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("pre-logging history\n"
            "[2026-11-07T11:00:00-05:00] [output-monitor pid=42] Started\n")
        self.assertEqual(logs.prune_log("output-monitor", now=now, days=7), 0)
        self.assertEqual(logs.prune_log("output-monitor",
            now=now + timedelta(days=8), days=7), 2)
        self.assertEqual(path.read_bytes(), b"")

    def test_untimestamped_file_uses_mtime(self):
        now = datetime(2026, 11, 8, tzinfo=timezone.utc)
        path = logs.log_path("backend")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("no timestamps anywhere\n")
        old = (now - timedelta(days=8)).timestamp()
        os.utime(path, (old, old))
        self.assertEqual(logs.prune_log("backend", now=now, days=7), 1)

    def test_healthy_log_does_not_allocate_copy(self):
        now = datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")
        logs.append_line("backend", f"[{now}] recent")
        with patch.object(logs.tempfile, "mkstemp", side_effect=AssertionError("Unexpected temporary copy")):
            self.assertEqual(logs.prune_all(), 0)

    def test_log_reader_fallback_does_not_close_backend_pipe(self):
        for script in ("scripts/start.sh", "packaging/arch/start-backend"):
            source = (ROOT / script).read_text()
            self.assertIn("|| cat >/dev/null", source)
        launcher = (ROOT / "src/viper_launcher.py").read_text()
        self.assertIn('"$1" "$2" stream output-monitor || cat >/dev/null', launcher)

    def test_stream_writer_combines_backend_and_monitor_records(self):
        line = b"[2026-11-08T09:00:00-05:00] [backend pid=4] diagnostic\n"
        result = subprocess.run(
            [sys.executable, str(ROOT / "src/viper_log_retention.py"),
             "stream", "backend"],
            input=line, capture_output=True, env=os.environ.copy(), timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(logs.log_path("backend").read_bytes(), line)
        self.assertEqual(logs.log_path("backend"), logs.log_path("output-monitor"))
        logs.append_line("output-monitor", "monitor startup")
        self.assertEqual(logs.log_path("backend").read_bytes(),
            line + b"monitor startup\n")
        self.assertFalse((logs.state_dir() / "output-monitor.log").exists())

    def test_concurrent_writer_and_retention_never_lose_live_lines(self):
        logs.save_days(7)
        stamp = datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")
        start = threading.Barrier(4)
        errors = []
        def writer(n):
            try:
                start.wait(timeout=5)
                for i in range(50):
                    logs.append_line("backend", f"[{stamp}] writer-{n}-{i}")
            except BaseException as exc:
                errors.append(exc)
        threads = [threading.Thread(target=writer, args=(n,)) for n in range(3)]
        for thread in threads:
            thread.start()
        start.wait(timeout=5)
        for _ in range(20):
            logs.prune_log("backend")
        for thread in threads:
            thread.join(timeout=5)
            self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])
        lines = logs.log_path("backend").read_text().splitlines()
        self.assertEqual(len(lines), 150)
        self.assertEqual(len(set(lines)), 150)


    def test_migrate_monitor_history_once_and_preserve_untimestamped_lines(self):
        legacy = logs.state_dir() / "output-monitor.log"
        legacy.parent.mkdir(parents=True, exist_ok=True)
        legacy.write_text("legacy monitor warning\n"
            "[2026-11-07T22:55:53-05:00] [output-monitor pid=42] Started\n")
        logs.append_line("backend", "[2026-11-08T10:00:00-05:00] [backend pid=4] started")
        self.assertTrue(logs.migrate_legacy_monitor())
        self.assertFalse(legacy.exists())
        self.assertFalse(logs.migrate_legacy_monitor())  # idempotent
        contents = logs.log_path("backend").read_text()
        self.assertIn("legacy monitor warning", contents)
        self.assertIn("[output-monitor pid=42] Started", contents)
        self.assertIn("[backend pid=4] started", contents)
        self.assertEqual(contents.count("BEGIN historical output-monitor.log import"), 1)
        self.assertEqual(contents.count("END historical output-monitor.log import"), 1)
        # New messages are interleaved into the same file with independent tags.
        logs.append_line("output-monitor", "[2026-11-08T10:01:00-05:00] [output-monitor pid=5] ready")
        self.assertIn("[output-monitor pid=5] ready", logs.log_path("backend").read_text())

    def test_prune_all_handles_one_combined_log(self):
        logs.save_days(7)
        now = datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")
        logs.append_line("backend", f"[{now}] [backend] retained")
        logs.append_line("output-monitor", "[2026-01-01T00:00:00+00:00] [output-monitor] expired")
        logs.append_line("output-monitor", f"[{now}] [output-monitor] retained")
        self.assertEqual(logs.prune_all(), 1)
        kept = logs.log_path("backend").read_text()
        self.assertIn("[backend] retained", kept)
        self.assertIn("[output-monitor] retained", kept)
        self.assertNotIn("expired", kept)
        self.assertFalse((logs.state_dir() / "output-monitor.log").exists())

    def test_menu_helper_changes_both_logs_immediately(self):
        old = "[2026-01-01T00:00:00+00:00] old\n"
        logs.append_line("backend", old)
        logs.append_line("output-monitor", old)
        result = subprocess.run(
            [sys.executable, str(ROOT / "src/viper_log_retention.py"),
             "set-days", "7"],
            capture_output=True, env=os.environ.copy(), timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(logs.log_path("backend").read_bytes(), b"")
        self.assertEqual(logs.log_path("output-monitor").read_bytes(), b"")
        self.assertEqual(logs.read_days(), 7)


if __name__ == "__main__":
    unittest.main()
