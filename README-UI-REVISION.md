> **Historical snapshot only:** This earlier revision describes a Gain / Headroom card that has now been removed from both GUI and audio backend. See README.md for the current design.

# ViPER for Linux — Android-inspired collapsible effect cards

This is a **complete standalone source tree**, rebuilt from the seven-effect
source archive supplied by the user. It is **not** the older five-effect build.

## Exact scope

The two original tabs remain **Enhancements** and **Surround**. Every original
processor remains in the source:

- Enhancements: ViPER Bass (Natural, Pure Bass+, Subwoofer), ViPER Clarity
  (Natural, Ozone, XHiFi), Spectrum Extension, and the separate Input Level
  (headroom) control card.
- Surround: Headphone Surround+, Differential Surround, **Field Surround**,
  and **Reverberation** (room size, stereo width, damping, wet, dry).
- Master processing bypass, presets, setting persistence, CLI, PipeWire
  routing, shared-memory version **10**, and GUI reconnection are retained.

The only changed original file is `src/viper-gui.cpp`. All DSP, header,
PipeWire, CLI, build scripts and DSP regression tests are byte-for-byte
unchanged from your supplied seven-effect source ZIP.

## Layout and behavior

Each effect appears in a rounded card, with a fixed header containing its
icon, name/expand arrow, and on/off slider-style switch. Click the effect
name/arrow to expand or collapse its **settings only**. Click the switch to
turn its **audio processing** on or off. These actions are independent:

- You can collapse an enabled effect and it **keeps processing**.
- You can expand a disabled effect to adjust its settings before enabling it.
- All four combinations (open/closed × on/off) work.
- The collapse preference is stored in QSettings (`ui/expanded/<effect>`),
  separately from existing DSP settings and the active tab.
- Slider label and current value sit above the slider, as in the reference
  ViPER4Android screenshots. The body scrolls inside each tab.
- The master bypass has a separate always-visible switch above both tabs.

No Master Limiter, LUFS, Playback Gain Control, or other DSP features from the
reference screenshot are claimed here: the screenshot is the **layout
reference**, not authorization to add unimplemented DSP effects.

## Safe fresh install / test on Arch

**Keep the existing working project unchanged.** Extract this ZIP to a
*separate* desktop directory named `viper-for-linux-seven-effects-ui`, then:

```bash
cd ~/Desktop/viper-for-linux-seven-effects-ui
./tests/check-ui-contract.py
./tests/check-mock.sh
./install.sh
```

`./install.sh` checks dependencies, fetches the pinned upstream ViPERDSP
revision and builds a new plugin, CLI and Qt6 GUI in this directory. It needs
network access for the upstream clone. It does **not** overwrite, read or
patch `~/Desktop/viper-for-linux`.

For a real-world test, close JamesDSP, stop the old ViPER backend using the
old directory's `./stop.sh`, then use **this** directory's `./start.sh` and
`./gui.sh`. Test with Rhythmbox playing. Confirm Field Surround, Reverb, tabs,
presets and processing status; then enable Bass and collapse its settings:
Bass must keep processing. Reopen and verify its prior values.

Do not run both ViPER backends at once. To return to the previous version,
stop this new backend and launch the backend/GUI in your original working
directory. Do not delete that working directory until all audio tests pass.

## Verification limitations

The included seven-effect mock regression suite passes in the build
packaging environment. The Qt6 GUI could **not** be compiled in that
environment because Qt6 development headers are unavailable; the actual
Qt6 compilation and visual/audio testing must be done on your Arch machine.
