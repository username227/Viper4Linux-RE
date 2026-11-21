# Timer and polling audit — 2026-11-21

Scope: tracked production source, packaging scripts, CI workflows, and test helpers at `main` commit `6af27be68b27`. This is a source-level audit, **not** a runtime profiler report. Timer usage in independent third-party ViPERDSP sources is outside this repository audit.

## Remove now

| Location | Previous behavior | Decision |
|---|---|---|
| `src/window/window-initialization.inc` | Every five seconds, run a new `gsettings get org.cinnamon.desktop.interface gtk-theme` process | **Removed**: one `gsettings monitor` change subscription plus a single startup snapshot. No periodic theme query. |
| Same file, Qt color-scheme callback | Zero-delay QTimer to defer a theme application | Replaced with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`, keeping deferred delivery without a timer. |

The new Cinnamon listener is **event-driven** rather than a repeated query. It does not require a new build dependency. It still classifies GTK theme names by the presence of `dark`; this separate detection heuristic is not proved correct for arbitrary custom themes and must be checked against real desktop settings.

## Periodic work that is still present

| Location | Interval | Purpose | Assessment |
|---|---|---|---|
| `src/viper-gui.cpp` and `src/sync/gui-backend-bridge.inc` | 500 ms | Show live audio activity from shared-memory callback counters, reconnect to the backend, reflect CLI changes; also read the current output-status JSON | **Long-term event-driven candidate**, but removing this now without adding an IPC change notification would break live status, reconnection and CLI/GUI synchronization. Consider adding low-overhead notifications for state changes while retaining a deliberate rate for activity displays. |
| `src/viper_output_monitor.py` | `select(..., 0.75)` with 1 s heartbeat, 3 s checkpoint | Process actual `pw-dump -m` change notifications, settle output identity, check backend death and preset requests, maintain status freshness, checkpoint state | **Retain pending architectural redesign**. Its `select` sleeps until PipeWire input or a bounded deadline; it is not an external command launched repeatedly. Frequent unchanged JSON writes could be optimized separately after testing recovery and file-watcher clients. |
| `src/viper_log_retention.py` | At most hourly maintenance (inside monitor servicing) | Expire old log records even when playback is silent | **Retain**. Not its own repeating timer. |

## Bounded waits, safety timeouts, and intentional deferrals

| Location | Function | Assessment |
|---|---|---|
| `src/viper-gui.cpp` `save_timer_` | Single-shot settings synchronization debounce | Keep: prevents repeated disk writes during slider adjustments. |
| `src/viper-gui.cpp` | Single-shot 2 s absent-tray startup fallback | Keep: necessary when the panel registers late at login. |
| `src/window/window-shell.inc` | Single-shot 45 s restart watchdog | Keep: prevents a hung restart process from disabling controls indefinitely. |
| `src/window/window-shell.inc` | Single-shot 450 ms X11 restore settle/retry | Keep pending separate X11 regression testing; this addresses a known window-manager race, not periodic polling. |
| `src/viper-gui.cpp`, `src/window/window-shell.inc` | Zero-delay queued GUI work and one-time Wayland startup retry | Keep: deferred event-loop operations, not periodic checks. |
| `src/viper-lifecycle.h` | Bounded 100 ms waits, one-time 250 ms wait, socket timeouts | Keep until lifecycle IPC offers acknowledgement signals; these execute only during explicit lifecycle operations. |
| `src/viper-cli-worker-ack.h` | Bounded 10 ms wait for asynchronous worker acknowledgement | Candidate for a future native wait/notification, but changing it requires paired worker-side changes and must not stall an audio callback. |
| `src/viper_output_info.py` | Bounded 100 ms wait, maximum 10 s, for output-preset assignment response file | Candidate for a future event-notified acknowledgement channel. Not an always-on timer. |
| `scripts/verify.sh` | One 3 s wait between consecutive DSP status snapshots | Keep: explicit manual verification command, not a running background loop. |

## Tests and CI

No always-on production polling was found in the scanned packaging/CI workflows. Tests contain short `sleep` and `singleShot` calls to model asynchronous operations or enforce timeouts (notably `tests/mock/check.cpp`, single-GUI/tray/startup tests and the mock worker). These are not runtime timers. Removing test delays mechanically would make those tests unreliable.

## Verification requirements

- Run `tests/check-theme-contract.py` and the full CTest suite on a machine with Qt6 installed.
- On Cinnamon **X11 and Wayland**: change the GTK Controls theme light → dark → light while the GUI remains open; inspect the main window and About/Preferences. Test a cold start in each theme.
- Confirm the `gsettings monitor` child exits with the GUI and that switching a non-GTK Cinnamon shell theme is not accidentally interpreted as changing the GTK Controls theme.
- On KDE: independently test the separate dialog palette branch against ZeroAnarchy's reported first-launch symptom.
- Run end-to-end GUI audio/status and lifecycle tests before attempting an IPC/heartbeat redesign.

**Do not treat a source audit as evidence that all timing code can safely be deleted.** Only remove a timer when it has a demonstrably correct event-driven replacement.
