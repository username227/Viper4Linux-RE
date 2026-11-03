#!/usr/bin/env python3
"""Source-level guard for the experimental one-way Android preset exporter.

This does not replace the real-world test: import a generated JSON file into
likelikeslike's Android GUI and inspect the resulting effect parameters.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
adapter = (root / "src/viper-android-export.h").read_text()
gui = (root / "src/viper-gui.cpp").read_text()

assert '#include "viper-android-export.h"' in gui
assert "Export for ViPER4Android…" in gui
assert "viper_android_export::convert(source, &android, &warning, &error)" in gui
assert "QSaveFile output(destination);" in gui
assert "QJsonDocument(android).toJson(QJsonDocument::Indented)" in gui
assert 'json["schemaVersion"] = 2.1;' in adapter
for key in (
    "masterLimiter", "playbackGainControl", "lufs", "bass", "bassMono",
    "clarity", "spectrumExtension", "equalizer", "dynamicEq",
    "headphoneSurround", "diffSurround", "stereoImager", "reverb",
    "tubeSimulator", "analogX", "convolver", "ddc",
):
    assert f'json["{key}"]' in adapter, key
for value in ("eq_levels_", "hundredths) / 100.0", "deq_b%1_", "crossChannel"):
    assert value in adapter
assert "preset.clear()" not in adapter
assert "settings_.setValue" not in adapter
assert "QSaveFile" in gui
print("PASS: Experimental one-way Android exporter UI and read-only schema contract")
