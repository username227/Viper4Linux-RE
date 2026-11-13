#pragma once
// Accept scoped Linux-style CLI syntax and translate it into the established
// legacy command stream. The existing handlers and validation remain authoritative.
// No IPC or settings access occurs during normalization.
#include <string>
#include <vector>

namespace viperbass::cli_scope {
struct EffectName { const char* name; const char* title; };
inline constexpr EffectName effects[] = {
    {"agc", "Playback Gain Control"}, {"analogx", "AnalogX"},
    {"bass", "ViPER Bass"}, {"clarity", "ViPER Clarity"},
    {"convolver", "Convolver"}, {"crossfeed", "Crossfeed"},
    {"ddc", "ViPER-DDC"}, {"deq", "Dynamic EQ"},
    {"diff", "Differential Surround"}, {"dynamic", "Dynamic System"},
    {"eq", "Graphic Equalizer"}, {"fet", "FET Compressor"},
    {"field", "Field Surround"}, {"headphone", "Headphone Surround+"},
    {"imager", "Stereo Imager"}, {"limiter", "Master Limiter"},
    {"lufs", "LUFS Targeting"}, {"mb", "Multiband Compressor"},
    {"mono", "ViPER Bass Mono"}, {"psycho", "Psychoacoustic Bass"},
    {"reverb", "Reverberation"}, {"speaker", "Speaker Correction"},
    {"spectrum", "Spectrum Extension"}, {"tube", "Tube Simulator"},
};
inline const EffectName* findEffect(const std::string& name) noexcept {
    for (const auto& entry : effects)
        if (name == entry.name) return &entry;
    return nullptr;
}
inline const char* effectNote(const std::string& name) noexcept {
    if (name == "deq") return "Configure individual Dynamic EQ bands in the GUI.";
    if (name == "mb") return "Individual compressor band settings are configured in the GUI.";
    if (name == "ddc") return "Choose a VDC profile in the GUI; a valid sample rate is required.";
    if (name == "convolver") return "Import and select an IRS or WAV impulse response in the GUI.";
    if (name == "eq") return "Preset names with spaces must be quoted as one argument.";
    if (name == "limiter") return "The limiter has no independent on/off; use master --on or --off.";
    return nullptr;
}
} // namespace viperbass::cli_scope
