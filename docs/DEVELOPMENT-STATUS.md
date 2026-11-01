# ViPER for Linux — development handoff / status

## Scope and architecture

This is a **proof of concept**, not a finished release. The app consists of a native Qt6 Widgets GUI (`src/viper-gui.cpp`), shared-memory control contract (`src/viper-control.h`), CLI (`src/viper-ctl.cpp`), and LADSPA processor (`src/viper-ladspa.cpp`) hosted by a PipeWire filter-chain sink. The DSP library comes from a **pinned external ViPERDSP revision**, retrieved by `install.sh`, not bundled. PipeWire smart routing is configured in `routing-config/` and controlled by `start.sh`, `stop.sh`, `status.sh`, and `verify.sh`.

The six-tab GUI retains the original independent effect switches, collapsible panels and saved-effect keys. This revision also integrates native Convolver loading and **removes adapter input attenuation completely**. The shared-memory protocol has changed (version 22, production IPC channel v10), so all three native binaries must come from this source tree. Existing effect presets remain supported, but legacy `input_percent` values are ignored and cleaned up by the GUI.

## Six-tab effect matrix

| Tab | Existing functional controls | Explicitly planned / absent |
| --- | --- | --- |
| Output | Master Limiter (always on), Playback Gain Control (AGC), LUFS Targeting | — |
| Bass | ViPER Bass, ViPER Bass Mono, Psychoacoustic Bass, Dynamic System | — |
| EQ & Tone | Graphic Equalizer (10/15/25/31 bands), native Dynamic EQ (1–10 bands), ViPER Clarity, Spectrum Extension, Tube Simulator, AnalogX | — |
| Spatial | Headphone Surround+, Differential Surround, Field Surround, Reverberation, native Cure / Crossfeed, native Stereo Imager | — |
| Dynamics | **No functional effects yet** | FET Compressor, Multiband Compressor |
| IR & Correction | Native Convolver (WAV-encoded `.irs` / `.wav`, live kernel switching, IRS library/import) | ViPER-DDC (Speaker Correction implemented; built-in filters, off by default) |

**Count (current):** 20 processing cards: four Bass, six EQ & Tone, six Spatial, three Output (Limiter, AGC, LUFS), and one Convolver. The limiter is always present; other processors can be enabled independently. Gain / Headroom was not a native ViPER effect and is removed in both GUI and backend.

## Before working on new effects

1. Keep `src/viper-control.h` consistent across GUI, CLI, and LADSPA backend; when the ABI changes, bump the protocol version and IPC namespace. Do not mix protocol versions between processes.
2. Add and test one effect at a time. New switches must be independent of card expansion and default OFF until safely tested. Check whether the native ViPERDSP API at the pinned commit actually exposes the expected processor before wiring a control.
3. Extend `tests/mock/` and `tests/check-mock.sh` to verify parameter mappings, bypass, state and sample-rate behavior. Mock tests cannot prove audible fidelity, real-time safety or Bluetooth stability.
4. On Arch, run `./install.sh` in an empty extracted folder, then `./start.sh`, `./gui.sh`, and `./verify.sh` with low output volume. Test a Rhythmbox stream change, a Bluetooth reconnect, and fresh GUI/backend launches.
5. Maintain exact names/semantics of saved settings to preserve user presets. Track any migration explicitly. An empty planned tab must not create a fake effect or a disabled switch that looks implemented.
6. Recheck licensing of ViPERDSP and required components before publishing as a redistributable package.

## Known limitations

- Qt6 is required for native GUI compilation; local packaging tests may not have it.
- The archive does not include the upstream ViPERDSP checkout or binary. First build needs git/network and local PipeWire/Qt/LADSPA development packages.
- The original effects were inherited from the Bass-tab master; native Convolver and unity-input changes have not been tested live on the user's Arch desktop.
- Current smart routing is PipeWire-specific; genuine PulseAudio server support is not implemented.
- The local mock suite checks the control boundary only, not real-time allocations, playback latency, codec rate or subjective sound.
- The source tree has historical release notes with superseded layouts: do not use them as the current implementation specification.

## Mock test isolation (November 1, 2026)

`tests/check-mock.sh` compiles its DSP mock and executable with
`VIPER_MOCK_TEST_IPC` so they communicate via a per-user, per-process
shared-memory object. A running production ViPER backend can own its normal
shared-memory object without blocking regression tests. Production binaries
are compiled **without** that macro and use the v9 IPC name and v16 layout. This solves the previously observed `Cannot create clean control
channel` / `strength_centi == 500` failure when the tests ran while ViPER
was already active.


