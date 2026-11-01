# Viper4Linux — six-tab native DSP prototype

**Current distribution:** Complete application source tree, not an incremental patch. Includes the native ViPERDSP Convolver integration, updated six-tab GUI, CLI, shared-memory controls, PipeWire routing and regression tests. The prior non-native Gain / Headroom stage has been removed end-to-end. This remains an experimental proof of concept.

**Six tabs:** Output · Bass · EQ & Tone · Spatial · Dynamics · Sound Correction. Eighteen native-processing cards are currently implemented, including Graphic EQ, Dynamic EQ, Tube Simulator, AnalogX and Convolver. The separate Dynamics tab remains partly planned; ViPER-DDC and Speaker Correction are not yet implemented. See `DEVELOPMENT-STATUS.md` and `CONVOLVER-PROOF-OF-CONCEPT.md`.

**Required external source:** This archive contains the entire ViPER for Linux application source, except for the upstream `ViPERDSP` dependency, which `./install.sh` retrieves at the pinned commit `6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933` over the network. It does not reuse code from any previous local ViPER installation. A distribution-provided PipeWire filter-chain configuration is also copied at build time.

**Compatibility:** The shared-memory protocol has changed to version 20 and uses the v10 production IPC name; the old and new backends must **not** run simultaneously, since they still use the same PipeWire smart-sink names. Existing effect settings and presets are preserved, but old input-gain values are deliberately ignored and removed on GUI startup. Input reaches native ViPERDSP at **100% (unity)**. The master bypass still leaves the native limiter active.

## Fresh Arch Linux build

Keep your existing working build. Extract into a **new, separate folder** and use:

```bash
sudo pacman -S --needed base-devel cmake git pkgconf ladspa qt6-base pipewire
cd /path/to/viper-for-linux-live-irs
./tests/check-ui-contract.py
./tests/check-mock.sh
./install.sh
```

`install.sh` runs the mock DSP tests, fetches the pinned upstream DSP library, configures from a clean build directory, builds all three binaries, and checks the exported LADSPA descriptor. It does **not** overwrite your old project. The mock suite is **not** a real Qt compilation test or an acoustic fidelity test.

## Run and test

1. Stop your old ViPER backend with its own `./stop.sh` and quit the old GUI. Do not run multiple copies simultaneously; they share PipeWire names and IPC paths. Fully close JamesDSP.
2. In the new folder, run `./start.sh`. In another terminal, run `./gui.sh`.
3. Keep headphone output low initially. With audio playing, run `./verify.sh`. Confirm its audio-block count increments and check the PipeWire routing.
4. Confirm switches work independently of expanding panels, presets survive GUI restarts, tabs restore correctly, and the planned Dynamics tab has no effect controls.
5. Re-test Bluetooth device reconnects and application stream changes before switching over permanently.

The existing `status.sh`, `stop.sh`, `verify.sh` and `viper-ctl` remain available for troubleshooting. Do not delete the prior working directory before live tests pass.

## Notes

- ViPERDSP is third-party code. Review its licensing and transitive rights before redistribution.
- All other release notes in this tree are historical and may describe earlier four-tab versions. See `DEVELOPMENT-STATUS.md` for the current source-of-truth about tab names, implemented groups and absent effects.
- **Turn your headphone volume down before first playback.** Removing the old 25% input scaling can increase the amplitude reaching ViPER fourfold (approximately +12 dB) before effects and limiting.
- AGC and LUFS can interact; enable one at a time while testing. The output limiter runs even when master effects are bypassed. Do not assume a software limiter provides a safe listening-level guarantee.

### IR library and live loading

The Convolver in **Sound Correction** has a library dropdown. The normal storage path is
`~/.local/share/viper4linux/IRS/` (respects `XDG_DATA_HOME`). **Import…** accepts multiple external
`.irs`/`.wav` files into this directory, using a unique name rather than overwriting
an existing file. Select from the dropdown to load a new kernel **without restarting
PipeWire or the ViPER backend**. **Refresh / Reload** also rescans the directory and
reloads the current choice. The list updates on filesystem changes only when the dropdown is closed, and remains stable while open. Different-rate IRs are resampled before live kernel replacement. **None** unloads the kernel.

A `?` button opens the effect descriptions instead of placing prose under every
effect heading. See `CONVOLVER-PROOF-OF-CONCEPT.md` for input format limits, live
processing handoff, and troubleshooting.

## GUI color themes

The title bar has a **Theme** selector: **Modern teal** (default) or **Classic Viper**
(cyan-blue accents, with light/dark following the desktop when detectable, and light as fallback). The selection applies immediately
and is saved separately from DSP settings and audio presets. See
[CLASSIC-V4A-THEME.md](CLASSIC-V4A-THEME.md).

### Native graphic equalizer (experimental)

The EQ & Tone tab uses ViPERDSP's native minimum-phase IIR graphic
equalizer with 10, 15, 25, or 31 bands. Two side-by-side responsive selectors
(Bands and Preset) stay visible above a **read-only smooth response curve**.
The compact curve fits the window, follows a logarithmic bass-to-treble
frequency axis, and is the *only* control that opens the detailed EQ editor.
The editor contains a single persistent bank of 31 Qt vertical faders, with
only the selected 10/15/25/31 displayed. Faders are closely spaced and the
editor scrolls horizontally as needed, instead of enlarging the main window.

The Graphic EQ contains the **12 original ViPER4Android presets** (including
Acoustic), with exact 10/15/25/31-band values from upstream, plus Custom.
Save preset… stores a custom named curve in QSettings; Delete preset removes
only a user-defined curve. Manual fader edits switch the selection to Custom.
Separate 10/15/25/31 manual layouts are retained. Settings are preserved in
0.01 dB steps; EQ defaults OFF. Band-count coefficient updates run on a
non-audio worker, and the audio callback does not wait on the wet mutex.

### Native Dynamic EQ (experimental)

**Dynamic EQ is immediately below the Graphic EQ** in the EQ & Tone tab. It
has an independent enable switch and a compact *static*, read-only, illustrative
target-response graphic. Click this graphic to open the separate editor.
There you can configure 1–10 bands independently (frequency, peak/low/high
shelf, Q, maximum gain, threshold, attack, release). This is native ViPERDSP's
`ApplyDynamicEq`, with no substitute processing algorithm. The preview is not
an exact measured filter response, nor real-time metering. No UI animation or
new polling timer is used.

Dynamic EQ defaults OFF, with three flat bands at 100/1000/8000 Hz. Settings
are saved across GUI restarts and with Save/Restore preset. Frequency and other
filter updates are handled by the non-RT worker. A brief transition toward dry
processing can occur after modifying the filters.

### Application labels and compatibility

The EQ tab uses Qt's escaped `&&` mnemonic syntax to display a literal
**EQ & Tone** without an unwanted underscore. Saved tab selection continues
to use the human-readable label. The tab formerly named `IR & Correction` is
**Sound Correction**.

The IPC layout has changed (v20). Stop the previous backend, then start the new
backend built from this same master. Native Qt6 compilation and PipeWire playback
must be checked on Arch; mock tests validate control forwarding only and do not
prove real ViPERDSP frequency response or flawless live playback.
