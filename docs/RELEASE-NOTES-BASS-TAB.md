# ViPER for Linux — Bass tab release (complete source)

Baseline: Output-first project, which itself was a GUI-only change to the proven Output-controls release.

## Layout

Output | Bass | Enhancements | Surround

The selected tab is stored by name. Each effect has an independent ON/OFF switch and expand/collapse control. Closing a card does not change audio processing.

## New, independent effects (OFF by default)

* ViPER Bass Mono: Natural / Pure Bass+ / Subwoofer, strength, frequency.
* Psychoacoustic Bass: cutoff, harmonic intensity, harmonic order 2–5, original bass level.
* Dynamic System: X and Y filter frequencies, low/high side gain, bass strength.

All three use native ViPERDSP ApplyBassMono, ApplyPsychoacousticBass and ApplyDynamicSystem calls from the pinned revision. Existing ViPER Bass (and its three algorithms) is moved into Bass without changing its processing. No existing effect implementation is intentionally modified.

## Safety and installation

This release extends the shared-memory protocol from v11 to v12; a BACKEND RESTART is required for these new processors. Do not mix this GUI with the older backend.

1. Extract to a separate directory, preserving the old working folder.
2. Build with ./install.sh. This requires Arch qt6-base, ladspa and PipeWire dependencies, plus network access to fetch the pinned ViPERDSP source if absent.
3. Stop the previous ViPER backend with its ./stop.sh. Quit JamesDSP completely.
4. Start from this project: ./start.sh; launch its GUI via ./gui.sh.
5. Initially leave all three new processors OFF; confirm prior processing works. Enable one at a time at low output volume.

The mock LADSPA regression verifies control plumbing and bypass, **not** the sonic behavior of native ViPERDSP. The Qt GUI could not be compiled in the packaging environment; build and real-audio tests must run on your Arch desktop.
