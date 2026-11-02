#!/usr/bin/env python3
"""Pure output-monitor unit tests; never touch a live backend."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from viper_output_graph import AudioOutput
from viper_output_store import OutputStore
from viper_output_monitor import write_status, status_for

class StubGraph:
    def __init__(self, active, connected):
        self._active, self._connected = active, connected
    def active(self): return self._active
    def available(self): return self._connected

class MonitorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.store = OutputStore(self.root / 'outputs')
        self.first = AudioOutput('a'*64, 'EDIFIER NeoBuds Planar', 'bluez_output.first', 91)
        self.second = AudioOutput('b'*64, 'EDIFIER NeoBuds Planar', 'bluez_output.second', 92)
    def test_same_name_outputs_and_offline_inventory(self):
        self.store.remember(self.first)
        self.store.remember(self.second)
        document = status_for(StubGraph(self.first, [self.first]), self.store, 123)
        self.assertEqual(document['active']['key'], self.first.key)
        self.assertEqual(len(document['outputs']), 2)
        self.assertFalse(next(o for o in document['outputs'] if o['key'] == self.second.key)['connected'])
        self.assertFalse(document['memory_enabled'])
    def test_status_is_atomic_and_not_a_preset(self):
        path = self.root / 'status.json'
        write_status(path, status_for(StubGraph(self.first, [self.first]), self.store, 123))
        self.assertEqual(json.loads(path.read_text())['active']['key'], self.first.key)
        self.assertEqual(path.stat().st_mode & 0o777, 0o600)

if __name__ == '__main__':
    unittest.main()
