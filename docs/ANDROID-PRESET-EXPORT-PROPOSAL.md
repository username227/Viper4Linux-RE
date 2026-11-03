# Proposal: Export named presets to ViPER4Android (one-way)

**Status:** experimental feature branch, not part of a stable release. This is a
proof of concept for manual testing against likelikeslike/ViPER4Android.

## Scope

- Add **Export for ViPER4Android…** to the existing **Effect Presets** manager.
- Read the selected native QSettings INI preset without changing it or applying
  it to the running DSP.
- Convert supported effect values to ViPER4Android's grouped JSON
  **schemaVersion 2.1** and save atomically using a file picker.
- Do **not** change native preset storage, import Android JSON, or write to
  Android's database.
- Do not add exporter tests to ordinary makepkg builds.

## Confirmed upstream format

ViPER4Android's \`EffectPrefs.kt\` uses \`schemaVersion: 2.1\` and a JSON
object per effect. The Android import path in \`MainViewModel.kt\` accepts
\`.json\` files and deserializes them using \`deserializeEffectPrefs\`.
When one preset is imported, it immediately applies the resulting settings.

Source:
- https://github.com/likelikeslike/ViPER4Android/blob/main/app/src/main/java/com/llsl/viper4android/effect/EffectPrefs.kt
- https://github.com/likelikeslike/ViPER4Android/blob/main/app/src/main/java/com/llsl/viper4android/ui/screens/main/MainViewModel.kt

## Safety and compatibility

This first pass maps limiter, AGC, LUFS, Bass, Bass Mono, Clarity,
Spectrum Extension, Graphic EQ (10/15/25/31 bands), Dynamic EQ, Headphone
Surround, Differential Surround, Stereo Imager, Reverb, AnalogX and Tube
enable. Android-only preset IDs are deliberately not fabricated.

Android cannot read Linux-local IRS and VDC paths. The exporter turns off
Convolver and DDC in exported JSON and warns when the source preset had them
enabled. Unsupported effect groups are explicitly turned off so importing
does not accidentally retain effects from the phone's previous state.

**Still requires physical Android validation:** Some controls may use
different allowable ranges or DSP interpretations, especially LUFS,
Headphone Surround, advanced compressor controls, spatial algorithms, and
the precise setting units for Spectrum Extension. The experimental exporter
does not yet translate advanced compressors, Field Surround, Cure
crossfeed, psychoacoustic bass, or Dynamic System; it warns when such
features were enabled. Do not treat this prototype as bit-perfect
cross-platform equivalence.

## Suggested acceptance test

1. Build and install the feature branch on Arch in isolation from the AUR
   package's main-branch source. Do not push this branch to the AUR.
2. Save a named preset with a distinctive Bass value, Clarity value and
   custom 10-band EQ. Turn off Convolver/DDC initially.
3. Open **Effect Presets**, select the preset, then choose
   **Export for ViPER4Android…** and save a JSON file.
4. Check that the file begins with \`"schemaVersion": 2.1\` and has expected
   \`bass\`, \`clarity\` and \`equalizer\` groups.
5. Transfer JSON to Android and import it with the ViPER4Android GUI.
   **Importing one preset applies it immediately**, so start with an
   easily reversible test configuration and moderate listening volume.
6. Compare the settings, especially effect enable flags, Bass gain,
   Clarity gain, Spectrum frequency and all 10 EQ band levels. Record any
   differences or import errors; then test more complex effects.

Run the isolated contract guard from a feature-branch checkout:

\`\`\`sh
python3 tests/check-android-export-contract.py
\`\`\`

The next iteration should capture a JSON export from Android (or inspect
the app's internal serialized values), compare precise units and bounds,
and add executable fixture-based conversion regression tests before merging.
