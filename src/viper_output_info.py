#!/usr/bin/env python3
"""Development tool to inspect actual PipeWire Audio Outputs (read-only)."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import tempfile
import time
import uuid
import subprocess
import sys

from viper_output_graph import PipeWireGraph
from viper_output_store import OutputStore
from viper_output_monitor import write_status


def main() -> int:
    p = argparse.ArgumentParser(description='Inspect physical Audio Outputs without fuzzy matching')
    p.add_argument('--json', action='store_true', help='produce machine-readable output')
    actions = p.add_mutually_exclusive_group()
    actions.add_argument('--enable-memory', action='store_true',
                         help='remember settings per physical Audio Output (opt-in)')
    actions.add_argument('--disable-memory', action='store_true',
                         help='stop automatic switching; keep saved output profiles')
    p.add_argument('--remember-discovered', action='store_true',
                   help='add discovered device descriptions to the local inventory (no DSP changes)')
    p.add_argument('--assign-preset', metavar='PRESET_INI', help='assign complete named preset to remembered output')
    p.add_argument('--target', metavar='OUTPUT_KEY', help='64-digit remembered output identity')
    args = p.parse_args()
    try:
        store = OutputStore()
        if args.assign_preset or args.target:
            if not args.assign_preset or not args.target:
                p.error('--assign-preset and --target must be supplied together')
            if args.target not in store.inventory():
                raise ValueError('Unknown remembered Audio Output')
            if not store.enabled():
                raise ValueError('Per-output memory is disabled')
            runtime = Path(os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}'))
            if not runtime.is_dir() or runtime.stat().st_uid != os.getuid():
                raise ValueError('Invalid PipeWire runtime directory')
            token = uuid.uuid4().hex
            request = runtime / f'viper4linux-assign-{token}.request.json'
            result = runtime / f'viper4linux-assign-{token}.result.json'
            write_status(request, {'target': args.target,
                                   'preset': str(Path(args.assign_preset).resolve())})
            try:
                deadline = time.monotonic() + 10.0
                while time.monotonic() < deadline:
                    if result.exists():
                        ack = json.loads(result.read_text())
                        if not ack.get('ok'):
                            raise ValueError(ack.get('error', 'Preset assignment rejected'))
                        print('Preset assigned to Audio Output ' + args.target[:12])
                        return 0
                    time.sleep(0.1)
                raise ValueError('Audio Output monitor did not acknowledge assignment')
            finally:
                request.unlink(missing_ok=True)
                result.unlink(missing_ok=True)
        if args.enable_memory or args.disable_memory:
            store.set_enabled(args.enable_memory)
            print('Remember settings per Audio Output: ' +
                  ('ON' if args.enable_memory else 'OFF'))
            return 0
        proc = subprocess.run(['pw-dump', '--no-colors'], check=True, capture_output=True,
                              text=True, timeout=10)
        graph = PipeWireGraph()
        graph.update(json.loads(proc.stdout))
        active = graph.active()
        outputs = graph.available()
        if args.remember_discovered:
            for output in outputs:
                store.remember(output)
        if args.json:
            from dataclasses import asdict
            print(json.dumps({'active': asdict(active) if active else None,
                              'outputs': [asdict(o) for o in outputs],
                              'memory_enabled': store.enabled(),
                              'remembered_outputs': store.inventory()}, indent=2))
        else:
            print('Remember settings per Audio Output: ' + ('ON' if store.enabled() else 'OFF'))
            if active:
                print(f'Current Audio Output: {active.description} [{active.key[:12]}]')
            else:
                print('Current Audio Output: unresolved (no DSP changes made)')
            for o in outputs:
                print(f'  {"*" if active and o.key == active.key else " "} {o.description}: {o.key[:12]}')
        return 0
    except (OSError, subprocess.SubprocessError, ValueError, json.JSONDecodeError) as e:
        print(f'Audio Output discovery failed: {e}', file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())
