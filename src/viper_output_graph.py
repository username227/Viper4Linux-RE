"""PipeWire Audio Output discovery, without fuzzy name matching.

This module is intentionally independent of ViPER's DSP and Qt widgets. It
consumes the structured snapshots/change arrays emitted by ``pw-dump -m``.
It never interprets a human-readable description as a persistent identifier.
"""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import re
from typing import Any

# Accept only complete six-octet Bluetooth hardware addresses.
_BT_ADDRESS = re.compile(r"^[0-9A-Fa-f]{2}(?:(?:_|:)[0-9A-Fa-f]{2}){5}$")
_BT_CARD = re.compile(r"^bluez_card\.([0-9A-Fa-f]{2}(?:_[0-9A-Fa-f]{2}){5})$")

SMART_CAPTURE = 'viperbass.smart.capture'
SMART_PLAYBACK = 'viperbass.smart.playback'


def _props(obj: dict[str, Any]) -> dict[str, Any]:
    return obj.get('info', {}).get('props', obj.get('props', {})) or {}


def _number(value: Any) -> int | None:
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


@dataclass(frozen=True)
class AudioOutput:
    key: str
    description: str
    node_name: str
    node_id: int
    connected: bool = True


class PipeWireGraph:
    """Applies full/delta JSON object arrays from a single pw-dump process."""

    def __init__(self) -> None:
        self.objects: dict[int, dict[str, Any]] = {}

    def update(self, batch: list[dict[str, Any]]) -> None:
        if not isinstance(batch, list):
            raise ValueError('PipeWire update must be a JSON array')
        for obj in batch:
            if not isinstance(obj, dict):
                continue
            obj_id = _number(obj.get('id'))
            if obj_id is None:
                continue
            # pw-dump -m may send an id-only object when an object disappears.
            if 'type' not in obj:
                self.objects.pop(obj_id, None)
                continue
            # Changed objects can omit fields; merge with their prior snapshot.
            before = self.objects.get(obj_id, {})
            combined = dict(before)
            combined.update(obj)
            if isinstance(before.get('info'), dict) and isinstance(obj.get('info'), dict):
                info = dict(before['info'])
                info.update(obj['info'])
                if isinstance(before['info'].get('props'), dict) and isinstance(obj['info'].get('props'), dict):
                    info['props'] = {**before['info']['props'], **obj['info']['props']}
                combined['info'] = info
            self.objects[obj_id] = combined

    def _nodes(self) -> list[dict[str, Any]]:
        return [o for o in self.objects.values() if o.get('type') == 'PipeWire:Interface:Node']

    def _physical(self, node: dict[str, Any]) -> bool:
        p = _props(node)
        if p.get('media.class') != 'Audio/Sink':
            return False
        if p.get('filter.smart') in (True, 'true', '1'):
            return False
        if p.get('node.name') in (SMART_CAPTURE, SMART_PLAYBACK):
            return False
        # Device-backed output: never mistake another software/virtual sink for hardware.
        return _number(p.get('device.id')) is not None

    def _device(self, node: dict[str, Any]) -> dict[str, Any]:
        index = _number(_props(node).get('device.id'))
        return _props(self.objects.get(index, {})) if index is not None else {}

    def output_for_node(self, node: dict[str, Any]) -> AudioOutput | None:
        if not self._physical(node):
            return None
        p = _props(node)
        device = self._device(node)
        node_name = p.get('node.name')
        device_name = device.get('device.name')
        if not isinstance(node_name, str) or not node_name:
            return None
        if not isinstance(device_name, str) or not device_name:
            return None  # Fail closed: no stable device association.
        # Bluetooth profiles can change node.name (A2DP, HFP, codec, etc.).
        # Identify the *hardware*, not the stream profile or display name.
        # A six-octet address is required: never merge two identical model names.
        is_bluetooth = (device_name.startswith('bluez_card.') or
                        device.get('device.api') == 'bluez5')
        if is_bluetooth:
            address = device.get('api.bluez5.address') or device.get('bluez5.address')
            if not isinstance(address, str) or not _BT_ADDRESS.fullmatch(address):
                card = _BT_CARD.fullmatch(device_name)
                if card is None:
                    return None  # Unknown hardware identity: do not guess.
                address = card.group(1)
            address = address.replace('_', ':').upper()
            identity = f'pipewire-bluetooth-v2\0{address}'
        else:
            # Keep existing ALSA keys: a physical card can expose independently
            # configured speaker and headphone outputs through different nodes.
            identity = f'pipewire-device-v1\0{device_name}\0{node_name}'
        key = hashlib.sha256(identity.encode('utf-8')).hexdigest()
        description = p.get('node.description') or p.get('node.nick') or node_name
        return AudioOutput(key, str(description), node_name, int(node['id']))

    def available(self) -> list[AudioOutput]:
        # A Bluetooth device may briefly expose more than one output profile.
        # A single hardware identity must appear only once in the picker.
        found = [self.output_for_node(node) for node in self._nodes()]
        unique = {}
        for output in found:
            if output is not None:
                unique.setdefault(output.key, output)
        return sorted(unique.values(), key=lambda x: (x.description.casefold(), x.key))

    def _default_name(self) -> str | None:
        for metadata in self.objects.values():
            if metadata.get('type') != 'PipeWire:Interface:Metadata':
                continue
            if _props(metadata).get('metadata.name') != 'default':
                continue
            for entry in metadata.get('metadata', []):
                if entry.get('subject') != 0 or entry.get('key') != 'default.audio.sink':
                    continue
                value = entry.get('value')
                if isinstance(value, str):
                    try:
                        value = json.loads(value)
                    except ValueError:
                        continue
                if isinstance(value, dict) and isinstance(value.get('name'), str):
                    return value['name']
        return None

    def active(self) -> AudioOutput | None:
        nodes = {int(n['id']): n for n in self._nodes()}
        # For a smart filter, the exact PipeWire link from its playback node
        # identifies the *real* destination. Do not treat the virtual sink as
        # a device or assume that the metadata default is physical.
        playback_ids = {idx for idx, node in nodes.items()
                        if _props(node).get('node.name') == SMART_PLAYBACK}
        linked: dict[int, int] = {}
        for obj in self.objects.values():
            if obj.get('type') != 'PipeWire:Interface:Link':
                continue
            p = _props(obj)
            source = _number(p.get('link.output.node'))
            destination = _number(p.get('link.input.node'))
            if source in playback_ids and destination in nodes:
                linked[destination] = linked.get(destination, 0) + 1
        if linked:
            # Two channels commonly create two links. If the graph temporarily
            # connects to multiple physical sinks, refuse to guess.
            resolved = [self.output_for_node(nodes[idx]) for idx in linked]
            resolved = [o for o in resolved if o is not None]
            if len(resolved) == 1:
                return resolved[0]
            return None
        name = self._default_name()
        if not name:
            return None
        matching = [n for n in nodes.values() if _props(n).get('node.name') == name]
        if len(matching) != 1:
            return None
        return self.output_for_node(matching[0])


