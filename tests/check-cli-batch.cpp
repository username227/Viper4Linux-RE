// Validate command batching and full preflight, without starting a real backend.
#include "viper-cli-batch.h"
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

struct TestResult { bool ok; std::size_t count; std::string error; };
static TestResult check(std::initializer_list<const char*> input, int bands = 10) {
    std::vector<std::string> values(input.begin(), input.end());
    std::vector<char*> argv;
    for (auto& v : values) argv.push_back(v.data());
    std::vector<viperbass::cli_batch::Command> parsed;
    std::string error;
    bool ok = viperbass::cli_batch::parse(static_cast<int>(argv.size()),
                                           argv.data(), bands, parsed, error);
    return {ok, parsed.size(), error};
}
int main() {
    assert(!check({"--bass"}).ok);
    assert(!check({"--bass", "--headphone", "on"}).ok);
    assert(check({"--on", "--bass", "on", "gain=3", "freq=60",
                  "--headphone", "on", "level=2"}).count == 6);
    assert(check({"on", "--bass", "on", "gain=3", "--headphone", "on", "level=2"}).count == 5);
    assert(check({"--bass", "on", "--on", "gain=3", "--off"}).count == 4);
    assert(check({"--bass", "status", "--full"}).count == 1);
    assert(check({"--status", "bass", "--full", "--on"}).count == 2);
    assert(!check({"status", "bass", "--full", "--on"}).ok);
    assert(check({"--status", "bass", "--on"}).count == 2);
    assert(check({"--eq", "bands=15", "level=2:-1.75",
                  "level=5:2.5", "level=9:-3"}).count == 4);
    assert(!check({"--eq", "bands=15", "level=2:-1.75",
                   "level=16:1"}).ok);
    assert(!check({"--eq", "bands=10", "level=12:-1.5"}).ok);
    assert(!check({"--bass", "gain="}).ok);
    assert(!check({"--bass", "gain=99", "--headphone", "on"}).ok);
    assert(!check({"--headphone", "level=2.5"}).ok);
    assert(!check({"--eq", "level=3"}).ok);
    assert(!check({"--limiter", "on"}).ok);
    assert(check({"on", "bass-on", "psycho-off"}).count == 3);
    assert(check({"bass-on", "analogx-off"}).count == 2);
    // Modern global commands and selectors may be chained, but not combined
    // with legacy effect switches. Initial bare master on/off is the exception.
    assert(check({"on", "--bass", "on"}).count == 2);
    assert(check({"off", "--headphone", "on"}).count == 2);
    assert(check({"--bass", "on", "--on", "gain=3", "--off"}).count == 4);
    assert(!check({"bass-on", "--analogx", "off"}).ok);
    assert(!check({"--bass", "on", "analogx-off"}).ok);
    assert(!check({"bass-toggle", "--on"}).ok);
    assert(!check({"--on", "bass-on"}).ok);
    assert(!check({"--status", "bass", "bass-on"}).ok);
    assert(!check({"bass-on", "--status", "bass"}).ok);
    assert(!check({"status", "--full", "--bass", "on"}).ok);
    assert(!check({"eq-presets", "--eq", "on"}).ok);
    assert(!check({"bass-gain", "2.5", "bass-freq", "75", "headphone-off"}).ok);
    assert(!check({"--bass", "gain=2.5", "freq=75", "headphone-off"}).ok);
    assert(!check({"eq-bands", "15", "eq-level", "12", "-1.5"}).ok);
    assert(check({"--eq", "bands=15", "level=12:-1.5"}).ok);
    assert(!check({"eq-bands", "10", "eq-level", "12", "-1.5"}).ok);
    assert(!check({"on", "bass-gain", "not-a-number"}).ok);
    assert(!check({"on", "gain", "2.5"}).ok);
    assert(!check({"toggle"}).ok);
    assert(check({"bass-toggle"}).ok);
    assert(!check({"on", "headphone-level", "2.5"}).ok);
    assert(!check({"on", "headphone-quality", "2"}).ok);
    assert(!check({"on", "tube-drive", "12.5"}).ok);
    assert(!check({"off", "field-depth", "100"}).ok);
    assert(!check({"on", "bass-gain"}).ok);
    assert(check({"status", "--full", "bass-on"}).count == 2);
    assert(!check({"eq-preset", "Bass Booster", "headphone-on"}).ok);
    assert(!check({"--eq", "preset=Bass Booster", "headphone-on"}).ok);
    assert(!check({"eq-level", "3", "1.75"}, 10).ok);
    assert(check({"--eq", "level=3:1.75"}, 10).count == 1);
    assert(!check({"eq-level", "31", "1.0"}, 15).ok);
    assert(!check({"eq-bands", "17"}).ok);
    assert(!check({"mb-bands", "4"}).ok);
    assert(!check({"invalid-command"}).ok);
    // A bare number is never an argument, even for formerly supported commands.
    assert(!check({"bass-gain", "3"}).ok);
    assert(!check({"--bass", "gain", "3"}).ok);
    assert(!check({"--bass", "gain", "=", "3"}).ok);
    assert(!check({"--bass", "gain=", "3"}).ok);
    assert(!check({"--bass", "on", "bass-gain", "3"}).ok);
    assert(!check({"--headphone", "level", "2"}).ok);
    assert(!check({"--eq", "level", "2", "-1.5"}).ok);
    // Every effect selector is mapped into its established command family.
    for (const auto& effect : viperbass::cli_scope::effects) {
        const std::string selector = std::string("--") + effect.name;
        assert(check({selector.c_str(), "status"}).ok);
        assert(check({"--status", effect.name}).ok);
        if (selector != "--limiter") {
            assert(check({selector.c_str(), "on", "off"}).count == 2);
        } else {
            assert(!check({selector.c_str(), "on"}).ok);
        }
    }
    // Every numeric argument already registered in the legacy validator can
    // be written under its selector with option=value (no duplicated ranges).
    for (const auto& spec : viperbass::cli_batch::specs) {
        const std::string legacy = spec.command;
        const auto dash = legacy.find('-');
        const std::string group = legacy.substr(0, dash);
        const std::string option = legacy.substr(dash + 1);
        std::string value;
        if (spec.kind == viperbass::cli_batch::Kind::Name) value = "Acoustic";
        else if (spec.kind == viperbass::cli_batch::Kind::EqLevel) value = "2:-1.5";
        else if (spec.kind == viperbass::cli_batch::Kind::EqBands) value = "15";
        else if (spec.kind == viperbass::cli_batch::Kind::MbBands) value = "3";
        else value = std::to_string(spec.min);
        const std::string selector = "--" + group;
        const std::string argument = option + "=" + value;
        assert(check({selector.c_str(), argument.c_str()}).ok);
    }

    std::cout << "PASS: CLI multi-command argument grouping and preflight\n";
}
