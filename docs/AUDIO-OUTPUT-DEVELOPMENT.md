# Audio Output memory — implementation reference

Audio Output memory is separate from the named full-effect preset library.
The GUI's **Preferences → Remember settings per Audio Output** option is
disabled until the user enables it. Enabling memory keeps the current sound
and records an independent state for the identified physical output.

## Physical output identity and safety

`src/viper_output_graph.py` discovers PipeWire's actual physical destination,
excluding ViPER's own virtual sink. A stable identity is derived from
structured device and node properties, not display-name substring matches or
short-lived PipeWire object IDs. Ambiguous, disconnected and unresolved states
must not trigger speculative restores.

The monitor (`src/viper_output_monitor.py`) runs independently of the GUI,
under the launcher, and waits for a stable output before checkpointing or
restoring its complete DSP state. A change to a previously remembered output
restores its saved snapshot. Physical-output JSON state is kept under
`$XDG_DATA_HOME/viper4linux/outputs/`; each output has independent settings.

## Native snapshots and preset assignments

`src/viper-output-state.cpp` captures, validates and restores complete native
DSP controls with the appropriate effect rebuild requests and IRS/VDC file
checks. Changes to the IPC state layout require corresponding snapshot
regression updates. Output snapshot restoration also refreshes full-precision
settings keys for CLI values that cannot be represented exactly by GUI sliders.

The Presets manager can assign a named full-effect preset to an active or
previously remembered inactive output. Requests are handled by the same monitor
thread that checkpoints output state, with an acknowledgement/result file.
Assigning to the active output applies settings immediately; assigning to a
different output only updates its saved memory and does not change the current
sound. Subsequent edits to the output do not modify the named preset.

The separate `viper4linux-output` utility lists outputs, enables/disables
memory, and supports `--assign-preset FILE --target OUTPUT_KEY`.

## Verification

Unit tests exercise discovery, stable identities, output transitions and
assignment. `tests/check-output-state.cpp` tests native snapshot round-trips,
validation and persistence; the developer CTest suite includes the mock
DSP and package checks. Live acceptance still requires checking hardware
switching, idle/no-playback initialization, Bluetooth reconnection and
per-output preset assignment. These were exercised on Cinnamon/X11 with the
project's primary tested hardware; wider hardware and desktop coverage remains
necessary.

No dedicated systemd unit is required by this feature.
