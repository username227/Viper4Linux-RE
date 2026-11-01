# Native ViPER-DDC

This effect reads standard ViPER4Android `.vdc` coefficient profiles.

- Two lines `SR_44100:` and `SR_48000:` with the same number of five-number sections, representing `(b0,b1,b2,a1,a2)`.
- Validates file size, coefficient ranges, missing sample rates, finite floats, and Schur stability.
- Native `ViPERDSP::LoadDdcCoefficients` and `ApplyDdc`, no substitute equalizer.
- Disabled by default. Selecting a profile or toggling the switch triggers an off-RT worker reload; a failed load bypasses DDC and discards the prior profile.
- Only processes 44,100 or 48,000 Hz, consistent with native engine; other sample rates bypass with an explicit status.
- An optional separately supplied `neutral-test.vdc` identity **test** profile verifies file loading; it does not correct headphones. For practical use, select a profile measured for your own headphone model.
- Does not modify or automatically load user audio kernels.

## User VDC library

- Profiles reside in `$XDG_DATA_HOME/viper4linux/VDC` (usually
  `~/.local/share/viper4linux/VDC`), separate from `IRS/`.
- The GUI creates the directory, lists readable `.vdc` files, and supports
  **Import…** (copy without overwriting an existing filename) and **Refresh**.
- Manually placing a `.vdc` file in the folder makes it available in the dropdown.
  A filesystem watcher refreshes the closed dropdown without interrupting the
  selection of an open popup.
- Previous settings referencing an external `.vdc` file remain selectable.
- The disposable test profiles are distributed **separately**, not installed,
  bundled into the source archive, or tracked in Git. The optional
  `treble-rolloff-test.vdc` intentionally attenuates high frequencies to provide
  an audible A/B check. It is not calibrated for any headphones.
