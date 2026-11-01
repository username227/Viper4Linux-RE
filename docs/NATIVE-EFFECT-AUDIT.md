# ViPER for Linux — native effect dispatch audit

Source reviewed: the Dynamic EQ / AnalogX / Tube Simulator / Graphic EQ master, against the pinned
ViPERDSP API at `6fc1f1b8b2123c8cc49f3b3c7c7d5fbbf8a60933`.

## Implemented functions

| UI feature | Actual DSP method | Implementation |
|---|---|---|
| ViPER Bass | `ViPER::ApplyBass` | Native ViPERDSP |
| Mono Bass | `ViPER::ApplyBassMono` | Native ViPERDSP |
| Psychoacoustic Bass | `ViPER::ApplyPsychoacousticBass` | Native ViPERDSP |
| Dynamic System | `ViPER::ApplyDynamicSystem` | Native ViPERDSP |
| Clarity | `ViPER::ApplyClarity` | Native ViPERDSP |
| Spectrum Extension | `ViPER::ApplySpectrumExtension` | Native ViPERDSP |
| Headphone Surround+ | `ViPER::ApplyHeadphoneSurround` | Native ViPERDSP; guarded at 44.1/48 kHz |
| Differential Surround | `ViPER::ApplyDiffSurround` | Native ViPERDSP |
| Field Surround | `ViPER::ApplyFieldSurround` | Native ViPERDSP |
| Reverberation | `ViPER::ApplyReverb` | Native ViPERDSP |
| Tube Simulator | `ViPER::ApplyTubeSimulator` | Native ViPERDSP |
| AnalogX | `ViPER::ApplyAnalogX` | Native ViPERDSP |
| Graphic Equalizer | `ViPER::ApplyEqualizer` | Native ViPERDSP 10/15/25/31-band IIR EQ |
| Dynamic EQ | `ViPER::ApplyDynamicEq` | Native ViPERDSP, 1–10 configurable dynamic bands |
| Convolver | `ViPER::ApplyConvolver` and `ViPER::LoadConvolverKernel` | Native ViPERDSP kernel convolution |
| Master Limiter | `ViPER::ApplyMasterLimiter` | Native ViPERDSP; wet and dry engines |
| Playback Gain Control | `ViPER::ApplyPlaybackGainControl` | Native ViPERDSP |
| LUFS Targeting | `ViPER::ApplyLufs` | Native ViPERDSP; see note below |

**No substituted EQ, bass booster, reverb or surround algorithm is called in the
production adapter.** Both wet and dry engines are instances of native `ViPER`.
Their audio-processing entry point is `ViPER::Process`.

## What our Linux adapter implements (not native effects)

- LADSPA interface and PipeWire filter-chain routing
- 20ms wet/dry bypass crossfade and safe handoff while changing EQ/IRS kernels and Tube/AnalogX/Dynamic EQ filters
- `.irs`/WAV decoding, on-the-fly sample-rate conversion before native IR loading
- Graphic-EQ preset selection, settings storage, precision migration and drawing
- Dynamic EQ settings, band selector, and an illustrative static frequency-curve preview (not real-time metering)
- Input NaN/Inf sanitization and GUI/CLI shared-memory transport

**There is no hidden 25% input gain stage.** Finite samples are forwarded at
unity gain into `ViPER::Process`. The native Master Limiter remains present
in the wet and dry paths (and can modify the audio on both).

## Important qualifications

1. `install.sh` applies one targeted local correction to upstream
   `LUFSTargeting::SetSpeed()` using `patches/fix_lufs_speed.py`. The algorithm
   is still upstream ViPERDSP; this is a locally corrected native setter.
2. Mock tests verify native method calls and parameter forwarding but use a
   **synthetic ViPER test double**; they do not demonstrate that the underlying
   native audio has a specific response or that PipeWire will be glitch-free.
3. Headphone Surround+ is intentionally disabled by the adapter when the
   processing sample rate isn't 44.1 or 48 kHz, despite a saved enabled toggle.
4. The live IRS handoff temporarily emits the dry-path ViPER output. IR
   resampling is Linux-adapter preprocessing; convolution itself is native.
5. Native AnalogX mutes its internal output for a short settling interval on enable/reset; this upstream behavior is not introduced by the Linux adapter.
6. As of this master, unimplemented GUI notices (compressors,
   Stereo Imager, Cure, ViPER-DDC, Speaker Correction, etc.) are not active
   processing controls and are not counted as implemented.
7. The custom `QPainterPath` draws the EQ overview; it never processes audio.

This is a **source-level routing audit**, not an upstream DSP correctness proof,
a professional real-time safety audit, or a native Qt6/PipeWire listening test.
