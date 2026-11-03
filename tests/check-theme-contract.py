#!/usr/bin/env python3
"""Static theme-contract checks. Runtime color verification still needs Qt on Arch."""
from pathlib import Path
import re
from gui_source_contract import read_gui_sources
source = read_gui_sources(Path(__file__).resolve().parents[1])
checks = (
    'theme_selector_->setObjectName("viperThemeSelector")',
    'QComboBox::drop-down',
    'border-top-right-radius:7px; border-bottom-right-radius:7px;',
    'background-color:transparent;',
    'theme_selector_->addItem("Modern teal", "modern")',
    'theme_selector_->addItem("Classic Viper", "classic-v4a")',
    'settings_.value("ui/theme", "modern")',
    'settings_.setValue("ui/theme", index == 1 ? "classic-v4a" : "modern")',
    'QPalette pal = qApp->palette();',
    'setPalette(pal);',
    'setStyleSheet(QString(R"THEME(',
    'QMainWindow#viperWindow',
    'QWidget#viperEffectsContent',
    'QLabel, QGroupBox, QToolButton, QRadioButton, QCheckBox { color:%2; }',
    'QPalette::WindowText',
    'QPalette::Text',
    'QPalette::ButtonText',
    'const QString activeTabText = dark ? "#092129" : "#ffffff";',
    'QTabBar::tab:selected { background-color:%4; color:%6; border-color:%4; }',
    'qEnvironmentVariable("XDG_CURRENT_DESKTOP")',
    'org.cinnamon.desktop.interface',
    'gtk-theme',
    'QProcess::finished',
    'cinnamon_theme_known_ && cinnamon_theme_dark_',
    'return false; // On Qt < 6.5 Unknown falls back to LIGHT.',
    '&QStyleHints::colorSchemeChanged',
    'QTimer::singleShot(0, this',
    'theme_muted_color_',
    'viperWarningText',
    'viperSliderValue',
    'class EffectGlyph final : public QWidget',
    'glyph->setColor(QColor(accent));',
    'viperMutedText',
    'setClassicTheme(classic, dark)',
)
for required in checks:
    assert required in source, required
for forbidden in ('qApp->setStyleSheet(', 'qApp->setPalette(', 'original_app_style_'):
    assert forbidden not in '\n'.join(line for line in source.splitlines() if not line.lstrip().startswith('//')), forbidden
apply = source[source.index('    void applyTheme(bool classic) {'):source.index('    QVBoxLayout* addEffectsTab(')]
for forbidden in ('push(', 'publishToBackend(', 'requestIrsLoad(', 'setChecked(', 'persistCurrentSettings('):
    assert forbidden not in apply, forbidden
handler = re.search(r'connect\(theme_selector_, QOverload<int>::of\(&QComboBox::currentIndexChanged\),\s*this, \[this\]\(int index\) \{(.*?)\n\s*\}\);', source, re.S)
assert handler, 'theme signal not wired'
for forbidden in ('push(', 'publishToBackend(', 'requestIrsLoad(', 'setChecked('):
    assert forbidden not in handler.group(1), forbidden

# Check the foreground/background combinations used by the C++ theme values.
def lum(c):
    channels = [int(c[i:i+2], 16) / 255 for i in (1,3,5)]
    r,g,b = [v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in channels]
    return .2126*r+.7152*g+.0722*b

def contrast(a,b):
    lo,hi=sorted((lum(a),lum(b)))
    return (hi+.05)/(lo+.05)

palettes = (
    ('Classic light', '#203038', '#ffffff', '#087eae', '#ffffff'),
    ('Modern light',  '#203038', '#eaf3f5', '#236e76', '#ffffff'),
    ('Classic dark',  '#edf4f6', '#252628', '#12b8e8', '#092129'),
    ('Modern dark',   '#edf4f6', '#273b40', '#3299a5', '#092129'),
)
for name, fg, card_bg, accent, selected_fg in palettes:
    for color in (fg, card_bg, accent, selected_fg):
        assert color in source, (name, color)
    assert contrast(fg, card_bg) >= 4.5, (name, 'body', contrast(fg, card_bg))
    assert contrast(accent, selected_fg) >= 4.5, (name, 'tab', contrast(accent, selected_fg))

print('PASS: theme switching source contract, Cinnamon GTK light fallback, no DSP writes, four contrast pairings')

# The Theme-only fix was visually verified on Cinnamon in r26. Apply the
# exact same cap and explicit arrow to ALL QComboBox descendants, while keeping
# the validated wider Theme label and preserving dialog/IRS selection behavior.
from xml.etree import ElementTree
assert 'theme_selector_->setFixedWidth(140)' not in source
assert 'theme_selector_->fontMetrics().horizontalAdvance("Classic Viper") + 62' in source
assert 'QComboBox { padding-right:26px; }' in source
assert 'QComboBox::down-arrow' in source
assert 'image:url(%8); width:12px; height:8px;' in source
assert 'subcontrol-origin:border; subcontrol-position:top right;' in source
assert source.count('::drop-down') == 1, 'A single common dropdown rule should style all combo boxes'
assert source.count('::down-arrow') == 1, 'A single common chevron rule should style all combo boxes'
assert ':/viper4linux/combo-chevron-light.png' in source
assert ':/viper4linux/combo-chevron-dark.png' in source
qrc = ElementTree.parse(Path(__file__).resolve().parents[1] / 'resources/viper4linux-icons.qrc')
icons = {x.attrib['alias']:x.text for x in qrc.iter('file')}
for name in ('combo-chevron-light.png', 'combo-chevron-dark.png'):
    assert name in icons
    img = Path(__file__).resolve().parents[1] / 'resources' / icons[name]
    assert img.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), img
print('PASS: all combo boxes use the r26 Cinnamon-approved rounded cap, explicit chevron, both palette icons')

assert 'QComboBox#viperThemeSelector::' not in source, 'No Theme-only cap: all combos should share rounded arrows'
# All application combo instances are created in the main window, including
# the special IRS library combo with its own popup update logic.
assert 'class IrsComboBox final : public QComboBox' in source
assert 'out.convolver_library = new IrsComboBox(' in source
assert source.count('new QComboBox(') >= 12
