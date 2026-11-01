#!/usr/bin/env python3
"""Preallocate native MultibandCompressor's band buffers off PipeWire's RT thread.

The LADSPA bridge caps processing at 2048 stereo frames. Apply only to the
verified pinned upstream constructor; fail if upstream changes unexpectedly.
"""
from pathlib import Path
p = Path(__file__).resolve().parent.parent / 'third_party/ViPERDSP/viper/effects/MultibandCompressor.cpp'
s = p.read_text()
old = '''    crossover_freqs_[0] = kDefault3BandFreqs[0];
'''
new = '''    // PipeWire LADSPA bridge processes at most 2048 frames per chunk.
    // Preallocate 4096 interleaved floats for all five bands before audio.
    for (auto& buffer : band_buffers_) buffer.resize(4096);
    crossover_freqs_[0] = kDefault3BandFreqs[0];
'''
if new in s:
    print('Verified Multiband Compressor scratch preallocation already present.')
elif old in s:
    p.write_text(s.replace(old, new, 1))
    print('Applied verified Multiband Compressor scratch preallocation to private dependency.')
else:
    raise SystemExit('ERROR: Unexpected Multiband Compressor constructor; refusing to patch.')
