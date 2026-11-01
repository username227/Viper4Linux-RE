# Multiband Compressor (native ViPERDSP)

The Dynamics tab offers 3-band and 5-band compression, **off by default**.
Each band can be bypassed independently and has threshold (-48..0 dBFS),
ratio (1:1..10:1), knee (0..12 dB), makeup gain (0..24 dB),
attack (1..100 ms) and release (5..500 ms).

Native ViPERDSP uses four crossover slots; two are used for 3 bands.
Defaults: 3 bands = 200/4000 Hz; 5 bands = 120/500/4000/8000 Hz.
Crossovers must be increasing and are clamped to remain below Nyquist.

Each band is a native ViPERDSP FET compressor, not a replacement DSP.
Band/crossover updates occur on the worker outside PipeWire's audio callback.

Use modest compression, avoid excessive makeup gain and compare level-matched.
Combining FET, multiband, AGC and LUFS can cause unexpected dynamics.

## Compact editor

The horizontal selector runs from bass to treble. Select a frequency band to edit its six controls; only that band’s settings are visible. The active selector is remembered between GUI sessions. Crossover adjustments are grouped under a collapsed “Edit crossover frequencies” panel. This is a GUI-only change: all bands continue processing and retain their independent settings.
