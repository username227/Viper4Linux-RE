#!/usr/bin/env python3
"""Only guard GUI behaviors not exercised by native/audio/output tests."""
from pathlib import Path
import re
from gui_source_contract import read_gui_sources

root = Path(__file__).resolve().parents[1]
source = read_gui_sources(root)

# A missing effect panel is user-visible even when its native DSP still builds.
cards = re.findall(r'addEffectPanel\([^;]+?,\s*"([a-z_]+)"\s*,', source)
assert len(cards) >= 24 and len(cards) == len(set(cards)), cards

# Retain the existing GUI switch-connection guards: native mock tests do not
# prove that the actual GUI buttons reach the corresponding DSP controls.
required = {
    'Bass': 'enabled_',
    'Clarity': 'clarity_enabled_',
    'Spectrum': 'spectrum_enabled_',
    'Headphone Surround+': 'headphone_enabled_',
    'Differential Surround': 'diff_enabled_',
    'Field Surround': 'field_enabled_',
    'Reverberation': 'reverb_enabled_',
}
for title, variable in required.items():
    field = variable.rstrip('_')
    assert re.search(r'\b(?:out\.)?' + re.escape(field) + r'_?\s*=\s*new EffectSwitch\(', source), variable
    assert f'connect({variable}, &QCheckBox::toggled' in source, variable
for title,control in [('ViPER Bass Mono','mono_enabled_'),('Psychoacoustic Bass','psycho_enabled_'),('Dynamic System','dyn_enabled_')]:
    field = control.rstrip('_')
    assert re.search(r'\b(?:out\.)?' + re.escape(field) + r'_?\s*=\s*new EffectSwitch\(', source), control
    assert f'connect({control}, &QCheckBox::toggled' in source

# Opening and closing an effect card must not change audio settings.
handler = re.search(
    r'connect\(expand, &QToolButton::toggled, this, \[this, group, expand, key\]\(bool expanded\) \{(.*?)\n        \}\);',
    source, re.S)
assert handler, "Effect cards must have a collapse handler"
body = "\n".join(line.split("//", 1)[0] for line in handler.group(1).splitlines())
for forbidden in ("push(", "publishToBackend(", "setChecked(",
                  "enable->", "persistCurrentSettings("):
    assert forbidden not in body, forbidden

# Real native mock tests check IR loading. Keep the GUI-to-worker entry points.
assert "convolver_request_serial.fetch_add(1" in source
assert "QFileSystemWatcher::directoryChanged" in source
assert "viper_library_import::copyOrSelect(selectedFile, irsDirectory())" in source

# The selected EQ preset must survive initial widget population.
assert source.index('eq_selected_preset_ = settings_.value("eq/selected_preset"') < source.index("populateEqPresets();")

# Backend-to-GUI readback must not publish stale UI values over CLI edits.
assert "const auto before = liveUiSnapshot();" in source
assert "const QSignalBlocker blockDeq(deq_enabled_);" in source
assert "const QSignalBlocker blockDdc(ddc_enabled_), blockSpeaker(speaker_enabled_);" in source
assert "const int beforeNativeLowX = dyn_xlo_native_;" in source
assert "if (before != liveUiSnapshot() || beforeNativeLowX != dyn_xlo_native_) persistCurrentSettings();" in source
assert "if (before != liveUiSnapshot()) publishToBackend();" not in source

# A connected but silent backend must not be presented as active audio.
bridge = (root / "src/sync/gui-backend-bridge.inc").read_text()
assert "Connected — no audio signal (DSP ready)" in bridge
print("PASS: effect panels, non-mutating collapse, IR wiring, EQ restore, safe GUI readback")
