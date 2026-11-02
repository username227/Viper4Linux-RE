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
    assert(check({"on", "bass-on", "psycho-off"}).count == 3);
    assert(check({"bass-gain", "2.5", "bass-freq", "75", "headphone-off"}).count == 3);
    assert(check({"eq-bands", "15", "eq-level", "12", "-1.5"}).ok);
    assert(!check({"eq-bands", "10", "eq-level", "12", "-1.5"}).ok);
    assert(!check({"on", "bass-gain", "not-a-number"}).ok);
    assert(!check({"on", "gain", "2.5"}).ok);
    assert(!check({"on", "headphone-level", "2.5"}).ok);
    assert(!check({"on", "tube-drive", "12.5"}).ok);
    assert(!check({"off", "field-depth", "100"}).ok);
    assert(!check({"on", "bass-gain"}).ok);
    assert(check({"status", "--full", "bass-on"}).count == 2);
    assert(check({"eq-preset", "Bass Booster", "headphone-on"}).count == 2);
    assert(check({"eq-level", "3", "1.75"}, 10).count == 1);
    assert(!check({"eq-level", "31", "1.0"}, 15).ok);
    assert(!check({"eq-bands", "17"}).ok);
    assert(!check({"mb-bands", "4"}).ok);
    assert(!check({"invalid-command"}).ok);
    std::cout << "PASS: CLI multi-command argument grouping and preflight\n";
}
