#!/usr/bin/env python3
"""Static native-effect routing audit, not a substitute for native audio tests."""
from pathlib import Path
import re
root = Path(__file__).resolve().parents[1]
s = (root / "src/viper-ladspa.cpp").read_text()
expected = {
 "Bass": "ApplyBass", "Mono Bass": "ApplyBassMono",
 "Psychoacoustic Bass": "ApplyPsychoacousticBass",
 "Dynamic System": "ApplyDynamicSystem", "Clarity": "ApplyClarity",
 "Spectrum Extension": "ApplySpectrumExtension",
 "Headphone Surround+": "ApplyHeadphoneSurround",
 "Differential Surround": "ApplyDiffSurround",
 "Field Surround": "ApplyFieldSurround", "Reverberation": "ApplyReverb",
 "Graphic Equalizer": "ApplyEqualizer", "Tube Simulator": "ApplyTubeSimulator",
 "AnalogX": "ApplyAnalogX", "Crossfeed": "ApplyCure", "Dynamic EQ": "ApplyDynamicEq",
 "Convolver": "ApplyConvolver",
 "Master Limiter": "ApplyMasterLimiter",
 "Playback Gain Control (AGC)": "ApplyPlaybackGainControl",
 "LUFS Targeting": "ApplyLufs",
}
for display,method in expected.items():
    assert f"wet.{method}(" in s, f"{display} does not call native ViPERDSP"
    assert method in (root / "tests/mock/viper/ViPER.h").read_text(), display
assert "wet.Process(wet_audio, n);" in s
assert "dry.Process(dry_audio, n);" in s
assert "dry.ApplyMasterLimiter(" in s
assert "LoadConvolverKernel(" in s
assert "viper_ir::resample_to(" in s
assert "* safe_attenuation" not in s
assert "input_permille" not in s
assert "InputScale" not in s
assert "effect_replacements" not in s
assert "std::isfinite(in_l[ix]) ? in_l[ix] : 0.0f" in s
assert "std::isfinite(in_r[ix]) ? in_r[ix] : 0.0f" in s
print(f"PASS: {len(expected)} named native effect dispatches, ViPERDSP wet/dry paths, unity input and no replacement effects")
