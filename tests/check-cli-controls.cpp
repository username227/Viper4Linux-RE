// Exercise the real CLI parser against an isolated mock shared-memory channel.
// This process owns the test channel and never touches the user's live backend.
#define main viper_ctl_entry
#include "../src/viper-ctl.cpp"
#undef main

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static int invoke(std::initializer_list<const char*> args, std::string* output = nullptr) {
    std::vector<std::string> words = {"viper4linux-ctl"};
    for (const auto* arg : args) words.emplace_back(arg);
    std::vector<char*> argv;
    for (auto& word : words) argv.push_back(word.data());
    argv.push_back(nullptr);
    std::ostringstream captured, errors;
    auto* old_out = std::cout.rdbuf(captured.rdbuf());
    auto* old_err = std::cerr.rdbuf(errors.rdbuf());
    const int result = viper_ctl_entry(static_cast<int>(words.size()), argv.data());
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    if (output) *output = captured.str();
    return result;
}

static void run(const char* command) {
    assert(invoke({command}) == 0);
}

int main() {
    viperbass::Mapping server;
    assert(server.create_server());
    auto& c = *server.get();
    assert(c.master_enabled.load() == 1 && c.enabled.load() == 1);

    run("bass-off");
    assert(c.enabled.load() == 0 && c.master_enabled.load() == 1);
    run("off");
    assert(c.master_enabled.load() == 0 && c.enabled.load() == 0);
    run("bass-on");
    assert(c.enabled.load() == 1 && c.master_enabled.load() == 0);
    run("on");
    assert(c.master_enabled.load() == 1 && c.enabled.load() == 1);

    run("bass-toggle");
    assert(c.enabled.load() == 0 && c.master_enabled.load() == 1);
    run("toggle"); // Legacy alias still toggles Bass, not master.
    assert(c.enabled.load() == 1 && c.master_enabled.load() == 1);
    run("all-off"); // Legacy global alias.
    assert(c.master_enabled.load() == 0 && c.enabled.load() == 1);
    run("all-on");
    assert(c.master_enabled.load() == 1 && c.enabled.load() == 1);

    // Scripting interface uses the same numbered levels as the GUI, while
    // shared memory / ViPERDSP retain their existing zero-based indices.
    for (int level = 1; level <= 5; ++level) {
        const std::string value = std::to_string(level);
        assert(invoke({"headphone-level", value.c_str()}) == 0);
        assert(c.headphone_quality.load() == level - 1);
        std::string status;
        assert(invoke({"status"}, &status) == 0);
        assert(status.find("Headphone Surround+:") != std::string::npos);
        assert(status.find("surround level " + value) != std::string::npos);
    }
    // Backward-compatible name, but the argument also follows GUI numbering.
    assert(invoke({"headphone-quality", "2"}) == 0);
    assert(c.headphone_quality.load() == 1);
    for (const char* invalid : {"0", "6", "-1", "2.5", "3.25", "abc", "2x", "nan"}) {
        const int before = c.headphone_quality.load();
        assert(invoke({"headphone-level", invalid}) == 2);
        assert(c.headphone_quality.load() == before);
        assert(invoke({"headphone-quality", invalid}) == 2);
        assert(c.headphone_quality.load() == before);
    }
    assert(invoke({"headphone-level"}) == 2);
    assert(invoke({"headphone-level", "2", "unexpected"}) == 2);
    assert(c.headphone_quality.load() == 1);

    std::ostringstream help;
    usage(help);
    assert(help.str().find("on | off | bass-on | bass-off | bass-toggle") != std::string::npos);
    assert(help.str().find("all-on") == std::string::npos);
    assert(help.str().find("headphone-level <1..5>") != std::string::npos);
    assert(help.str().find("headphone-quality <0..4>") == std::string::npos);
    server.stop_server();
    std::cout << "PASS: CLI global/bass switches, headphone levels, and backward-compatible aliases\n";
}
