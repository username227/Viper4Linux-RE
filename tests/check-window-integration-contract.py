#!/usr/bin/env python3
"""Keep only GUI initialization order that compilation alone cannot verify."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "src/viper-gui.cpp").read_text()

# Saved values must load before signal wiring to avoid spurious DSP edits.
assert (main.index("loadSavedEffectSettings();")
        < main.index("connectEffectSignals(eq_reset, deq_spread);")
        < main.index("setupThemeMonitoring();"))

# Correction libraries can only be wired after their widgets are constructed.
assert (main.index('addEffectPanel(correctionLayout, correctionDdc.group, "ddc", ddc_enabled_)')
        < main.index("setupDdcLibrary(importDdc, refreshDdc);"))
assert (main.index('addEffectPanel(correctionLayout, correctionConvolver.group, "convolver", convolver_enabled_)')
        < main.index("setupIrsLibrary(browse, refresh);"))
print("PASS: GUI restore and correction-library setup order")
