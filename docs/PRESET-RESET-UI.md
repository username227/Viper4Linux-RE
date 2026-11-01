# Named effect presets and guarded defaults reset (GUI-only)

The former Save Preset, Restore Preset and Reset to Test Defaults footer actions
are consolidated into a single **Presets…** dialog, also available from the ☰
menu. The main effects screen stays uncluttered.

- **Save Preset…** asks for a descriptive name. Names are 1–70 printable
  characters without directory separators. Matching names, ignoring case,
  produce a confirmation before overwriting an existing file.
- **Load** asks before replacing active effect settings, which may contain
  unsaved adjustments. Other stored presets and application preferences are
  preserved. Double-clicking a list item follows the same confirmation path.
- **Rename** updates the human-readable display name in the chosen file while
  keeping its UUID filename stable. A duplicate is refused.
- **Delete** requests confirmation; it removes only the selected saved preset,
  not the configuration currently applied to audio.
- **Reset Effects to Defaults…** asks before returning to Natural Bass enabled,
  other effects disabled, with Cancel preselected. No named presets are removed.

## Data layout and migration

QSettings continues to store active audio settings and app preferences in the
existing configuration. Each manually saved full-effect snapshot is a separate
versioned INI file in `$XDG_DATA_HOME/viper4linux/presets/` (normally
`~/.local/share/viper4linux/presets/`). UUID filenames avoid invalid paths
and accidental collisions from user-provided preset names.

On the first invocation of Presets, the previous one-slot manual preset, if
present, is copied to a named file called **Previously Saved Preset** before
its staging keys can be reused. If no old manual preset exists, migration is
still marked complete, so a newly saved preset will not be falsely migrated
later. A migration write failure stops the dialog without discarding the old
slot. Active settings remain unchanged during migration and when saving.

These snapshots include all available effect settings, bands and external
IRS/VDC selections. An external correction/impulse-response file is referenced,
not embedded: a file removed from the local library cannot be recovered by
loading its preset alone.

No LADSPA DSP, PipeWire, backend protocol, launcher, or routing code changes.
