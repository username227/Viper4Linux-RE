// Exercise the real CLI parser against an isolated mock shared-memory channel.
// This process owns the test channel and never touches the user's live backend.
#define main viper_ctl_entry
#include "../src/viper-ctl.cpp"
#undef main

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

static void run(const char* command) {
    char name[] = "viper4linux-ctl";
    char arg[128];
    const std::string text(command);
    assert(text.size() < sizeof(arg));
    std::copy(text.begin(), text.end(), arg);
    arg[text.size()] = '\0';
    char* argv[] = {name, arg, nullptr};
    // Suppress the normal status line from each command under test.
    std::ostringstream captured;
    auto* old = std::cout.rdbuf(captured.rdbuf());
    const int result = viper_ctl_entry(2, argv);
    std::cout.rdbuf(old);
    assert(result == 0);
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

    std::ostringstream help;
    usage(help);
    assert(help.str().find("on | off | bass-on | bass-off | bass-toggle") != std::string::npos);
    assert(help.str().find("all-on") == std::string::npos);
    server.stop_server();
    std::cout << "PASS: CLI global/bass switches and backward-compatible aliases\n";
}
