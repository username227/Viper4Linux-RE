#!/usr/bin/env python3
"""Offline preset-assignment checks: no PipeWire or live DSP operations."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
from viper_output_graph import AudioOutput
from viper_output_store import OutputStore
from viper_output_memory import OutputMemory
from viper_output_monitor import process_assignments

class Effects:
    def __init__(self):
        self.restores = []
    def preset(self, path):
        return {"format": 1, "preset": path.stem}
    def restore(self, snapshot):
        self.restores.append(snapshot)
    def capture(self):
        return {"format": 1, "preset": "live"}

class AssignmentTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.runtime = self.root / "run"
        self.runtime.mkdir()
        self.data = self.root / "data"
        self.library = self.data / "viper4linux/presets"
        self.library.mkdir(parents=True)
        self.preset = self.library / "named.ini"
        self.preset.write_text("[format]\nversion=1\n")
        self.store = OutputStore(self.data / "viper4linux/outputs")
        self.store.set_enabled(True)
        self.a = AudioOutput("a"*64, "Speakers", "alsa.speakers", 12)
        self.b = AudioOutput("b"*64, "Edifiers", "bluez.edifiers", 13)
        self.store.remember(self.a)
        self.store.remember(self.b)
        self.effects = Effects()
        self.memory = OutputMemory(self.store, self.effects)
        self.memory.current = self.b
        self.env = patch.dict(os.environ, {"XDG_DATA_HOME": str(self.data)})
        self.env.start()
        self.addCleanup(self.env.stop)

    def submit(self, target, preset=None):
        token = "testcase"
        (self.runtime / f"viper4linux-assign-{token}.request.json").write_text(
            json.dumps({"target": target, "preset": str(preset or self.preset)}))
        return self.runtime / f"viper4linux-assign-{token}.result.json"

    def test_offline_assignment_leaves_live_dsp_alone(self):
        ack = self.submit(self.a.key)
        self.assertFalse(process_assignments(self.runtime, self.store, self.effects,
                                             self.memory, self.b, True))
        self.assertTrue(json.loads(ack.read_text())["ok"])
        self.assertEqual(self.store.settings(self.a.key)["preset"], "named")
        self.assertEqual(self.effects.restores, [])
        self.assertEqual(self.memory.current.key, self.b.key)

    def test_active_assignment_restores_and_remembers(self):
        self.memory.current = self.a
        ack = self.submit(self.a.key)
        self.assertTrue(process_assignments(self.runtime, self.store, self.effects,
                                            self.memory, self.a, True))
        self.assertTrue(json.loads(ack.read_text())["ok"])
        self.assertEqual(len(self.effects.restores), 1)
        self.assertEqual(self.store.settings(self.a.key), self.effects.restores[0])

    def test_unsettled_active_assignment_fails_closed(self):
        self.memory.current = self.a
        ack = self.submit(self.a.key)
        self.assertFalse(process_assignments(self.runtime, self.store, self.effects,
                                             self.memory, self.a, False))
        self.assertFalse(json.loads(ack.read_text())["ok"])
        self.assertIsNone(self.store.settings(self.a.key))
        self.assertEqual(self.effects.restores, [])

    def test_invalid_target_fails_closed(self):
        ack = self.submit("c"*64)
        self.assertFalse(process_assignments(self.runtime, self.store, self.effects,
                                             self.memory, self.b, True))
        self.assertFalse(json.loads(ack.read_text())["ok"])
        self.assertEqual(self.effects.restores, [])

if __name__ == "__main__":
    unittest.main()
