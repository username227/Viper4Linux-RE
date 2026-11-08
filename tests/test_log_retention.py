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

    def test_stream_writer_preserves_bytes_and_separate_logs(self):
        line = b"[2026-11-08T09:00:00-05:00] [backend pid=4] diagnostic\n"
        result = subprocess.run(
            [sys.executable, str(ROOT / "src/viper_log_retention.py"),
             "stream", "backend"],
            input=line, capture_output=True, env=os.environ.copy(), timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(logs.log_path("backend").read_bytes(), line)
        self.assertFalse(logs.log_path("output-monitor").exists())
        logs.append_line("output-monitor", "monitor startup")
        self.assertEqual(logs.log_path("output-monitor").read_text(), "monitor startup\n")

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
