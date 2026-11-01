# Viper4Linux — six-tab native DSP prototype

**Current distribution:** Complete Git history in a tar.gz release, with an Arch VCS PKGBUILD for local system installation. Includes the native ViPERDSP Convolver integration, updated six-tab GUI, CLI, shared-memory controls, PipeWire routing and regression tests. The prior non-native Gain / Headroom stage has been removed end-to-end. This remains an experimental proof of concept.

**Six tabs:** Output · Bass · EQ & Tone · Spatial · Dynamics · Sound Correction. Twenty native-processing cards are currently implemented, including Graphic EQ, Dynamic EQ, Tube Simulator, AnalogX and Convolver. The separate Dynamics tab remains partly planned; ViPER-DDC and Speaker Correction are not yet implemented. See `docs/DEVELOPMENT-STATUS.md` and `docs/CONVOLVER-PROOF-OF-CONCEPT.md`.

**Required external source:** This archive contains the entire ViPER for Linux application source, except for the upstream `ViPERDSP` dependency, which `./install.sh` retrieves at the pinned commit `6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933` over the network. It does not reuse code from any previous local ViPER installation. A distribution-provided PipeWire filter-chain configuration is also copied at build time.

**Compatibility:** The shared-memory protocol has changed to version 22 and uses the v10 production IPC name; the old and new backends must **not** run simultaneously, since they still use the same PipeWire smart-sink names. Existing effect settings and presets are preserved, but old input-gain values are deliberately ignored and removed on GUI startup. Input reaches native ViPERDSP at **100% (unity)**. The master bypass still leaves the native limiter active.

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

1. Fully close JamesDSP and quit any older Viper4Linux GUI (do not run multiple control windows).
2. Run **`viper4linux`** from a terminal (or `./viper4linux` inside the project folder). `install.sh` creates a safe symlink at `~/.local/bin/viper4linux` when possible. The command reuses a matching running backend, or stops the previous Viper4Linux filter-chain client and starts the freshly built backend before opening the GUI.
3. Keep headphone output low initially. With audio playing, run `./verify.sh`. Confirm its audio-block count increments and check the PipeWire routing.
4. Confirm switches work independently of expanding panels, presets survive GUI restarts, tabs restore correctly, and the planned Dynamics tab has no effect controls.
5. Re-test Bluetooth device reconnects and application stream changes before switching over permanently.

The launcher identifies the backend by SHA-256 of the *built* LADSPA module, controller and PipeWire filter-chain config. An older backend that lacks the fingerprint will be restarted. It never stops system PipeWire or unrelated clients; if an old marked client cannot stop, the launcher refuses to create a duplicate. Backend startup logs go to `~/.local/state/viper4linux/backend.log` (or `$XDG_STATE_HOME/viper4linux/backend.log`). The original `./start.sh`, `./gui.sh`, `./stop.sh`, and `./status.sh` remain available for debugging.

The existing `status.sh`, `stop.sh`, `verify.sh` and `viper-ctl` remain available for troubleshooting. Do not delete the prior working directory before live tests pass.

## Notes

- ViPERDSP is third-party code. Review its licensing and transitive rights before redistribution.
- Documentation and historical release notes in `docs/` are preserved for possible GitHub wiki publication. Earlier notes may describe earlier four-tab versions. See `docs/DEVELOPMENT-STATUS.md` for the current source-of-truth about tab names, implemented groups and absent effects.
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
effect heading. See `docs/CONVOLVER-PROOF-OF-CONCEPT.md` for input format limits, live
processing handoff, and troubleshooting.

## GUI color themes

The title bar has a **Theme** selector: **Modern teal** (default) or **Classic Viper**
(cyan-blue accents, with light/dark following the desktop when detectable, and light as fallback). The selection applies immediately
and is saved separately from DSP settings and audio presets. See
[CLASSIC-V4A-THEME.md](docs/CLASSIC-V4A-THEME.md).

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

### Native Crossfeed (ViPER Cure)

The Spatial tab includes ViPERDSP Cure Crossfeed, disabled by default. Its
three native modes are Mild, Medium and Strong. It blends a filtered amount
of left/right audio for headphone spatial realism. The controls are kept
independent of the Convolver cross-channel mix, which is a simpler linear
stereo blend. Preset adjustments while Crossfeed is off are saved without
changing the running DSP path.

### Native Stereo Imager (experimental)

The Spatial tab's final card is a three-band native ViPERDSP Stereo Imager.
Each of the low, mid and high bands has its own width slider (0–200%; 100%
is the normal side-channel gain). The two adjustable crossovers default to
200 Hz and 4 kHz. The effect is **OFF** by default and does not override
Headphone Surround+, Crossfeed, or Field Surround. Widths above 100% can
reduce mono compatibility. Use modest values and compare at matched volume.

Crossover updates are handed to the non-realtime worker. The pinned
ViPERDSP copy is patched to preallocate the native Stereo Imager's 2048-frame
scratch buffers before audio starts, preventing an initial callback allocation
from this effect; this does not certify real-time safety of all other effects.

### Native Dynamic EQ (experimental)

**Dynamic EQ is immediately below the Graphic EQ** in the EQ & Tone tab. It
has an independent enable switch and a compact *static*, read-only, illustrative
target-response graphic. Click a numbered marker to open the separate editor with that band selected; click elsewhere on the graphic to reopen the previous selection.
There you can configure 1–10 bands independently (frequency, peak/low/high
shelf, Q, maximum gain, threshold, attack, release). This is native ViPERDSP's
`ApplyDynamicEq`, with no substitute processing algorithm. The preview is not
an exact measured filter response, nor real-time metering. No UI animation or
new polling timer is used.

Dynamic EQ defaults OFF, with three flat bands at 100/1000/8000 Hz. Settings
are saved across GUI restarts and with Save/Restore preset. Frequency and other
filter updates are handled by the non-RT worker. Edits no longer deliberately trigger a global transition to dry audio.

### Application labels and compatibility

The EQ tab uses Qt's escaped `&&` mnemonic syntax to display a literal
**EQ & Tone** without an unwanted underscore. Saved tab selection continues
to use the human-readable label. The tab formerly named `IR & Correction` is
**Sound Correction**.

The IPC layout has changed (v20). Stop the previous backend, then start the new
backend built from this same master. Native Qt6 compilation and PipeWire playback
must be checked on Arch; mock tests validate control forwarding only and do not
prove real ViPERDSP frequency response or flawless live playback.

## Arch Linux system package (development)

See [Arch packaging instructions](docs/ARCH-PACKAGING.md). The release tarball
contains a real Git checkout and a `packaging/arch/PKGBUILD` that builds the
current commit as `viper4linux-git` and installs the application under `/usr`.
Run `cd packaging/arch && makepkg -si` from the extracted project. The upstream
ViPERDSP library is fetched at its pinned commit during package preparation.
The first system-package build should be tested separately from source checkouts.
