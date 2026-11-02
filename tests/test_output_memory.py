#!/usr/bin/env python3
import importlib.util
import pathlib
import sys
import tempfile
import unittest

SRC = pathlib.Path(__file__).resolve().parents[1] / 'src'
sys.path.insert(0, str(SRC))
from viper_output_graph import AudioOutput
from viper_output_memory import OutputMemory
from viper_output_store import OutputStore


class DSP:
    def __init__(self):
        self.level = 2
        self.restores = []
    def capture(self):
        return {'format': 1, 'bass': self.level}
    def restore(self, snap):
        self.level = snap['bass']
        self.restores.append(snap['bass'])


class MemoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.dsp = DSP()
        self.memory = OutputMemory(OutputStore(pathlib.Path(self.temp.name)), self.dsp)
        self.a = AudioOutput('a'*64, 'Headphones', 'bluez_output.first', 9)
        self.b = AudioOutput('b'*64, 'Speakers', 'alsa_output.second', 15)

    def test_remember_independently_and_no_unexpected_reset(self):
        self.assertEqual(self.memory.observe(self.a), 'disabled')
        self.memory.enable()
        self.assertEqual(self.dsp.level, 2)
        self.dsp.level = 5
        self.assertEqual(self.memory.observe(self.b), 'initialized')
        self.assertEqual(self.dsp.level, 5)
        self.dsp.level = 9
        self.assertEqual(self.memory.observe(self.a), 'restored')
        self.assertEqual(self.dsp.level, 5)
        self.assertEqual(self.memory.observe(self.b), 'restored')
        self.assertEqual(self.dsp.level, 9)
        self.memory.disable()
        self.dsp.level = 3
        self.assertEqual(self.memory.observe(self.a), 'disabled')
        self.assertEqual(self.dsp.level, 3)

    def test_unknown_graph_and_identical_sink_do_not_change_settings(self):
        self.memory.observe(self.a)
        self.memory.enable()
        self.dsp.level = 7
        self.assertEqual(self.memory.observe(None), 'unknown')
        self.assertEqual(self.memory.observe(self.a), 'same')
        self.memory.checkpoint()
        self.assertEqual(self.memory.store.settings(self.a.key)['bass'], 7)

    def test_restore_failure_never_overwrites_target(self):
        self.memory.observe(self.a)
        self.memory.enable()
        self.memory.observe(self.b)
        self.memory.store.save_settings(self.a.key, {'format': 1, 'bass': 33})
        class Broken(DSP):
            def restore(self, snap):
                raise RuntimeError('backend disconnected')
        self.memory.effects = Broken()
        with self.assertRaises(RuntimeError):
            self.memory.observe(self.a)
        self.assertEqual(self.memory.current.key, self.b.key)
        self.assertEqual(self.memory.store.settings(self.a.key)['bass'], 33)

if __name__ == '__main__':
    unittest.main()
