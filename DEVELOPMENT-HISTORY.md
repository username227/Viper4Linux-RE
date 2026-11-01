# Development versions and Git history

Viper4Linux source changes are now committed to Git. The first Git commit is
an imported source snapshot of the tested Dynamic EQ/window-memory baseline,
not a reconstruction of all earlier ZIP development steps. Each subsequent
change is a normal Git commit with a real object ID. No commit IDs have been
invented for earlier archives.

- During development, the About dialog shows `dev-<12-character Git hash>`.
- A clone determines this at CMake configure time from `git rev-parse`.
- The complete release ZIP has no `.git` directory but ships a
  `RELEASE-REVISION` manifest with the exact commit hash for version display.
- To refresh a GUI built from an existing Git checkout after another commit,
  rerun `cmake -S . -B build` before `cmake --build build --target viper-gui`.
- The Git bundle accompanying this ZIP contains all genuine commits since
  Git tracking began, plus tags. It can be cloned/imported locally and pushed
  to GitHub later. Prior ZIP-only releases are not individual historical commits.

This maintenance release changes Dynamic EQ control handoff only, removing
its unnecessary 25 ms *global* transition-to-dry for edits while active.
The native algorithm and other effect processing are unchanged. PipeWire's
audio callback still uses a nonblocking `try_lock` and can briefly fall back
to the dry path if it encounters the worker lock; therefore completely
artifact-free live editing must be verified on real hardware.

No automatic taskbar/desktop launcher workaround is needed for the About icon.
