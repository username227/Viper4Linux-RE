#!/usr/bin/env python3
"""Regression guard for pre-playback Audio Output restoration."""
from pathlib import Path
import re
import unittest

CONFIG = Path(__file__).resolve().parents[1] / "routing-config/filter-chain.conf.d/99-viper-smart.conf"

class SmartFilterIdleTests(unittest.TestCase):
    def test_smart_capture_keeps_dsp_running_without_music(self):
        config = CONFIG.read_text(encoding="utf-8")
        capture = re.search(r'capture\.props\s*=\s*\{([^}]*)\}', config, re.S)
        self.assertIsNotNone(capture, "smart filter capture.props missing")
        self.assertRegex(capture.group(1), r'(?m)^\s*node\.always-process\s*=\s*true\s*$')
        self.assertIn('node.name = "viperbass.smart.capture"', capture.group(1))

if __name__ == "__main__":
    unittest.main()
