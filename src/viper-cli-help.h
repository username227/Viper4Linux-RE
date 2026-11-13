#pragma once
// Human-readable descriptions for public CLI options.
// Numeric ranges and validation remain in viper-cli-batch.h.
#include <string>
namespace viperbass::cli_help {
struct OptionDescription { const char* command; const char* description; };
inline constexpr OptionDescription descriptions[] = {
    {"agc-strength", "AGC strength"},
    {"agc-max", "Maximum AGC gain"},
    {"agc-threshold", "AGC threshold (linear multiplier)"},
    {"analogx-mode", "AnalogX intensity: 0 Mild, 1 Medium, 2 Strong"},
    {"bass-mode", "Bass mode: 0 Natural, 1 Pure Bass+, 2 Subwoofer"},
    {"bass-gain", "Bass gain (stored to hundredths)"},
    {"bass-freq", "Bass frequency (Hz, rounded to whole Hz)"},
    {"clarity-gain", "Clarity gain (stored to thousandths)"},
    {"clarity-mode", "Clarity mode: 0 Natural, 1 Ozone, 2 XHiFi"},
    {"convolver-cross", "Cross-channel convolution mix (%)"},
    {"crossfeed-mode", "Crossfeed preset: 0 Mild, 1 Medium, 2 Strong"},
    {"diff-delay", "Differential delay (ms)"},
    {"diff-reverse", "Delay routing: 0 normal, 1 reversed"},
    {"diff-mix", "Delayed-channel mix (%)"},
    {"diff-lowpass", "Low-pass cutoff (Hz; 0 disables low-pass)"},
    {"dynamic-x-low", "Dynamic System low X parameter"},
    {"dynamic-x-high", "Dynamic System high X parameter"},
    {"dynamic-y-low", "Dynamic System low Y parameter"},
    {"dynamic-y-high", "Dynamic System high Y parameter"},
    {"dynamic-side-low", "Low-side contribution (%)"},
    {"dynamic-side-high", "High-side contribution (%)"},
    {"dynamic-strength", "Dynamic System strength (%)"},
    {"eq-bands", "Graphic EQ layout (10, 15, 25 or 31 bands)"},
    {"eq-level", "Band BAND gain in dB (-12 to +12; band is 1-based)"},
    {"eq-preset", "Apply named EQ preset (quote names with spaces)"},
    {"fet-threshold", "Compression threshold (dB)"},
    {"fet-ratio", "Compression ratio (1:1 to 10:1)"},
    {"field-widen", "Field Surround widening factor"},
    {"field-mid", "Field Surround mid-image factor"},
    {"field-depth", "Depth: 0 disables; 200..950 adjusts depth"},
    {"headphone-level", "Headphone Surround+ level (matches GUI numbering)"},
    {"imager-low", "Low-band stereo width (%; 100 unchanged)"},
    {"imager-mid", "Mid-band stereo width (%; 100 unchanged)"},
    {"imager-high", "High-band stereo width (%; 100 unchanged)"},
    {"imager-xover-low", "Low crossover frequency (Hz)"},
    {"imager-xover-high", "High crossover frequency (Hz)"},
    {"limiter-threshold", "Master limiter threshold (linear multiplier)"},
    {"limiter-gain", "Master output gain (1 = unity)"},
    {"limiter-pan", "Output pan: -1 left, 0 center, +1 right"},
    {"lufs-target", "Target loudness (LUFS)"},
    {"lufs-max", "Maximum gain adjustment (dB)"},
    {"lufs-speed", "Adaptation: 0 Slow, 1 Normal, 2 Fast"},
    {"mb-bands", "Compressor band count (3 or 5)"},
    {"mono-mode", "Bass Mono mode: 0 Natural, 1 Pure Bass+, 2 Subwoofer"},
    {"mono-strength", "Bass Mono strength (stored to hundredths)"},
    {"mono-freq", "Bass Mono frequency (Hz)"},
    {"psycho-cutoff", "Psychoacoustic Bass cutoff (Hz)"},
    {"psycho-intensity", "Psychoacoustic Bass intensity (%)"},
    {"psycho-order", "Harmonic order"},
    {"psycho-original", "Original (unprocessed) signal retained (%)"},
    {"reverb-room", "Reverberation room size (%)"},
    {"reverb-width", "Reverberation stereo width (%)"},
    {"reverb-damp", "Reverberation damping (%)"},
    {"reverb-wet", "Processed (wet) signal mix (%)"},
    {"reverb-dry", "Unprocessed (dry) signal mix (%)"},
    {"spectrum-freq", "Spectrum reference frequency (Hz)"},
    {"spectrum-exciter", "Harmonic exciter strength"},
    {"tube-model", "Tube type: 0 12AX7, 1 12AU7, 2 6N1P"},
    {"tube-drive", "Tube drive (%)"},
    {"tube-mix", "Tube mix (%)"},
};
inline const char* descriptionFor(const std::string& command) noexcept {
    for (const auto& entry : descriptions)
        if (command == entry.command) return entry.description;
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
} // namespace viperbass::cli_help
