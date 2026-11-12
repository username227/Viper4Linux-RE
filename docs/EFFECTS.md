# Native DSP effects reference

All 24 effect groups are routed to the pinned native ViPERDSP implementation.
Our Linux integration supplies PipeWire, shared-memory commands, file loading,
GUI state, and worker-thread handoff; it is not a substitute DSP algorithm.
Three documented downstream patches are listed in [Development](DEVELOPMENT.md).

## Output (3)

| Effect | Purpose and cautions |
|---|---|
| Master Limiter | Mandatory native peak limiting, output gain and channel balance **while Master is ON**, even if every optional effect is OFF. Master OFF bypasses the entire native DSP stage (including limiter, gain, pan and lookahead) after a short transition; stable OFF audio is exact sanitized passthrough. The limiter is not a listening-safety guarantee. |
| Playback Gain Control (AGC) | Automatically manages level; can interact with LUFS targeting and change loudness. |
| LUFS Targeting | Targets perceived loudness with selectable adjustment speed and ceiling; includes a downstream `SetSpeed` bug fix. |

## Bass (4)

| Effect | Purpose and cautions |
|---|---|
| ViPER Bass | Natural/Pure/Subwoofer styles; frequency and strength. Native anti-pop is configured internally. |
| ViPER Bass Mono | Similar controls for mono-oriented low-frequency processing. |
| Psychoacoustic Bass | Generates harmonics to create perceived bass; use conservative intensity. |
| Dynamic System | Adjusts low/high response according to its X/Y and strength controls. |

## EQ & Tone (6)

| Effect | Purpose and cautions |
|---|---|
| Graphic Equalizer | Native 10/15/25/31-band EQ; compact curve opens the fader editor. Includes built-in and user-defined EQ curve presets. |
| Dynamic EQ | Up to 10 independently configured dynamic bands, with frequency, shape, Q, gain, threshold, attack, and release. Overview curve is illustrative, not live metering. |
| ViPER Clarity | Native clarity modes and strength. |
| Spectrum Extension | Native spectrum/exciter processing. |
| Tube Simulator | Native tube coloration with model/drive/mix controls. |
| AnalogX | Native analog-character processing. |

## Spatial (6)

| Effect | Purpose and cautions |
|---|---|
| Headphone Surround+ | Native VHE convolution-based headphone spatial effect, with five distinct selectable profiles rather than a quality ranking; especially audible on headphones. |
| Differential Surround | Delay, wet/dry mix, polarity and low-pass processing. |
| Field Surround | Width, central image, and depth controls. |
| Reverberation | Room, width, damping, wet and dry levels. |
| Cure / Crossfeed | Blends filtered left/right content; may narrow apparent stereo width. |
| Stereo Imager | Independent low/mid/high side-channel width (100% = unchanged width) and crossover frequencies; widening can weaken mono compatibility. |

### Headphone Surround+ details

Headphone Surround+ uses five upstream VHE surround profiles, labeled
**Level 1** through **Level 5**. Higher numbers do not necessarily mean
better audio: each level gives a different spatial presentation. Switching
levels can rebuild the convolution filters. The effect supports native
processing at **44.1 and 48 kHz**; when the PipeWire processing rate is
different, this effect is inactive. The GUI keeps the level selector compact
and reports a sample-rate incompatibility in the selector/switch tooltip,
rather than displaying a permanent description in the expanded panel.

## Dynamics (2)

| Effect | Purpose and cautions |
|---|---|
| FET Compressor | Full-spectrum compression. Makeup gain may cause a large volume increase even with subtle compression. |
| Multiband Compressor | Three or five frequency bands, with one selected band's controls visible at a time. Each band compresses independently. |

The GUI deliberately omits some native **advanced compressor parameters**
(e.g., automatic timing/knee/gain and adaptation) to avoid clutter. These are
candidates for a separate Advanced dialog; the missing controls should not
be described as currently implemented. Most professionally mastered music
may not benefit from additional compression; both effects default off.

## Sound Correction (3)

| Effect | Purpose and cautions |
|---|---|
| Convolver | Loads stereo/mono `.irs` or compatible `.wav` impulse responses. Library selection can replace an IR without restarting the backend. |
| ViPER-DDC | Loads external `.vdc` cascaded-biquad correction profiles; native processing supports **44.1 and 48 kHz** only. A valid profile must be selected to have an audible effect. When selecting/reloading a profile, the Linux loader briefly crossfades the **entire effects chain** to the dry path (approximately 20 ms out, 35 ms total wait, 20 ms back) to protect against filter-swap clicks. Other effects resume automatically; the bypass is not continuous during playback. |
| Speaker Correction | Built-in high/low/band-pass filter chain; not a general-purpose headphone calibration profile. Internal filter parameters are not yet user-exposed. |

ViPER-DDC and Speaker Correction default off. Speaker Correction's fixed
filters are not tailored to a specific headset and should not be assumed
appropriate for headphones.

## Verification and limits

Effect switches can be tested through the GUI, but a responsive switch does
not prove identical acoustic response to the original platform. The included
mock tests verify control forwarding, state and bypass; actual real-time
behavior, frequency response and listening tests need live PipeWire tests.
Some native features have limitations at unsupported rates. External IRS/VDC
files are **not included** in saved presets; the preset stores their selection.
