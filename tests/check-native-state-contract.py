#!/usr/bin/env python3
"""Reject partial per-output snapshots when the DSP IPC layout changes."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
ipc = (root / 'src/viper-control.h').read_text()
helper = (root / 'src/viper-output-state.cpp').read_text()
controls = ipc.split('struct alignas(64) Controls {', 1)[1].split('static_assert(', 1)[0]
controls = re.sub(r'struct (?:MultibandControls|DynamicEqBandControl) \{.*?\n    \};', '',
                  controls, flags=re.S)
all_scalar = set(re.findall(r'std::atomic<std::int32_t>\s+([A-Za-z_]\w*)', controls))
transient = {'owner_pid', 'active', 'ddc_ready', 'ddc_status',
             'convolver_ready', 'convolver_status', 'sample_rate_hz',
             'mb_crossovers_hz', 'eq_levels_hundredths'}
required = all_scalar - transient
body = helper.split('static const Field<Controls> fields[] = {', 1)[1].split('};', 1)[0]
included = set(re.findall(r'&Controls::([A-Za-z_]\w*)', body))
assert included == required, f'Missing/extra native Audio Output fields: {required ^ included}'
assert len(required) >= 85, f'Unexpected DSP control coverage: {len(required)}'
for member, size in [('eq_levels_hundredths', 31), ('mb_crossovers_hz', 4),
                     ('mb_bands', 5), ('deq_bands', 10)]:
    assert member in ipc and member in helper
assert 'control_version' in helper and 'kVersion' in helper
assert 'validateState(doc.object())' in helper
assert 'settings.sync()' in helper
assert 'convolver_request_serial' in helper and 'ddc_request_serial' in helper
assert 'backend_pid' in helper
assert 'current()' in helper
print(f'PASS: native output snapshot covers all {len(required)} DSP scalar controls and four arrays')
