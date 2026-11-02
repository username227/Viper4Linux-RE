#!/usr/bin/env python3
"""Offline adapter tests; all native subprocesses are fake, never live IPC."""
from pathlib import Path
import os
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from viper_output_monitor import NativeEffects

class AdapterTests(unittest.TestCase):
    def test_backend_instance_guard_and_passthrough(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            marker = root / 'owner'
            marker.write_text('123\n')
            helper = root / 'fake-helper'
            helper.write_text(
                '#!/usr/bin/env python3\n'
                'import json,sys\n'
                'from pathlib import Path\n'
                f'owner = int(Path({str(marker)!r}).read_text())\n'
                'if sys.argv[1] == "capture":\n'
                '  print(json.dumps({"format":1,"backend_pid":owner,"bass":12}))\n'
                'elif sys.argv[1] == "restore":\n'
                '  state=json.load(sys.stdin)\n'
                '  assert state["bass"] == 12\n'
                'else: sys.exit(2)\n')
            helper.chmod(0o700)
            effects = NativeEffects(helper)
            snap = effects.capture()
            self.assertEqual(snap, {'format': 1, 'bass': 12})
            effects.restore(snap)
            marker.write_text('999\n')
            with self.assertRaisesRegex(RuntimeError, 'DSP session changed'):
                effects.capture()
            self.assertEqual(effects.capture(), snap)

    def test_missing_helper_refuses_to_modify_audio(self):
        with tempfile.TemporaryDirectory() as directory:
            effects = NativeEffects(Path(directory) / 'not-installed')
            with self.assertRaisesRegex(RuntimeError, 'not installed'):
                effects.capture()
            with self.assertRaisesRegex(RuntimeError, 'not installed'):
                effects.restore({'format': 1})

if __name__ == '__main__':
    unittest.main()
