#!/usr/bin/env python3
"""Offline regression tests for ViPER lifecycle logs (no PipeWire/audio started)."""
from __future__ import annotations

from contextlib import redirect_stderr
from datetime import datetime
import io
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))
import viper_launcher
import viper_output_monitor

STAMP = re.compile(r"^\[(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}[+-]\d{2}:\d{2})\]")
LOGGER = re.compile(r"(?ms)^backend_pid=\$\$\nexec > >\(\n.*?^\) 2>&1$")


def check_stamp(line: str) -> None:
    match = STAMP.match(line)
    if not match:
        raise AssertionError(f"missing timestamp: {line!r}")
    assert datetime.fromisoformat(match.group(1)).utcoffset() is not None


class BackendLifecycleLogTests(unittest.TestCase):
    def test_launcher_timestamp_record(self):
        with tempfile.TemporaryDirectory() as tmp:
            with patch.dict(os.environ, {"XDG_STATE_HOME": tmp}):
                viper_launcher.record_backend_event("test backend stop requested")
            line = (Path(tmp) / "viper4linux/backend.log").read_text().strip()
            check_stamp(line)
            self.assertIn("[launcher] test backend stop requested", line)

    def test_output_monitor_timestamp(self):
        output = io.StringIO()
        with redirect_stderr(output):
            viper_output_monitor.log_message("backend no longer detected")
        line = output.getvalue().strip()
        check_stamp(line)
        self.assertIn("[output-monitor pid=", line)
        self.assertIn("backend no longer detected", line)

    def test_both_start_scripts_timestamp_output_and_preserve_process_pid(self):
        for script_path in ("scripts/start.sh", "packaging/arch/start-backend"):
            with self.subTest(script=script_path):
                source = (ROOT / script_path).read_text()
                match = LOGGER.search(source)
                self.assertIsNotNone(match, "Missing backend log stream timestamp block")
                with tempfile.TemporaryDirectory() as tmp:
                    log = Path(tmp) / "viper4linux/backend.log"
                    source_block = match.group(0)
                    # Installed and source scripts share the same helper, but
                    # the installed absolute path is absent in this isolated test.
                    for name in ("src/viper_log_retention.py",
                                 "/usr/lib/viper4linux/viper_log_retention.py"):
                        source_block = source_block.replace(
                            "python3 " + name,
                            "python3 " + shlex.quote(str(ROOT / "src/viper_log_retention.py")))
                    # Exercise only the logging block with a dummy process. Never
                    # launch PipeWire or touch the user's running backend.
                    command = ("set -euo pipefail\n"
                               "exec 9>" + subprocess.list2cmdline([str(Path(tmp) / 'backend.lock')]) + "\n"
                               "flock -n 9\n" + source_block + "\n"
                               "echo 'dummy startup'\n"
                               "echo 'dummy stderr' >&2\n"
                               "exec bash -c 'echo dummy pipewire; sleep 0.05'\n")
                    env = {**os.environ, "XDG_STATE_HOME": tmp,
                           "XDG_CONFIG_HOME": tmp}
                    result = subprocess.run(["bash", "-c", command],
                                            stdin=subprocess.DEVNULL,
                                            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
                                            env=env, timeout=5, check=False)
                    self.assertEqual(result.returncode, 0)
                    deadline = time.monotonic() + 2
                    while time.monotonic() < deadline:
                        # Bash process substitution is asynchronous: the parent
                        # can exit before the log writer has created its file.
                        # Poll for the file as well as the closing record.
                        try:
                            lines = log.read_text().splitlines()
                        except FileNotFoundError:
                            time.sleep(0.02)
                            continue
                        if any("output stream closed" in line for line in lines):
                            break
                        time.sleep(0.02)
                    else:
                        self.fail("Timestamp logger did not close after backend process exited")
                    self.assertEqual(len(lines), 4)
                    for line in lines:
                        check_stamp(line)
                        self.assertRegex(line, r"\[backend pid=\d+\]")
                    self.assertIn("dummy startup", lines[0])
                    self.assertIn("dummy stderr", lines[1])
                    self.assertIn("dummy pipewire", lines[2])
                    self.assertIn("output stream closed", lines[3])


if __name__ == "__main__":
    unittest.main()
