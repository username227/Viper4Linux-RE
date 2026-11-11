#!/usr/bin/env python3
"""Fix ViPERDSP FET Compressor's fractional-sample peak detector.

Unqualified abs(float) may resolve to abs(int) under GCC, reducing every
sample with magnitude below 1.0 to zero and disabling compressor reduction.
Only touch CMake's private prepared copy; fail closed on unknown upstream.
"""
import os
from pathlib import Path

root = os.environ.get("VIPERDSP_SOURCE_DIR")
if not root:
    raise SystemExit("ERROR: VIPERDSP_SOURCE_DIR is required for private FET fix.")
root = Path(root).resolve()
if root.name != "_viperdsp-prepared":
    raise SystemExit("ERROR: FET fix may only modify CMake's _viperdsp-prepared copy.")

path = root / "viper/effects/FETCompressor.cpp"
source = path.read_text()
original = (
    "const double in_l = abs(samples[i]);\n"
    "        const double in_r = abs(samples[i + 1]);"
)
corrected = (
    "const double in_l = std::abs(samples[i]);\n"
    "        const double in_r = std::abs(samples[i + 1]);"
)
if source.count(original) == 1 and corrected not in source:
    path.write_text(source.replace(original, corrected, 1))
    print("Applied verified FET fractional-sample abs fix to private dependency.")
elif source.count(corrected) == 1 and original not in source:
    print("Verified FET fractional-sample abs fix already present.")
else:
    raise SystemExit("ERROR: Unexpected ViPERDSP FET sidechain; refusing to patch.")
