#!/usr/bin/env python3
"""Guard theme contrast and prevent theme changes from writing DSP state."""
from pathlib import Path
import re
from gui_source_contract import read_gui_sources

root = Path(__file__).resolve().parents[1]
source = read_gui_sources(root)
apply = source[source.index("    void applyTheme(bool classic) {"):source.index("    QVBoxLayout* addEffectsTab(")]
for forbidden in ("push(", "publishToBackend(", "requestIrsLoad(",
                  "setChecked(", "persistCurrentSettings("):
    assert forbidden not in apply, forbidden

# The theme selector callback must also avoid mutating DSP state.
handler = re.search(r'connect\(theme_selector_, QOverload<int>::of\(&QComboBox::currentIndexChanged\),\s*this, \[this\]\(int index\) \{(.*?)\n\s*\}\);', source, re.S)
assert handler, 'theme signal not wired'
for forbidden in ('push(', 'publishToBackend(', 'requestIrsLoad(', 'setChecked('):
    assert forbidden not in handler.group(1), forbidden


# Check real declared theme colors, rather than frozen stylesheet formatting.
def luminance(color):
    channels = [int(color[i:i+2], 16) / 255 for i in (1, 3, 5)]
    rgb = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in channels]
    return .2126 * rgb[0] + .7152 * rgb[1] + .0722 * rgb[2]

def contrast(a, b):
    low, high = sorted((luminance(a), luminance(b)))
    return (high + .05) / (low + .05)

theme = (root / "src/viper-theme.h").read_text()
def declared_colors(name, expected):
    declaration = re.search(r"const QString " + name + r"\s*=\s*([^;]+);", theme, re.S)
    assert declaration, name
    colors = re.findall(r'"(#[0-9A-Fa-f]{6})"', declaration.group(1))
    assert len(colors) == expected, (name, colors)
    return colors

text_dark, text_light = declared_colors("textColor", 2)
tab_dark, tab_light = declared_colors("activeTabText", 2)
bg_dark_classic, bg_dark_modern, bg_light_classic, bg_light_modern = declared_colors("bg", 4)
accent_classic_dark, accent_classic_light, accent_modern_dark, accent_modern_light = declared_colors("accent", 4)
palettes = (
    ("Classic light", text_light, bg_light_classic, accent_classic_light, tab_light),
    ("Modern light", text_light, bg_light_modern, accent_modern_light, tab_light),
    ("Classic dark", text_dark, bg_dark_classic, accent_classic_dark, tab_dark),
    ("Modern dark", text_dark, bg_dark_modern, accent_modern_dark, tab_dark),
)
for name, fg, background, accent, tab in palettes:
    assert contrast(fg, background) >= 4.5, (name, "body")
    assert contrast(accent, tab) >= 4.5, (name, "tab")

# Cinnamon must subscribe to actual setting changes, not periodically run
# `gsettings get` from a GUI timer. Offscreen/CI cannot simulate Cinnamon's
# D-Bus session; this source check guards the event-driven wiring.
init = (root / "src/window/window-initialization.inc").read_text()
gui = (root / "src/viper-gui.cpp").read_text()
assert 'QStringLiteral("monitor")' in init
assert 'QProcess::readyReadStandardOutput' in init
assert 'QStringLiteral("get")' in init  # One startup snapshot only.
assert 'cinnamon_theme_monitor_buffer_' in init  # Cope with split pipe reads.
assert 'desktopThemeTimer' not in init
assert 'QTimer::timeout' not in init
assert 'cinnamon_theme_query_' not in gui
assert 'QStringLiteral("org.x.apps.portal")' in init
assert 'QStringLiteral("color-scheme")' in init
assert 'applyCinnamonColorScheme(QString value)' in init
assert 'scheme == QLatin1String("prefer-dark")' in init
assert 'scheme != QLatin1String("default")' in init
assert 'cinnamon_scheme_monitor_buffer_' in init
assert 'cinnamon_preference_known_ ? cinnamon_preference_dark_' in source
assert 'cinnamon_preference_known_' in gui

# Cinnamon/Wayland must not keep Qt's generic light client-side frame when
# its built-in portal-aware Adwaita decoration is present. Never change X11
# or override a user-selected QT_WAYLAND_DECORATION plugin.
assert 'QGuiApplication::platformName() == QLatin1String("wayland")' in gui
assert '!qEnvironmentVariableIsSet("QT_WAYLAND_DECORATION")' in gui
assert 'qputenv("QT_WAYLAND_DECORATION", QByteArrayLiteral("adwaita"))' in gui
assert gui.index('QApplication app(argc, argv);') < gui.index('qputenv("QT_WAYLAND_DECORATION"') < gui.index('ViperWindow window;')
assert 'viper_theme::applyDialogTheme(dialog, this)' in gui
assert 'qApp->installEventFilter(this)' in gui
assert 'QFrame#viperFooterDivider' in theme


print("PASS: theme contrast and non-mutating theme changes")
