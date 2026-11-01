# Viper4Linux: Dynamic EQ editor and theme-dependent branding

## User-facing updates

- The clickable Dynamic EQ preview opens a resizable, scrollable editor showing **all active bands** in a single summary table (frequency, maximum gain and threshold).
- Double-click a table cell to edit its numeric value; click a row or a chart marker to select a band for the complete native controls (frequency, shape, Q, maximum gain, threshold, attack, release). No DSP functionality has been removed.
- The Graphic EQ click-to-open behavior and its Acoustic preset remain unchanged.
- Existing band values, including saved custom frequencies, take precedence over default values. Previously unused bands get distributed default frequencies rather than stacking at 1 kHz.
- Editing Dynamic EQ while **already disabled** no longer triggers the adapter's global 25 ms dry transition, preventing interruption to other active effects. Enable/disable and active parameter changes still use the native filter handoff.
- The Static curve remains a **configured maximum-target illustration**, not measured frequency response or real-time metering.

## Dynamic EQ usability refinements (November 2026)

- The main Dynamic EQ card displays **Active bands (1–10)** without opening the editor. The band-count control inside the editor is synchronized with it.
- Both the compact main-window graph and the larger editor graph display **one numbered marker per active band** even before any click. Where multiple saved bands share or nearly share a frequency, their numbered markers spread slightly apart, with dotted leaders to the true curve locations. This is display-only; no DSP settings are changed by marker spacing.
- New/default band layouts use distinct bass-to-treble center frequencies for each count from 1 through 10. Legacy saved frequencies are **never silently overwritten**, even when several are at 1 kHz.
- The editor offers **Spread frequencies** to deliberately redistribute the active bands while keeping their existing gain, Q, threshold, filter type, attack and release settings. After changing the frequency, listen again to judge the effect; the shape will change.
- The main window, Graphic EQ editor and Dynamic EQ editor remember their **sizes** via the existing QSettings store. The main window also restores its maximized state. Restored sizes are constrained to the current screen's available area; window positions are deliberately not restored.
- The graphs remain **static illustrations of configured target response**, not live audio meters. The current native DSP and audio routing are unchanged.

## Application branding

- The user-approved broad-left/narrow-right V has no background: both PNG images have real RGBA alpha and scalable SVG sources are included.
- **Modern teal GUI** displays the teal V beside the `Viper4Linux` heading.
- **Classic Viper GUI** displays the blue V beside the heading.
- **Application/window/taskbar icon** is always the **blue V**, installed through `QApplication::setWindowIcon` and `QMainWindow::setWindowIcon` using icons embedded by Qt's RCC compiler; no .desktop launcher is required for the X11 window icon.
- Window-manager behavior can vary by desktop environment; test on Cinnamon X11.

## Developer verification

- Isolated mock DSP, native-effect forwarding, EQ presets, live IRS, Qt GUI source and theme regressions pass.
- No installed Qt6 development package was available in the packaging runtime, so Arch native Qt6 compilation and X11 taskbar observation remain to be tested.
