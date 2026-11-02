"""Persistent Audio Output inventory and snapshots (not the named preset library).

Output identities are SHA-256 keys from viper_output_graph, not user-supplied
filenames or ephemeral PipeWire object ids. No Qt/application preferences are
overwritten by the store. Output settings are stored separately from presets.
"""
from __future__ import annotations

import fcntl
import json
import os
from pathlib import Path
import re
import tempfile
from typing import Any

_KEY = re.compile(r'^[a-f0-9]{64}$')
_FORMAT = 1


def default_directory() -> Path:
    return Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'viper4linux' / 'outputs'


def _atomic_json(path: Path, content: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd, temp = tempfile.mkstemp(prefix='.write-', dir=str(path.parent))
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, 'w', encoding='utf-8') as out:
            json.dump(content, out, indent=2, sort_keys=True)
            out.write('\n')
            out.flush()
            os.fsync(out.fileno())
        os.replace(temp, path)
        parent_fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(parent_fd)
        finally:
            os.close(parent_fd)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)


def _read_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    value = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(value, dict):
        raise ValueError(f'Invalid output state file: {path.name}')
    return value


class OutputStore:
    """Versioned, atomic store. Does not modify application presets."""
    def __init__(self, root: Path | None = None):
        self.root = Path(root) if root is not None else default_directory()
        self.root.mkdir(parents=True, exist_ok=True, mode=0o700)
        self._lock = self.root / '.lock'

    def _check(self, key: str) -> None:
        if not _KEY.fullmatch(key):
            raise ValueError('Invalid Audio Output identifier')

    def _locked(self):
        class Lock:
            def __init__(self, path):
                self.path = path
                self.fd = None
            def __enter__(self):
                self.fd = os.open(self.path, os.O_CREAT | os.O_RDWR | os.O_CLOEXEC, 0o600)
                fcntl.flock(self.fd, fcntl.LOCK_EX)
                return self
            def __exit__(self, *exc):
                fcntl.flock(self.fd, fcntl.LOCK_UN)
                os.close(self.fd)
        return Lock(self._lock)

    def inventory(self) -> dict[str, dict[str, Any]]:
        obj = _read_json(self.root / 'inventory.json')
        if not obj:
            return {}
        if obj.get('format') != _FORMAT or not isinstance(obj.get('outputs'), dict):
            raise ValueError('Unsupported Audio Output inventory version')
        return obj['outputs']

    def remember(self, output) -> None:
        self._check(output.key)
        if not isinstance(output.description, str) or not isinstance(output.node_name, str):
            raise ValueError('Invalid Audio Output properties')
        with self._locked():
            inventory = self.inventory()
            inventory[output.key] = {
                'description': output.description,
                'node_name': output.node_name,
            }
            _atomic_json(self.root / 'inventory.json', {'format': _FORMAT, 'outputs': inventory})

    def save_settings(self, key: str, snapshot: dict[str, Any]) -> None:
        self._check(key)
        if not isinstance(snapshot, dict) or snapshot.get('format') != _FORMAT:
            raise ValueError('Invalid Audio Output settings snapshot')
        with self._locked():
            _atomic_json(self.root / f'{key}.json', snapshot)

    def settings(self, key: str) -> dict[str, Any] | None:
        self._check(key)
        data = _read_json(self.root / f'{key}.json')
        if not data:
            return None
        if data.get('format') != _FORMAT:
            raise ValueError('Unsupported Audio Output snapshot version')
        return data

    def enabled(self) -> bool:
        value = _read_json(self.root / 'options.json')
        return value.get('remember_outputs') is True

    def set_enabled(self, value: bool) -> None:
        with self._locked():
            _atomic_json(self.root / 'options.json', {'format': _FORMAT, 'remember_outputs': bool(value)})
