# Presets and external audio libraries

## Named effect presets

Open **Presets…** from the main window or ☰ menu. The manager supports:

- **Save Preset…** — enter a name; an existing case-insensitive match requires
  confirmation before overwrite.
- **Load** — confirm replacement of the currently active effect configuration.
- **Rename** — update a name without changing the preset file's UUID filename.
- **Delete** — confirm removal of that saved file only.
- **Reset Effects to Defaults…** — confirm restoring the application's built-in
  starting effect settings; Cancel is the default confirmation action.

The built-in reset returns to Natural Bass on and other effects off. It does
**not** delete named presets, theme choices, window/tray settings, or library
files. Current active effect settings are saved separately and automatically.
Saving a named preset is an explicit snapshot, not a change to active playback.

Each complete effect snapshot is a versioned `.ini` file in
`$XDG_DATA_HOME/viper4linux/presets/` (typically
`~/.local/share/viper4linux/presets/`). The GUI uses Qt's QSettings for current
settings under the `ViPERforLinux` organization and `ViPERClean` application.
On first use, an existing pre-library one-slot preset is imported as
**Previously Saved Preset**. This migration happens only once; users without
an older preset do not get a fictitious imported entry.

The Graphic EQ also maintains **separate named EQ curve presets** inside its
editor. Those are not full-effect snapshots and remain stored in GUI settings.

## Data layout

```text
~/.local/share/viper4linux/
├── presets/    # named effect snapshots (.ini)
├── IRS/        # convolver impulse responses (.irs, supported .wav)
└── VDC/        # headphone correction profiles (.vdc)
```

The precise base directory follows XDG data-home configuration. The GUI can
import external IRS or VDC files by copying them into their library folder,
with collision-safe naming, and can refresh the corresponding selector.
Files copied directly into the folders can also be selected after refresh.

### IRS (Convolver)

Use the Sound Correction tab's Convolver selector. Import supports multiple
files. The backend accepts supported mono/stereo RIFF PCM16/24/32 or float32
impulse-response data, and resamples IRs that differ from the processing rate.
Loads happen away from the real-time audio callback. **None** unloads the
selected kernel. A selected preset can recall an IRS **reference** but cannot
restore a file removed from disk.

### VDC (ViPER-DDC)

Select a `.vdc` file in Sound Correction; importing a file copies it to the
VDC library. The format contains filter coefficients for 44.1 and 48 kHz.
An invalid or absent profile must not silently enable effective processing;
other sampling rates bypass native DDC processing. Test profiles used during
development were deliberately excluded from the application repository.

**Back up** the preset library and any manually imported IRS/VDC files if you
intend to reuse configurations across installations. A named preset does not
embed those external files.
