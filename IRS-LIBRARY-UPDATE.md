# IRS library updates and live sample-rate conversion

* `Import…` accepts multiple `.irs` and `.wav` files in one selection. Each is copied
  into `$XDG_DATA_HOME/viper4linux/IRS` (`~/.local/share/viper4linux/IRS` by default).
  Existing filenames are never overwritten; a numeric suffix is added on collision.
  The first imported IR becomes active; the rest appear in the dropdown.
* The dropdown updates in response to `QFileSystemWatcher::directoryChanged` while
  closed. While it is open the list is frozen. Changes made while it is open
  become visible on the next open (or Refresh). No periodic IRS polling timer.
* The native DSP's Convolver kernel is rebuilt after an offline (worker-thread)
  windowed-sinc resampling pass when the IR's rate differs from the PipeWire rate.
  The original IR file is never altered. Playback continues through the existing
  dry handoff during a kernel swap. The resampler preserves filter DC gain.
* Theme label is now **Classic Viper**. Existing stored value `classic-v4a`
  remains valid for backward compatibility.
* Only mock/backend and static GUI checks are run here. Native Qt build, real
  PipeWire switching, CPU use and perceived sound quality need verification on Arch.

## Responsive IRS selector (GUI-only update)

The IRS dropdown's width no longer depends on the longest filename in the
library. Its selected text and popup entries may be visually elided, but the
entire selected filename remains available in a selectable, word-wrapped
read-only field below the dropdown. Uninterrupted names wrap by character,
and exceptionally long names can scroll vertically. Hovering an entry also
shows its full name. The long filename,
path, and stored settings are not truncated. The Convolver form wraps long
rows rather than pushing its slider or Import/Refresh buttons off-screen.
No changes to DSP, live switching, import paths, or sample-rate resampling.
