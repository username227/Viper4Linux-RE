#!/usr/bin/env python3
"""Apply one verified fix to pinned ViPERDSP's LUFS speed setter.

No upstream rebase; refuse unknown source instead of fuzzy substitution.
"""
from pathlib import Path
p = Path(__file__).resolve().parent.parent/'third_party/ViPERDSP/viper/effects/LUFSTargeting.cpp'
s = p.read_text()
old = '''void LUFSTargeting::SetSpeed(const int value) {
    if (value < 0) speed_ = 0;
    if (value > 2) speed_ = 2;
    UpdateSmoothingCoeffs();
}'''
new = '''void LUFSTargeting::SetSpeed(const int value) {
    speed_ = value < 0 ? 0 : (value > 2 ? 2 : value);
    UpdateSmoothingCoeffs();
}'''
if old in s:
    p.write_text(s.replace(old, new, 1))
    print('Applied verified LUFS speed control correction to private dependency copy.')
elif new in s:
    print('Verified LUFS speed correction already present.')
else:
    raise SystemExit('ERROR: Unexpected LUFS implementation; cannot safely patch ViPERDSP.')
