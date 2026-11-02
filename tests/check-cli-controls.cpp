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

    // All 24 native effect groups have a basic, scriptable switch (the
    // limiter is built into the output path and has parameters, not an on/off).
    auto checkWorker = [&](const char* on, const char* off,
                           std::atomic<std::int32_t>& flag,
                           std::atomic<std::uint32_t>& serial) {
        const auto first = serial.load();
        run(on);
        assert(flag.load() == 1 && serial.load() == first + 1);
        run(on); // Idempotent scripts must not trigger a redundant DSP rebuild.
        assert(serial.load() == first + 1);
        run(off);
        assert(flag.load() == 0 && serial.load() == first + 2);
    };
    checkWorker("deq-on", "deq-off", c.deq_enabled, c.deq_request_serial);
    checkWorker("tube-on", "tube-off", c.tube_enabled, c.tube_request_serial);
    checkWorker("analogx-on", "analogx-off", c.analogx_enabled, c.analogx_request_serial);
    checkWorker("crossfeed-on", "crossfeed-off", c.crossfeed_enabled, c.crossfeed_request_serial);
    checkWorker("imager-on", "imager-off", c.imager_enabled, c.imager_request_serial);
    checkWorker("fet-on", "fet-off", c.fet_enabled, c.fet_request_serial);
    checkWorker("mb-on", "mb-off", c.mb_enabled, c.mb_request_serial);
    checkWorker("ddc-on", "ddc-off", c.ddc_enabled, c.ddc_request_serial);
    checkWorker("speaker-on", "speaker-off", c.speaker_enabled, c.speaker_request_serial);

    const auto testValue = [&](const char* cmd, const char* arg,
                               std::atomic<std::int32_t>& atom,
                               std::atomic<std::uint32_t>& serial, int expected) {
        const auto before = serial.load();
        assert(invoke({cmd, arg}) == 0);
        assert(atom.load() == expected && serial.load() == before + 1);
        assert(invoke({cmd, arg}) == 0);
        assert(serial.load() == before + 1);
    };
    testValue("tube-model", "2", c.tube_model, c.tube_request_serial, 2);
    testValue("tube-drive", "63", c.tube_drive_percent, c.tube_request_serial, 63);
    testValue("tube-mix", "24", c.tube_mix_percent, c.tube_request_serial, 24);
    testValue("analogx-mode", "1", c.analogx_mode, c.analogx_request_serial, 1);
    testValue("crossfeed-mode", "2", c.crossfeed_preset, c.crossfeed_request_serial, 2);
    testValue("imager-low", "137", c.imager_low_width_percent, c.imager_request_serial, 137);
    testValue("imager-mid", "148", c.imager_mid_width_percent, c.imager_request_serial, 148);
    testValue("imager-high", "152", c.imager_high_width_percent, c.imager_request_serial, 152);
    testValue("imager-xover-low", "240", c.imager_low_crossover_hz, c.imager_request_serial, 240);
    testValue("imager-xover-high", "5000", c.imager_high_crossover_hz, c.imager_request_serial, 5000);
    testValue("fet-threshold", "-20", c.fet_threshold_db, c.fet_request_serial, -20);
    testValue("fet-ratio", "2.5", c.fet_ratio_tenths, c.fet_request_serial, 25);
    testValue("mb-bands", "5", c.mb_band_count, c.mb_request_serial, 5);
    for (const char* invalid : {"0", "4", "2.5", "xyz"}) {
        const auto previous = c.mb_band_count.load();
        const auto serial = c.mb_request_serial.load();
        assert(invoke({"mb-bands", invalid}) == 2);
        assert(c.mb_band_count.load() == previous && c.mb_request_serial.load() == serial);
    }
    for (const char* invalid : {"3", "1.5", "nan"}) {
        const auto previous = c.tube_model.load();
        assert(invoke({"tube-model", invalid}) == 2);
        assert(c.tube_model.load() == previous);
    }

    std::ostringstream help;
    usage(help);
    assert(help.str().find("Global:        status, on, off") != std::string::npos);
    assert(help.str().find("bass-on/off") != std::string::npos);
    assert(help.str().find("all-on") == std::string::npos);
    assert(help.str().find("headphone-level <1..5>") != std::string::npos);
    assert(help.str().find("headphone-quality <0..4>") == std::string::npos);
    assert(help.str().find("ddc-on/off") != std::string::npos);
    assert(help.str().find("speaker-on/off") != std::string::npos);
    server.stop_server();
    std::cout << "PASS: CLI effect switches, serials, parameters, headphone levels, and aliases\n";
}
