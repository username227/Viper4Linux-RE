# Output tab milestone — control protocol v11

Based on the seven-effect teal-tab source release; no reimplementation of Bass/Clarity/Spectrum/Surround.

* Master Limiter: permanent disabled-on switch; threshold in 0.001 linear steps (0.3..1), output gain 1..200%, balance -100..100; defaults: -6.02 dBFS, 100% gain, centered.
* AGC: independently enabled, off by default. Defaults strength 1.0, max gain 3.0, threshold 0.9.
* LUFS: independently enabled, off by default. Defaults -14 LUFS, 6 dB max adjustment, normal speed.
* Native limiter configured on both wet and bypass engines; hard clipping ceiling removed.
* Upstream pinned LUFS speed-setting bug fixed via private source change, with strict source-match guard.
* Existing effects and UI tab style unchanged; Output is a new third tab.
* All new controls saved in existing QSettings and manual presets; CLI and startup loader updated.
* Mock tests validate value forwarding and independent enable/bypass. They do NOT measure audible quality, true peak, or realtime performance.
* All existing source files for seven original effects retained. Regression suite must pass before install.
