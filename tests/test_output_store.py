#!/usr/bin/env python3
import fcntl
import importlib.util
import os
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch
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


    def test_lock_excludes_other_descriptors_and_releases_on_exception(self):
        with tempfile.TemporaryDirectory() as temp:
            store = module.OutputStore(pathlib.Path(temp))
            with self.assertRaisesRegex(RuntimeError, 'synthetic failure'):
                with store._locked():
                    # A different open-file description cannot acquire the lock.
                    other_fd = os.open(store._lock, os.O_RDWR | os.O_CLOEXEC)
                    try:
                        with self.assertRaises(BlockingIOError):
                            fcntl.flock(other_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    finally:
                        os.close(other_fd)
                    raise RuntimeError('synthetic failure')
            # Context exit releases the lock even when its body raises.
            other_fd = os.open(store._lock, os.O_RDWR | os.O_CLOEXEC)
            try:
                fcntl.flock(other_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                fcntl.flock(other_fd, fcntl.LOCK_UN)
            finally:
                os.close(other_fd)

    def test_failed_lock_acquisition_closes_descriptor(self):
        with tempfile.TemporaryDirectory() as temp:
            store = module.OutputStore(pathlib.Path(temp))
            opened = []
            real_open = os.open

            def tracked_open(*args, **kwargs):
                fd = real_open(*args, **kwargs)
                opened.append(fd)
                return fd

            with patch.object(module.os, 'open', side_effect=tracked_open), \
                 patch.object(module.fcntl, 'flock', side_effect=OSError('synthetic lock failure')):
                with self.assertRaisesRegex(OSError, 'synthetic lock failure'):
                    store.set_enabled(True)

            self.assertEqual(len(opened), 1)
            with self.assertRaises(OSError):
                os.fstat(opened[0])
            self.assertFalse((store.root / 'options.json').exists())

if __name__ == '__main__':
    unittest.main()
