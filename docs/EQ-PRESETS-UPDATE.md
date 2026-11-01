# Exact ViPER4Android built-in EQ presets

The previous generic Flat/Classical/Rock/Pop/Jazz/Vocal/Bass/Treble presets
have been replaced with the **12 built-in ViPER4Android presets**:
Acoustic, Bass Booster, Bass Reducer, Classical, Deep, Flat, R&B, Rock,
Small Speakers, Treble Booster, Treble Reducer and Vocal Booster.

Values are transcribed from
`likelikeslike/ViPER4Android/app/src/main/java/com/llsl/viper4android/effect/BuiltinPresets.kt`.
Each preset has *native, separate* 10/15/25/31-band curves (no synthesized
interpolation for built-in presets). Acoustic's 1.75 dB band is retained
exactly by upgrading the EQ IPC and fader resolution to 0.01 dB.

The compact EQ curve auto-fits to its own min/max and uses cubic Bezier
segments that pass exactly through each data point. Flat profiles remain
centered. This is a **read-only visualization** of band gains—not a measured
frequency response, which depends on the underlying ViPERDSP filter bank.

A one-time QSettings migration converts previously saved tenth-dB EQ values
(including custom presets and app snapshots) to hundredths; names/curves are
preserved. Old built-in names no longer offered fall back to Custom so
we don't silently replace Pop/Jazz/etc. with a different curve.

This change bumps shared-memory protocol version to 17 and namespace to
`viperlinux-unified-v10-control-<UID>`. Stop the previous backend and install
the new complete master once; existing GUI and backend builds must not be mixed.

Native Qt6 rendering and live PipeWire listening have **not** been performed
in the portable test environment. The isolated mock test suite validates
native method forwarding, save-format data, exact preset curves and range math.
