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

print("PASS: theme contrast and non-mutating theme changes")
