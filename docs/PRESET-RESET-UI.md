# Defaults reset confirmation (GUI-only)

The bottom-row button formerly labeled **Reset to test defaults** is now
**Reset Effects to Defaults…**. It restores the same original application
values as before (Natural Bass on, other effects off); the DSP backend and
preset storage schema are unchanged.

A warning dialog explains the effect of the reset, offers **Cancel** (the
default) and **Reset Effects**, and explicitly promises that the separately
saved manual preset will not be deleted. Dismissing the dialog or canceling
does not alter effect settings. This does **not** introduce named, full-system
presets; the existing global Save Preset / Restore Preset pair continues to
store one manual snapshot independently of auto-saved startup settings.

No LADSPA, PipeWire, launcher, backend protocol, routing, or audio effect source
files changed in this commit.
