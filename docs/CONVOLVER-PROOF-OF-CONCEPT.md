# ViPER for Linux — Convolver proof of concept

This **complete source master** adds the pinned ViPERDSP native Convolver to the existing PipeWire LADSPA adapter. All preexisting DSP processors, CLI, preset handling, six-tab GUI and routing scripts remain present.

## How to test (Arch)

1. Extract to a new directory; leave the prior working installation unchanged.
2. Run `./install.sh` (mock regression + native build); ensure it completes.
3. Stop the prior ViPER instance with its own `./stop.sh` before starting this one. The new source uses an independent v8 IPC namespace, but both instances must not route sound at once.
4. Start this build with `./start.sh`, then `./gui.sh`.
5. Use `./build/viper-ctl status` to inspect IR status (`READY`, `NONE`, `RESAMPLE ERROR` or `INVALID`), and `./build/viper-ctl convolver-on` / `convolver-off` for command-line testing. In **IR & Correction > Convolver**, select a WAV-encoded `.irs` or `.wav` file (the backend converts its rate to the active PipeWire rate automatically). Selections, imports, clearing and cross-channel mix changes apply **without restarting the backend**. Only the initial installation of this revised backend requires replacing the old running instance.
6. Compare with Convolver off/on and master bypass; try one IR at a time and start at a safe volume.

## Supported and intentionally not supported

- Mono/stereo RIFF/WAVE files containing 16-, 24-, or 32-bit PCM or IEEE 32-bit float.
- `*.irs` files are accepted **when the contents really are RIFF/WAVE**. Files labeled IRS can also contain custom/unknown data and are not necessarily supported.
- File size <= 8 MiB; IR duration <= 5 seconds; at least 16 samples. Invalid files fail without enabling Convolver.
- The loader uses a bounded windowed-sinc resampler off the audio thread to match the active PipeWire sample rate. The source file is not modified, and its frequency-response gain is preserved approximately.
- Native kernels load at startup or from a background worker. The worker decodes/validates files, briefly ramps toward the bypass path, then locks the wet DSP for kernel construction. The audio callback **never waits** on that lock: if necessary, it runs the dry DSP path while the kernel changes. After loading, the wet mix ramps back in. This is a proof-of-concept technique; audible transitions, very long kernels, and sustained Bluetooth load still need real-device tests.
- The IRS library is `${XDG_DATA_HOME:-~/.local/share}/viper4linux/IRS/`. Import copies files into the library without overwriting existing files; name collisions receive numbered names. The GUI uses filesystem change notifications to refresh when the dropdown is closed. It never rebuilds the list while the popup is open; a change made during selection appears the next time the popup opens. **Import…** accepts multiple files and activates the first imported IR. Manual **Refresh / Reload** remains available. The `?` Help dialog holds descriptions formerly shown beneath effect headings.
- There is no separate Convolver wet/dry slider, no 4-channel true stereo convolution, and no verified Bluetooth-latency/CPU profiling yet. A bad file or failed resampling is reported without discarding the previously working kernel.
- **The entire adapter Gain / Headroom stage is removed**, including LADSPA port, IPC field, GUI settings, presets and CLI. Input passes to ViPERDSP unchanged at 100% (unity). Native Master Limiter and AGC remain. **Start listening with low device volume: replacing 25% input gain with unity may sound up to 12 dB louder with effects or limiting configured differently.**

## Tests

`./tests/check-mock.sh` builds and exercises a mock ViPERDSP library, validates RIFF parsing, live IR replacement/unload in the *same backend instance*, invalid-file retention, cross-rate resampling, bypass and independently forwarded controls. `python3 tests/check-ui-contract.py` checks the GUI sources. Mock tests are **not** an acoustic convolver test. A native Qt6/ViPERDSP compile and PipeWire listening test on Arch are still required.

The IRS collection at https://github.com/eedeidk/PulseAudio-IRSs describes its files as usable by both PulseEffects and ViPER4Android. It is a collection of IR audio assets, *not* a dependency on PulseAudio. Its README says the uploader did not create the underlying IR assets, so do not redistribute those audio files with this project.
