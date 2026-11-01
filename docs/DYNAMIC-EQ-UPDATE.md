# Dynamic EQ — native ViPERDSP

This update adds the upstream ViPERDSP `ApplyDynamicEq` processor. There is no substitute DSP.

In **EQ & Tone**, Dynamic EQ sits immediately below Graphic Equalizer. Its always-visible compact preview is an illustrative **static maximum target curve**, not the exact multi-biquad measured transfer function or real-time metering. Click the curve to open the separate editor.

Choose 1–10 bands; every band has frequency, Peak / Low shelf / High shelf, Q, maximum gain (-12 to +12 dB), threshold (-80 to 0 dB), attack (1–100 ms), and release (10–500 ms). Defaults: three flat bands at 100/1000/8000 Hz, with effect disabled. Configured band values persist in normal settings and the one-slot Save/Restore preset.

Filter changes are applied by the existing background worker, guarded by the non-blocking audio mutex. The DSP itself is upstream native ViPERDSP at the pinned revision. Discrete edits may cause brief dry transitions; there is no added audio callback polling or visual animation.

**Note on the tab name**: Qt interprets `&` as a mnemonic; the source now escapes it as `&&` in the actual tab label, to display `EQ & Tone` instead of `EQ_Tone` with an underline. The saved tab key remains `EQ & Tone`.

Validation: isolated mock and source contracts run without native Qt6 compilation. Test the native build and actual PipeWire processing on Arch before declaring release quality.
