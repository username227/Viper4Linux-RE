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
inline bool normalize(int argc, char* const* argv, std::vector<std::string>& result,
                      std::string& error) {
    result.clear();
    error.clear();
    std::string scope;
    bool scopeHasOption = true;
    for (int i = 0; i < argc; ++i) {
        const std::string word(argv[i]);
        if (word == "--on" || word == "--off") {
            result.push_back(word.substr(2)); // Always master, even inside a scope.
            continue;
        }
        if (word.size() > 2 && word.compare(0, 2, "--") == 0) {
            if (const auto* effect = findEffect(word.substr(2))) {
                if (!scopeHasOption) {
                    error = "Effect --" + scope + " requires an option";
                    result.clear();
                    return false;
                }
                scope = effect->name;
                scopeHasOption = false;
                continue;
            }
        }
        if (word == "--status" || word == "status") {
            result.emplace_back("status");
            // A plain 'status' inside an effect scope means that effect; the
            // explicitly global '--status' requires an explicit effect argument.
            const bool implicit = word == "status" && !scope.empty();
            std::string target = implicit ? scope : "";
            bool expanded = false;
            for (int j = 0; j < 2 && i + 1 < argc; ++j) {
                const std::string next(argv[i + 1]);
                if (next == "--full" && !expanded) {
                    expanded = true;
                    ++i;
                } else if (!implicit && target.empty() && findEffect(next)) {
                    target = next;
                    ++i;
                } else break;
            }
            if (implicit) scopeHasOption = true;
            if (!target.empty()) result.push_back(target);
            if (expanded) result.emplace_back("--full");
            continue;
        }
        if (!scope.empty() && (word == "on" || word == "off" ||
                                (scope == "bass" && word == "toggle") ||
                                (scope == "eq" && word == "presets"))) {
            result.push_back(scope + "-" + word);
            scopeHasOption = true;
            continue;
        }
        if (!scope.empty()) {
            const auto equals = word.find('=');
            if (equals != std::string::npos) {
                const std::string option = word.substr(0, equals);
                const std::string value = word.substr(equals + 1);
                if (option.empty() || value.empty()) {
                    error = "Expected option=value after --" + scope;
                    result.clear();
                    return false;
                }
                if (scope == "eq" && option == "level") {
                    const auto colon = value.find(':');
                    if (colon == std::string::npos || colon == 0 ||
                        colon + 1 >= value.size()) {
                        error = "EQ level requires level=BAND:DB (e.g. level=2:-1.5)";
                        result.clear();
                        return false;
                    }
                    result.push_back("eq-level");
                    result.push_back(value.substr(0, colon));
                    result.push_back(value.substr(colon + 1));
                    scopeHasOption = true;
                    continue;
                }
                result.push_back(scope + "-" + option);
                result.push_back(value);
                scopeHasOption = true;
                continue;
            }
        }
        // Legacy commands and positional values are retained unchanged.
        result.push_back(word);
    }
    if (!scopeHasOption) {
        error = "Effect --" + scope + " requires an option";
        result.clear();
        return false;
    }
    return true;
}
} // namespace viperbass::cli_scope
