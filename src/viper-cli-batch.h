#pragma once
// Preflight parsing for multi-command CLI invocations.  This header never
// changes shared memory or QSettings.  All commands are checked before the
// caller performs the first edit.  Runtime I/O errors can still interrupt a
// batch after an earlier command succeeded.
#include <algorithm>
#include <utility>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>
#include "viper-cli-scope.h"

namespace viperbass::cli_batch {
struct Command {
    std::vector<std::string> args; // command followed by its own parameters
    int eqBandsAfter = 10;
};
enum class Kind { Decimal, Whole, EqBands, MbBands, Name, EqLevel };
struct ArgSpec {
    const char* command;
    int count;
    Kind kind;
    const char* description;
    float min = 0;
    float max = 0;
};

inline constexpr ArgSpec specs[] = {
        {"bass-mode",1,Kind::Whole,"Bass mode: 0 Natural, 1 Pure Bass+, 2 Subwoofer",0,2}, {"bass-gain",1,Kind::Decimal,"Bass gain (stored to hundredths)",0.5f,10},
        {"bass-freq",1,Kind::Decimal,"Bass frequency (Hz, rounded to whole Hz)",15,150},
        {"mono-mode",1,Kind::Whole,"Bass Mono mode: 0 Natural, 1 Pure Bass+, 2 Subwoofer",0,2}, {"mono-strength",1,Kind::Decimal,"Bass Mono strength (stored to hundredths)",0.5f,10},
        {"mono-freq",1,Kind::Decimal,"Bass Mono frequency (Hz)",15,150},
        {"psycho-cutoff",1,Kind::Decimal,"Psychoacoustic Bass cutoff (Hz)",60,150}, {"psycho-intensity",1,Kind::Decimal,"Psychoacoustic Bass intensity (%)",0,100},
        {"psycho-order",1,Kind::Whole,"Harmonic order",2,5}, {"psycho-original",1,Kind::Decimal,"Original (unprocessed) signal retained (%)",0,100},
        {"dynamic-x-low",1,Kind::Decimal,"Dynamic System low X parameter",0,2400}, {"dynamic-x-high",1,Kind::Decimal,"Dynamic System high X parameter",0,12000},
        {"dynamic-y-low",1,Kind::Decimal,"Dynamic System low Y parameter",0,200}, {"dynamic-y-high",1,Kind::Decimal,"Dynamic System high Y parameter",0,300},
        {"dynamic-side-low",1,Kind::Decimal,"Low-side contribution (%)",0,100}, {"dynamic-side-high",1,Kind::Decimal,"High-side contribution (%)",0,100},
        {"dynamic-strength",1,Kind::Decimal,"Dynamic System strength (%)",0,100},
        {"eq-bands",1,Kind::EqBands,"Graphic EQ layout (10/15/25/31; restores independent curve or selected preset)",10,31}, {"eq-level",2,Kind::EqLevel,"Band BAND gain in dB (-12 to +12; band is 1-based)",-12,12},
        {"eq-preset",1,Kind::Name,"Apply named EQ preset (quote names with spaces)"},
        {"clarity-gain",1,Kind::Decimal,"Clarity gain (stored to thousandths)",0,4.5f}, {"clarity-mode",1,Kind::Whole,"Clarity mode: 0 Natural, 1 Ozone, 2 XHiFi",0,2},
        {"spectrum-freq",1,Kind::Decimal,"Spectrum reference frequency (Hz)",2200,8200}, {"spectrum-exciter",1,Kind::Decimal,"Harmonic exciter strength",0,6},
        {"tube-model",1,Kind::Whole,"Tube type: 0 12AX7, 1 12AU7, 2 6N1P",0,2}, {"tube-drive",1,Kind::Whole,"Tube drive (%)",0,100},
        {"tube-mix",1,Kind::Whole,"Tube mix (%)",0,100}, {"analogx-mode",1,Kind::Whole,"AnalogX intensity: 0 Mild, 1 Medium, 2 Strong",0,2},
        {"crossfeed-mode",1,Kind::Whole,"Crossfeed preset: 0 Mild, 1 Medium, 2 Strong",0,2},
        {"headphone-level",1,Kind::Whole,"Headphone Surround+ level (matches GUI numbering)",1,5},
        {"diff-delay",1,Kind::Decimal,"Differential delay (ms)",1,20}, {"diff-reverse",1,Kind::Whole,"Delay routing: 0 normal, 1 reversed",0,1},
        {"diff-mix",1,Kind::Decimal,"Delayed-channel mix (%)",0,100}, {"diff-lowpass",1,Kind::Decimal,"Low-pass cutoff (Hz; 0 disables low-pass)",0,20000},
        {"field-widen",1,Kind::Decimal,"Field Surround widening factor",0,8}, {"field-mid",1,Kind::Decimal,"Field Surround mid-image factor",1,2},
        {"field-depth",1,Kind::Decimal,"Depth: 0 disables; 200..950 adjusts depth",0,950},
        {"reverb-room",1,Kind::Decimal,"Reverberation room size (%)",0,100}, {"reverb-width",1,Kind::Decimal,"Reverberation stereo width (%)",0,100},
        {"reverb-damp",1,Kind::Decimal,"Reverberation damping (%)",0,100}, {"reverb-wet",1,Kind::Decimal,"Processed (wet) signal mix (%)",0,100},
        {"reverb-dry",1,Kind::Decimal,"Unprocessed (dry) signal mix (%)",0,100},
        {"imager-low",1,Kind::Whole,"Low-band stereo width (%; 100 unchanged)",0,200}, {"imager-mid",1,Kind::Whole,"Mid-band stereo width (%; 100 unchanged)",0,200},
        {"imager-high",1,Kind::Whole,"High-band stereo width (%; 100 unchanged)",0,200}, {"imager-xover-low",1,Kind::Whole,"Low crossover frequency (Hz)",80,400},
        {"imager-xover-high",1,Kind::Whole,"High crossover frequency (Hz)",2000,8000},
        {"fet-threshold",1,Kind::Whole,"Compression threshold (dBFS)",-48,0}, {"fet-ratio",1,Kind::Decimal,"Compression ratio (1:1 to 10:1)",1,10},
        {"mb-bands",1,Kind::MbBands,"Compressor band count (3 or 5)",3,5},
        {"convolver-cross",1,Kind::Decimal,"Cross-channel convolution mix (%)",0,100},
        {"limiter-threshold",1,Kind::Decimal,"Master limiter threshold: negative/zero dBFS (GUI) or positive linear multiplier",-10.5f,1},
        {"limiter-threshold-db",1,Kind::Decimal,"Master limiter peak threshold (dBFS; matches GUI display)",-10.5f,0},
        {"limiter-gain",1,Kind::Decimal,"Master output gain (1 = unity)",0.01f,2},
        {"limiter-pan",1,Kind::Decimal,"Output pan: -1 left, 0 center, +1 right",-1,1},
        {"agc-strength",1,Kind::Decimal,"AGC strength",0.5f,3}, {"agc-max",1,Kind::Decimal,"Maximum AGC gain",1,10},
        {"agc-threshold",1,Kind::Decimal,"AGC threshold (linear multiplier)",0.3f,1},
        {"lufs-target",1,Kind::Decimal,"Target loudness (LUFS)",-24,-8}, {"lufs-max",1,Kind::Decimal,"Maximum gain adjustment (dB)",0,12},
        {"lufs-speed",1,Kind::Whole,"Adaptation: 0 Slow, 1 Normal, 2 Fast",0,2},
};
inline const ArgSpec* withArgs(const std::string& cmd) {
    for (const auto& spec : specs) if (cmd == spec.command) return &spec;
    return nullptr;
}
inline bool noArgs(const std::string& cmd) {
    if (cmd == "on" || cmd == "off" || cmd == "all-on" ||
        cmd == "all-off" || cmd == "status" || cmd == "eq-presets" ||
        cmd == "bass-toggle")
        return true;
    const auto dash = cmd.rfind('-');
    if (dash == std::string::npos) return false;
    const std::string action = cmd.substr(dash + 1);
    if (action != "on" && action != "off") return false;
    const std::string effect = cmd.substr(0, dash);
    return effect != "limiter" && cli_scope::findEffect(effect) != nullptr;
}
inline float decimal(const std::string& text, float low, float high) {
    std::size_t used = 0;
    const float value = std::stof(text, &used);
    if (used != text.size() || !std::isfinite(value) || value < low || value > high)
        throw std::invalid_argument("number outside permitted range");
    return value;
}
inline void checkValue(const ArgSpec& spec, const Command& cmd, int& eqBands) {
    if (spec.kind == Kind::Name) {
        if (cmd.args[1].empty()) throw std::invalid_argument("empty preset name");
        return;
    }
    if (spec.kind == Kind::EqLevel) {
        const float band = decimal(cmd.args[1], 1, 31);
        if (std::trunc(band) != band || band > eqBands)
            throw std::invalid_argument("EQ band must be a whole number within current layout");
        (void)decimal(cmd.args[2], -12, 12);
        return;
    }
    const float value = decimal(cmd.args[1], spec.min, spec.max);
    // threshold= supports GUI dBFS (-10.5..0) and native linear (0.3..1).
    // Zero is unambiguously 0 dBFS; the positive gap is not a valid value.
    if (cmd.args[0] == "limiter-threshold" && value > 0.f && value < 0.3f)
        throw std::invalid_argument("limiter threshold must be -10.5..0 dBFS or 0.3..1 linear");
    if (spec.kind == Kind::Whole && std::trunc(value) != value)
        throw std::invalid_argument("expected a whole number");
    if (spec.kind == Kind::EqBands) {
        if (value != 10 && value != 15 && value != 25 && value != 31)
            throw std::invalid_argument("EQ band count must be 10, 15, 25 or 31");
        eqBands = static_cast<int>(value);
    }
    if (spec.kind == Kind::MbBands && value != 3 && value != 5)
        throw std::invalid_argument("multiband count must be 3 or 5");
    if (cmd.args[0] == "field-depth" && value != 0 && value < 200)
        throw std::invalid_argument("field depth must be 0 or 200..950");
}
// Reconstruct the user's scoped spelling from the normalized command.
// Diagnostic-only: keep the parser's command and execution semantics intact.
inline std::string optionForError(const Command& cmd) {
    const auto separator = cmd.args[0].find('-');
    if (separator == std::string::npos || cmd.args.size() < 2)
        return cmd.args[0];
    std::string option = "--" + cmd.args[0].substr(0, separator) + " " +
        cmd.args[0].substr(separator + 1) + "=" + cmd.args[1];
    if (cmd.args[0] == "eq-level" && cmd.args.size() > 2)
        option += ":" + cmd.args[2];
    return option;
}

// Use the existing validation metadata to show the relevant limit, including
// special discrete choices and the limiter's two supported units.
inline std::string expectedForError(const ArgSpec& spec, int eqBands) {
    const std::string command(spec.command);
    if (spec.kind == Kind::EqBands) return "one of 10, 15, 25 or 31 bands";
    if (spec.kind == Kind::MbBands) return "3 or 5 bands";
    if (spec.kind == Kind::EqLevel)
        return "BAND:DB (BAND a whole number from 1 to " + std::to_string(eqBands) +
               "; DB from -12 to +12 dB)";
    if (spec.kind == Kind::Name) return "a non-empty preset name";
    if (command == "limiter-threshold")
        return "-10.5 to 0 dBFS or 0.3 to 1 linear";
    if (command == "limiter-threshold-db")
        return "-10.5 to 0 dBFS";
    if (command == "field-depth") return "0 or a number from 200 to 950";
    std::ostringstream range;
    range << (spec.kind == Kind::Whole ? "whole number from " : "number from ")
          << spec.min << " to " << spec.max;
    return range.str();
}

// argv starts at the first command (not at executable name).
inline bool parseLegacy(const std::vector<std::string>& argv, int currentEqBands,
                        std::vector<Command>& output, std::string& error) {
    output.clear();
    const int count = static_cast<int>(argv.size());
    int bands = (currentEqBands == 10 || currentEqBands == 15 ||
                 currentEqBands == 25 || currentEqBands == 31) ? currentEqBands : 10;
    try {
        for (int i = 0; i < count;) {
            Command cmd;
            cmd.args.emplace_back(argv[i++]);
            const auto* spec = withArgs(cmd.args[0]);
            if (!spec && !noArgs(cmd.args[0]))
                throw std::invalid_argument("unknown command '" + cmd.args[0] + "'");
            int needed = spec ? spec->count : 0;
            if (cmd.args[0] == "status") {
                // Normalized status operands are [effect] [--full].
                // Consume them together so a later command remains separate.
                if (i + needed < count && cli_scope::findEffect(argv[i + needed]))
                    ++needed;
                if (i + needed < count && argv[i + needed] == "--full")
                    ++needed;
            }
            if (count - i < needed)
                throw std::invalid_argument("missing argument for '" + cmd.args[0] + "'");
            for (int j = 0; j < needed; ++j) cmd.args.emplace_back(argv[i++]);
            if (spec) {
                try {
                    checkValue(*spec, cmd, bands);
                } catch (const std::exception&) {
                    error = optionForError(cmd) + "\n  Expected: " +
                            expectedForError(*spec, bands);
                    output.clear();
                    return false;
                }
            }
            cmd.eqBandsAfter = bands;
            output.push_back(std::move(cmd));
        }
    } catch (const std::exception& e) {
        error = e.what();
        output.clear();
        return false;
    }
    return true;
}
// Normalize and validate scoped commands with one grammar pass and one
// validation pass. The normalized strings retain storage until parsing ends.
inline bool normalize(int argc, char* const* argv, std::vector<std::string>& result,
                      std::string& error) {
    result.clear();
    error.clear();
    std::string scope;
    bool scopeHasOption = true;
    bool modern = false;
    bool legacy = false;
    for (int i = 0; i < argc; ++i) {
        const std::string word(argv[i]);
        // Check syntax compatibility while normalizing, not in a separate scan.
        if (withArgs(word)) {
            error = "Value-taking legacy command '" + word +
                    "' is not supported; use --effect option=value";
            result.clear();
            return false;
        }
        const bool selector = word.size() > 2 &&
            word.compare(0, 2, "--") == 0 &&
            cli_scope::findEffect(word.substr(2)) != nullptr;
        const bool global = word == "--on" || word == "--off" ||
                            word == "--status";
        const bool scopedOperation = !scope.empty() &&
            (word == "on" || word == "off" || word == "status");
        const bool legacyCommand = noArgs(word) && !scopedOperation &&
            word != "on" && word != "off";
        if ((selector || global) && legacy) {
            error = "Cannot mix legacy commands with scoped syntax";
            result.clear();
            return false;
        }
        if (legacyCommand && modern) {
            error = "Cannot mix legacy commands with scoped syntax";
            result.clear();
            return false;
        }
        modern = modern || selector || global;
        legacy = legacy || legacyCommand;
        if (word == "--on" || word == "--off") {
            result.push_back(word.substr(2)); // Always master, even inside a scope.
            continue;
        }
        if (selector) {
            if (!scopeHasOption) {
                error = "Effect --" + scope + " requires an option";
                result.clear();
                return false;
            }
            scope = word.substr(2);
            scopeHasOption = false;
            continue;
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
                } else if (!implicit && target.empty() && cli_scope::findEffect(next)) {
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

inline bool parse(int count, char* const* argv, int currentEqBands,
                  std::vector<Command>& output, std::string& error) {
    std::vector<std::string> normalized;
    if (!normalize(count, argv, normalized, error)) {
        output.clear();
        return false;
    }
    return parseLegacy(normalized, currentEqBands, output, error);
}
} // namespace viperbass::cli_batch
