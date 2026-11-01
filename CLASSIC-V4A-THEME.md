# ViPER for Linux — consistent Modern Teal and Classic Viper colors

The **Theme** menu offers **Modern teal** and **Classic Viper**. Both have complete
light and dark palettes, including card backgrounds, all foreground labels,
sliders, tabs, input widgets, and the informational panels.

## Desktop color scheme

- **Cinnamon/X11:** Read `org.cinnamon.desktop.interface gtk-theme` via an
  asynchronous `gsettings` query. A name containing `dark` selects the dark
  palette; any other valid theme name selects light. Recheck every five seconds
  to track desktop theme changes. If querying the setting fails, **light is the
  fallback**. This is deliberately preferred over Qt's potentially conflicting
  color scheme hint on Cinnamon.
- **Other desktops:** Use Qt 6.5+ `QStyleHints::colorScheme()`, responding to
  `colorSchemeChanged`. `Unknown` and Qt <6.5 use **light**.

For either desktop, theme colors are applied **only to this window**. The
application-wide palette and stylesheet are not changed. `ui/theme` remains
stored in `QSettings`; DSP presets and processing are untouched.

## Checks and limitations

- `python3 tests/check-theme-contract.py` — static source requirements, including
  text/background contrast calculations for all four style combinations.
- `python3 tests/check-ui-contract.py`
- `./tests/check-mock.sh`

Those checks do **not** replace an actual Qt6 build or the visual test on Arch.
For an appearance problem on Cinnamon, inspect the desktop's GTK setting with:

```bash
gsettings get org.cinnamon.desktop.interface gtk-theme
```

If Classic Viper is still dark despite a light GTK theme, report this value and
`echo "$XDG_CURRENT_DESKTOP"` so detection can be corrected without touching DSP.