## Convolver development stage (2026-11-01)
Native Convolver IR loader added to IR & Correction, disabled by default. Reader supports standard mono/stereo RIFF PCM16/24/32 and float32 impulse responses with live background loading and a nonblocking audio-thread handoff. Non-native input trim removed end-to-end: unity input, no saved or hidden attenuation, removed LADSPA control and CLI command. IPC layout/name bumped to v8 for safe isolation. See CONVOLVER-PROOF-OF-CONCEPT.md for validation and limits.

## IRS library / Help / live switching update (November 1, 2026)

- `~/.local/share/viper4linux/IRS/` is the normal library location, honoring `$XDG_DATA_HOME`. The GUI scans this directory, offers a dropdown and imports external IRS files (no overwriting).
- Selecting, clearing, restoring a preset or clicking Refresh / Reload sends a versioned request through IPC after the settings file is synced. The loader thread decodes and constructs kernels off PipeWire's audio callback. The callback uses a nonblocking mutex attempt; during the brief load it uses the dry path.
- A small `?` Help dialog collects concise effect explanations. Inline effect descriptions are removed.
- IPC version 15, named v8, deliberately prevents accidental connection to the old backend. Existing effects, presets, native output limiter and unity input remain unchanged.
- Mock tests and GUI contract checks passed in the isolated test environment. Real Arch Qt6/PipeWire builds and real-time playback must still be verified.

## Native EQ integration (2026-11-01)

- 10, 15, 25 and 31-band native ViPERDSP `ApplyEqualizer` using the upstream
  minimum-phase IIR implementation; the EQ defaults OFF. Fader values use
  tenth-dB IPC integers (-120..+120) and 0.5-dB GUI steps.
- Only one selected fader layout visible: 31 Qt widgets created once,
  unused columns hidden and inner widget width resized. The EQ region scrolls
  horizontally without increasing the main window minimum width.
- Each band-count mode retains its own settings and manual preset values.
- Coefficients are recomputed by the existing non-RT loader worker, with the
  audio callback falling back to the dry path if the wet mutex is held.
  EQ mode changes ramp the wet path down before filter reconstruction.
- IPC layout v16, name v9: install all binaries from one source master.
- Mock tests and GUI source-contract tests pass; real QT6 compilation and
  native audio playback still require testing on Arch.

## Compact EQ overview and named presets (2026-11-01)

- Keep Bands and Preset selectors in one width-responsive row in EQ & Tone.
- Read-only log-frequency smooth curve summarizes active bass-to-treble gains.
  Clicking the graphic opens a nonmodal editor; closing hides it without
  destroying its reusable 31-slider bank. Only selected band count is visible.
- Narrow fader columns (36 px, 3 px spacing), horizontal scrolling within editor.
- Built-in Flat/Classical/Rock/Pop/Jazz/Vocal/Bass Boost/Treble Boost curves.
  They are examples rather than official upstream ViPERDSP presets.
- Save/Delete named user EQ curves in existing QSettings and preserve the
  selected preset on startup. Band switching interpolates log-frequency.
- Independent C++17 preset regression test and GUI source checks added.
  Qt6 compilation and live audio must still be verified on Arch.


## Development packaging

Historical Markdown notes live in `docs/`; `README.md` is the sole top-level
Markdown introduction. Development releases display `0.1.0-<short commit>`
in the About dialog, and the complete master carries the `.git` history.

## Stereo Imager addition (November 1, 2026)

- Native `ApplyStereoImager` controls added at the bottom of Spatial: independent
  enable and low, mid and high width (0–200%), 80–400 Hz low crossover,
  2000–8000 Hz high crossover. Default OFF, width=100%, crossovers 200/4000 Hz.
- Slider settings survive GUI restarts and one-slot presets. New control requests
  are applied by the off-RT worker under the wet-engine mutex; audio try-locks
  and never blocks waiting for crossover coefficients.
- Private, pinned ViPERDSP source has its native three-band scratch buffers
  preallocated at the application's 2048-frame block cap before audio starts.
- Protocol version bumped to 22. Mock audio, control/worker forwarding, GUI
  source-contract and launcher/packaging checks pass. Actual Qt6 build and
  listening tests remain to be run on the user's Arch Linux system.