class JsonArrayStream:
    """Extract adjacent JSON arrays from partial bytes, respecting strings."""

    def __init__(self, limit: int = 8 * 1024 * 1024) -> None:
        self.buf = bytearray()
        self.limit = limit

    def feed(self, chunk: bytes) -> list[list[dict[str, Any]]]:
        self.buf.extend(chunk)
        if len(self.buf) > self.limit:
            raise ValueError('PipeWire event stream exceeds size limit')
        results = []
        start = None
        depth = 0
        quoted = False
        escaped = False
        end_offset = 0
        for i, b in enumerate(self.buf):
            if start is None:
                if chr(b).isspace():
                    end_offset = i + 1
                    continue
                if b != ord('['):
                    raise ValueError('Expected JSON array in PipeWire stream')
                start = i
            if quoted:
                if escaped:
                    escaped = False
                elif b == ord('\\'):
                    escaped = True
                elif b == ord('"'):
                    quoted = False
                continue
            if b == ord('"'):
                quoted = True
            elif b == ord('['):
                depth += 1
            elif b == ord(']'):
                depth -= 1
                if depth == 0:
                    obj = json.loads(self.buf[start:i + 1])
                    if not isinstance(obj, list):
                        raise ValueError('Expected array')
                    results.append(obj)
                    start = None
                    end_offset = i + 1
        if end_offset:
            del self.buf[:end_offset]
        return results
