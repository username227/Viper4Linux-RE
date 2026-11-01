# Viper4Linux — Native Tube Simulator and consistent effect icons

- EQ & Tone: ViPERDSP native TubeSimulator effect with 12AX7, 12AU7, 6N1P models, Drive (0–100%), and Wet/Dry Mix (0–100%). Disabled by default. Settings auto-save and participate in the Save/Restore preset controls.
- Model and drive changes are applied by the existing non-realtime worker because the upstream native implementation resets filters when they change. Global bypass remains independent. No alternative tube DSP implementation was introduced.
- Six tab labels remain Output, Bass, EQ & Tone, Spatial, Dynamics, Sound Correction; underscores and the old IR & Correction label are removed. The main window title and heading are Viper4Linux. The top subtitle and bottom explanatory sentences were removed.
- All 17 effect cards now use font-independent, monochrome QPainter vector glyphs rather than a mixture of Unicode symbols and a generic dot. The line artwork has a consistent stroke, 26-pixel slot, and the active GUI theme accent. Glyphs are decorative and never modify DSP state.
- Icon meanings: Bass (two bass waves); Bass Mono (centered bass wave); Psychoacoustic Bass (ear plus wave); Dynamic System (wave with lightning); Graphic EQ (faders); Clarity (sparkle); Spectrum Extension (increasing frequency bars); Tube Simulator (vacuum tube); AnalogX (analog tracks); Headphone Surround+ (headphones); Differential Surround (opposing arrows); Field Surround (radiating center); Reverberation (decaying echoes); Convolver (impulse and response); Master Limiter (signal ceiling); AGC (gain meter); LUFS (loudness target).
- The control shared-memory protocol is updated from version 18 to 19. Install and restart the running backend at least once when moving from the prior EQ master.

## Verification

Run `./tests/check-mock.sh`, `python tests/check-ui-contract.py`, `python tests/check-theme-contract.py`, and `python tests/check-native-effect-routing.py`. The mock backend deliberately synthesizes tube output only to verify control delivery; it does not mimic the actual ViPERDSP tube transfer function. Native Qt6 compilation, icon rendering, and live listening on Arch remain to be verified.

## AnalogX follow-up

- Added native `ViPER::ApplyAnalogX` (Off by default), modes Mild, Medium, Strong. Filter reset is handled by the non-RT worker; the audio callback uses the existing dry-handoff path.
- Tube model captions now read `12AX7 (High Gain)`, `12AU7 (Low Gain)`, `6N1P (Medium Gain)`; underlying model indices remain unchanged.
- AnalogX is included in startup settings, live shared-memory controls, Save/Restore, and the reset defaults. It has its own monochrome vector glyph.
- Note: upstream AnalogX briefly zeroes its output on enable/reset while warming filters. A short audible gap is possible and is native behavior.
