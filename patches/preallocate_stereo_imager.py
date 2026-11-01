#!/usr/bin/env python3
"""Preallocate native StereoImager's per-band scratch at LADSPA's block cap.

The pinned native implementation otherwise resizes its three buffers on the
first active PipeWire callback. The adapter deliberately caps chunks at 2048
frames, so 4096 interleaved samples per band are sufficient. Fail closed on
unexpected upstream revisions. This modifies only the private dependency copy.
"""
from pathlib import Path
p = Path(__file__).resolve().parent.parent / 'third_party/ViPERDSP/viper/effects/StereoImager.cpp'
s = p.read_text()
old = '''StereoImager::StereoImager() :
    enable_(false),
    sampling_rate_(VIPER_DEFAULT_SAMPLING_RATE) {
    band_widths_[0] = 1.0f;'''
new = '''StereoImager::StereoImager() :
    enable_(false),
    sampling_rate_(VIPER_DEFAULT_SAMPLING_RATE) {
    // The ViPER for Linux LADSPA bridge processes at most 2048 stereo frames.
    // Preallocate on construction, outside the audio callback, so first
    // activation cannot resize the band scratch buffers on PipeWire's thread.
    for (auto& buffer : band_buffers_) buffer.resize(4096);
    band_widths_[0] = 1.0f;'''
if old in s:
    p.write_text(s.replace(old, new, 1))
    print('Applied verified Stereo Imager scratch preallocation to private dependency.')
elif new in s:
    print('Verified Stereo Imager scratch preallocation already present.')
else:
    raise SystemExit('ERROR: Unexpected native Stereo Imager constructor; refusing to patch.')
