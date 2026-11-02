#!/usr/bin/env python3
import importlib.util
import pathlib
import sys
import tempfile
import unittest
from types import SimpleNamespace

SRC = pathlib.Path(__file__).resolve().parents[1] / 'src/viper_output_store.py'
spec = importlib.util.spec_from_file_location('viper_output_store', SRC)
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)

class StoreTests(unittest.TestCase):
    def test_remember_and_snapshots_without_preset_side_effects(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            store = module.OutputStore(root)
            key = 'e' * 64
            device = SimpleNamespace(key=key, description='Headphones', node_name='bluez_output.A2DP')
            store.remember(device)
            self.assertEqual(store.inventory()[key]['description'], 'Headphones')
            self.assertFalse(store.enabled())
            store.set_enabled(True)
            self.assertTrue(store.enabled())
            sample = {'format': 1, 'master_enabled': True, 'bass': 300}
            store.save_settings(key, sample)
            self.assertEqual(store.settings(key), sample)
            self.assertFalse((root / 'presets').exists())
            with self.assertRaises(ValueError):
                store.save_settings('../../hack', sample)
            with self.assertRaises(ValueError):
                store.save_settings(key, {'not_versioned': True})

if __name__ == '__main__':
    unittest.main()
