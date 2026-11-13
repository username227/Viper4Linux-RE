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
    float min = 0;
    float max = 0;
};

inline constexpr ArgSpec specs[] = {
        {"bass-mode",1,Kind::Whole,0,2}, {"bass-gain",1,Kind::Decimal,0.5f,10},
        {"bass-freq",1,Kind::Decimal,15,150},
        {"mono-mode",1,Kind::Decimal,0,2}, {"mono-strength",1,Kind::Decimal,0.5f,10},
        {"mono-freq",1,Kind::Decimal,15,150},
        {"psycho-cutoff",1,Kind::Decimal,60,150}, {"psycho-intensity",1,Kind::Decimal,0,100},
        {"psycho-order",1,Kind::Decimal,2,5}, {"psycho-original",1,Kind::Decimal,0,100},
        {"dynamic-x-low",1,Kind::Decimal,0,2400}, {"dynamic-x-high",1,Kind::Decimal,0,12000},
        {"dynamic-y-low",1,Kind::Decimal,0,200}, {"dynamic-y-high",1,Kind::Decimal,0,300},
        {"dynamic-side-low",1,Kind::Decimal,0,100}, {"dynamic-side-high",1,Kind::Decimal,0,100},
        {"dynamic-strength",1,Kind::Decimal,0,100},
        {"eq-bands",1,Kind::EqBands,10,31}, {"eq-level",2,Kind::EqLevel,-12,12},
        {"eq-preset",1,Kind::Name},
        {"clarity-gain",1,Kind::Decimal,0,4.5f}, {"clarity-mode",1,Kind::Decimal,0,2},
        {"spectrum-freq",1,Kind::Decimal,2200,8200}, {"spectrum-exciter",1,Kind::Decimal,0,6},
        {"tube-model",1,Kind::Whole,0,2}, {"tube-drive",1,Kind::Whole,0,100},
        {"tube-mix",1,Kind::Whole,0,100}, {"analogx-mode",1,Kind::Whole,0,2},
        {"crossfeed-mode",1,Kind::Whole,0,2},
        {"headphone-level",1,Kind::Whole,1,5},
        {"diff-delay",1,Kind::Decimal,1,20}, {"diff-reverse",1,Kind::Whole,0,1},
        {"diff-mix",1,Kind::Decimal,0,100}, {"diff-lowpass",1,Kind::Decimal,0,20000},
        {"field-widen",1,Kind::Decimal,0,8}, {"field-mid",1,Kind::Decimal,1,2},
        {"field-depth",1,Kind::Decimal,0,950},
        {"reverb-room",1,Kind::Decimal,0,100}, {"reverb-width",1,Kind::Decimal,0,100},
        {"reverb-damp",1,Kind::Decimal,0,100}, {"reverb-wet",1,Kind::Decimal,0,100},
        {"reverb-dry",1,Kind::Decimal,0,100},
        {"imager-low",1,Kind::Whole,0,200}, {"imager-mid",1,Kind::Whole,0,200},
        {"imager-high",1,Kind::Whole,0,200}, {"imager-xover-low",1,Kind::Whole,80,400},
        {"imager-xover-high",1,Kind::Whole,2000,8000},
        {"fet-threshold",1,Kind::Whole,-48,0}, {"fet-ratio",1,Kind::Decimal,1,10},
        {"mb-bands",1,Kind::MbBands,3,5},
        {"convolver-cross",1,Kind::Decimal,0,100},
        {"limiter-threshold",1,Kind::Decimal,0.3f,1}, {"limiter-gain",1,Kind::Decimal,0.01f,2},
        {"limiter-pan",1,Kind::Decimal,-1,1},
        {"agc-strength",1,Kind::Decimal,0.5f,3}, {"agc-max",1,Kind::Decimal,1,10},
        {"agc-threshold",1,Kind::Decimal,0.3f,1},
        {"lufs-target",1,Kind::Decimal,-24,-8}, {"lufs-max",1,Kind::Decimal,0,12},
        {"lufs-speed",1,Kind::Whole,0,2},
};
inline const ArgSpec* withArgs(const std::string& cmd) {
    for (const auto& spec : specs) if (cmd == spec.command) return &spec;
    return nullptr;
}
inline bool noArgs(const std::string& cmd) {
    static const char* const commands[] = {
        "on", "off", "all-on", "all-off", "status", "eq-presets",
        "bass-on", "bass-off", "bass-toggle", "mono-on", "mono-off",
        "psycho-on", "psycho-off", "dynamic-on", "dynamic-off",
        "eq-on", "eq-off", "deq-on", "deq-off", "clarity-on", "clarity-off",
        "spectrum-on", "spectrum-off", "tube-on", "tube-off",
        "analogx-on", "analogx-off", "crossfeed-on", "crossfeed-off",
        "headphone-on", "headphone-off", "diff-on", "diff-off",
        "field-on", "field-off", "reverb-on", "reverb-off",
        "imager-on", "imager-off", "fet-on", "fet-off", "mb-on", "mb-off",
        "ddc-on", "ddc-off", "speaker-on", "speaker-off",
        "convolver-on", "convolver-off", "agc-on", "agc-off", "lufs-on", "lufs-off"
    };
    for (const char* entry : commands) if (cmd == entry) return true;
    return false;
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
// argv starts at the first command (not at executable name).
inline bool parseLegacy(int count, char* const* argv, int currentEqBands,
                        std::vector<Command>& output, std::string& error) {
    output.clear();
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
                if (i + needed < count && std::string(argv[i + needed]) == "--full")
                    ++needed;
            }
            if (count - i < needed)
                throw std::invalid_argument("missing argument for '" + cmd.args[0] + "'");
            for (int j = 0; j < needed; ++j) cmd.args.emplace_back(argv[i++]);
            if (spec) checkValue(*spec, cmd, bands);
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
// Normalize the new --effect grammar into the unchanged, fully validated
// legacy operations. Nothing is executed until the entire batch is valid.
inline bool parse(int count, char* const* argv, int currentEqBands,
                  std::vector<Command>& output, std::string& error) {
    // Only no-argument legacy aliases remain. Value-bearing commands use
    // --effect option=value; positional numbers are never accepted.
    for (int i = 0; i < count; ++i) {
        if (withArgs(argv[i])) {
            error = "Value-taking legacy command '" + std::string(argv[i]) +
                    "' is not supported; use --effect option=value";
            output.clear();
            return false;
        }
    }
    std::vector<std::string> normalized;
    if (!cli_scope::normalize(count, argv, normalized, error)) {
        output.clear();
        return false;
    }
    std::vector<char*> arguments;
    arguments.reserve(normalized.size());
    for (auto& word : normalized) arguments.push_back(word.data());
    return parseLegacy(static_cast<int>(arguments.size()), arguments.data(),
                       currentEqBands, output, error);
}
} // namespace viperbass::cli_batch
