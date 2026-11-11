#!/usr/bin/env python3
"""Normalize pinned ViPERDSP Harmonic coefficient magnitude on Linux.

The original unqualified abs(float) may bind the integer overload with GCC,
truncating fractional harmonic weights and shortening startup warm-up. Apply
this proven one-line correction only to CMake's prepared dependency copy;
never edit the pinned upstream submodule. Refuse changed upstream context.
"""
import os
from pathlib import Path

source_dir = os.environ.get('VIPERDSP_SOURCE_DIR')
if not source_dir:
    raise SystemExit('ERROR: VIPERDSP_SOURCE_DIR is required for the private Harmonic fix.')
source_dir = Path(source_dir).resolve()
if source_dir.name != '_viperdsp-prepared':
    raise SystemExit('ERROR: Harmonic fix may only modify CMake\'s _viperdsp-prepared copy.')

path = source_dir / 'viper/utils/Harmonic.cpp'
source = path.read_text()
original = 'const float abs_coeff = abs(coeffs[i]);'
corrected = 'const float abs_coeff = std::abs(coeffs[i]);'
if source.count(original) == 1 and corrected not in source:
    path.write_text(source.replace(original, corrected, 1))
    print('Applied verified Harmonic floating-point abs correction to private dependency.')
elif source.count(corrected) == 1 and original not in source:
    print('Verified Harmonic floating-point abs correction already present.')
else:
    raise SystemExit('ERROR: Unexpected ViPERDSP Harmonic coefficient calculation; refusing to patch.')
