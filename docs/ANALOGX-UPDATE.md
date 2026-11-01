# Viper4Linux — AnalogX and Tube Label Update

## Native effect

- EQ & Tone has independent **AnalogX**, using upstream `ViPER::ApplyAnalogX` without a substitute DSP algorithm.
- Native modes are Mild (0), Medium (1), Strong (2). Off by default.
- Changes in mode or enabled state reset upstream AnalogX filters. The existing non-real-time settings worker applies changes while the callback uses the safe dry handoff. No DSP modification is made inside the PipeWire callback for this effect.
- **Upstream behavior:** `AnalogX::Process()` briefly zeros processed output for up to one quarter second after a filter reset. A short transient or audio dip when enabling or switching modes is possible, regardless of this Linux adapter.
- Settings are retained across GUI restart, included in the global Save/Restore preset, and reset to Off/Mild by Reset.

## Tube dropdown wording

- `12AX7 (High Gain)`
- `12AU7 (Low Gain)`
- `6N1P (Medium Gain)`

Descriptions are broad references to tube families, not claims about measurable output gain of ViPERDSP at each setting. The model indices, Drive and Mix defaults and processing are unchanged.

## Compatibility and validation

- Control protocol changes **18 to 19**. Restart the existing filter-chain backend after rebuilding; the previous backend is not protocol-compatible.
- Includes a seventeenth monochrome effect glyph for AnalogX.
- Tests: `./tests/check-mock.sh`, `python3 tests/check-ui-contract.py`, `python3 tests/check-theme-contract.py`. The mock tests verify the native dispatch call, all three settings, disable, bypass, and controls isolation; they do not reproduce upstream audio coloration.
- Native Qt6 build and real PipeWire playback must be tested on Arch.
