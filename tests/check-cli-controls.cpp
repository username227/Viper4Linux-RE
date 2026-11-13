// Exercise the real CLI parser against an isolated mock shared-memory channel.
// This process owns the test channel and never touches the user's live backend.
#define main viper_ctl_entry
#include "../src/viper-ctl.cpp"
#undef main

#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdlib>
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

static int occurrences(const std::string& text, const std::string& match) {
    int count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(match, pos)) != std::string::npos) {
        ++count;
        pos += match.size();
    }
    return count;
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
    assert(invoke({"toggle"}) == 2); // Generic toggle must not target Bass.
    assert(c.enabled.load() == 0 && c.master_enabled.load() == 1);
    run("bass-toggle");
    assert(c.enabled.load() == 1 && c.master_enabled.load() == 1);
    run("all-off"); // Legacy global alias.
    assert(c.master_enabled.load() == 0 && c.enabled.load() == 1);
    run("all-on");
    assert(c.master_enabled.load() == 1 && c.enabled.load() == 1);

    assert(invoke({"--bass", "gain=2.35"}) == 0);
    assert(c.strength_centi.load() == 235);
    assert(invoke({"--bass", "freq=72.6"}) == 0);
    assert(c.frequency_hz.load() == 73);
    // No compatibility aliases for generic Bass gain and frequency names.
    assert(invoke({"gain", "5"}) == 2);
    assert(invoke({"freq", "90"}) == 2);
    assert(c.strength_centi.load() == 235);
    assert(c.frequency_hz.load() == 73);

    // Validate complete multi-command sequences before executing any part.
    c.master_enabled.store(0);
    c.enabled.store(0);
    c.psycho_enabled.store(1);
    std::string multiOutput;
    assert(invoke({"on", "bass-on", "psycho-off"}, &multiOutput) == 0);
    assert(c.master_enabled.load() == 1 && c.enabled.load() == 1 &&
           c.psycho_enabled.load() == 0);
    assert(multiOutput.find("Master processing: ON") != std::string::npos);
    assert(multiOutput.find("ViPER Bass - ON") != std::string::npos);
    assert(occurrences(multiOutput, "ViPER Bass -") == 1);
    assert(occurrences(multiOutput, "Psychoacoustic Bass -") == 1);
    assert(occurrences(multiOutput, "Master processing:") == 1);

    // Only final values are reported: no intermediate 101% gain snapshot.
    c.limiter_output_percent.store(101);
    assert(invoke({"--limiter", "threshold=0.6", "gain=1"}, &multiOutput) == 0);
    assert(c.limiter_threshold_permille.load() == 600);
    assert(c.limiter_output_percent.load() == 100);
    assert(occurrences(multiOutput, "Master Limiter -") == 1);
    assert(multiOutput.find("Threshold: 0.6") != std::string::npos);
    assert(multiOutput.find("Output: 100%") != std::string::npos);
    assert(multiOutput.find("Output: 101%") == std::string::npos);

    // A sequence crossing scopes reports each distinct effect once, using
    // final settings, in first-appearance order even if a scope is revisited.
    assert(invoke({"--bass", "on", "gain=2.5",
                   "--headphone", "on", "level=3",
                   "--bass", "gain=3"}, &multiOutput) == 0);
    assert(occurrences(multiOutput, "ViPER Bass -") == 1);
    assert(occurrences(multiOutput, "Headphone Surround+ -") == 1);
    assert(multiOutput.find("Strength: 3") != std::string::npos);
    assert(multiOutput.find("Surround level: 3") != std::string::npos);
    assert(multiOutput.find("ViPER Bass -") <
           multiOutput.find("Headphone Surround+ -"));

    // Targeted status merges with a changed effect, and reflects edits
    // made even when the status token appears earlier in the invocation.
    assert(invoke({"--status", "bass", "--bass", "gain=4"}, &multiOutput) == 0);
    assert(occurrences(multiOutput, "ViPER Bass -") == 1);
    assert(multiOutput.find("Strength: 4") != std::string::npos);
    assert(invoke({"--limiter", "gain=1.25", "threshold=0.7",
                   "--status", "limiter", "--full"}, &multiOutput) == 0);
    assert(occurrences(multiOutput, "Master Limiter -") == 1);
    assert(multiOutput.find("Output: 125%") != std::string::npos);
    assert(multiOutput.find("Threshold: 0.7") != std::string::npos);

    // Explicit global status overrides per-effect confirmations.
    assert(invoke({"--bass", "gain=3", "--headphone", "level=2",
                   "--status", "--full"}, &multiOutput) == 0);
    assert(occurrences(multiOutput, "ViPER4Linux-RE - DSP status") == 1);
    assert(occurrences(multiOutput, "ViPER Bass -") == 1);
    assert(occurrences(multiOutput, "Headphone Surround+ -") == 1);

    // Edited EQ bands are shown together without intermediate reports.
    assert(invoke({"--eq", "bands=15", "level=2:-1.75",
                   "level=5:2.5", "level=2:1.25"}, &multiOutput) == 0);
    assert(occurrences(multiOutput, "Graphic Equalizer -") == 1);
    assert(occurrences(multiOutput, "Edited band 2:") == 1);
    assert(multiOutput.find("Edited band 2: 1.25 dB") != std::string::npos);
    assert(multiOutput.find("Edited band 5: 2.5 dB") != std::string::npos);
    assert(multiOutput.find("Edited band 2: -1.75") == std::string::npos);

    assert(invoke({"off", "bass-gain", "bad"}, &multiOutput) == 2);
    assert(c.master_enabled.load() == 1); // Invalid later token did not bypass.
    assert(invoke({"off", "bass-gain", "2.5", "unknown-command"}) == 2);
    assert(c.master_enabled.load() == 1);
    assert(invoke({"--eq", "bands=15", "level=2:-1.75",
                   "level=5:2.5", "level=9:-3", "level=12:-1.5"}) == 0);
    assert(c.eq_band_count.load() == 15);
    assert(c.eq_levels_hundredths[1].load() == -175);
    assert(c.eq_levels_hundredths[4].load() == 250);
    assert(c.eq_levels_hundredths[8].load() == -300);
    assert(c.eq_levels_hundredths[11].load() == -150);
    assert(invoke({"--eq", "level=2:1", "level=16:2"}) == 2);
    assert(c.eq_levels_hundredths[1].load() == -175);
    assert(invoke({"--eq", "bands=10", "level=12:-1.5"}) == 2);
    assert(c.eq_band_count.load() == 15);

    // Effect-scoped syntax is translated before the unchanged DSP handlers.
    c.master_enabled.store(0);
    c.enabled.store(0);
    c.headphone_enabled.store(0);
    assert(invoke({"--on", "--bass", "on", "gain=3", "freq=60",
                   "--headphone", "on", "level=2"}) == 0);
    assert(c.master_enabled.load() == 1 && c.enabled.load() == 1);
    assert(c.strength_centi.load() == 300 && c.frequency_hz.load() == 60);
    assert(c.headphone_enabled.load() == 1 && c.headphone_quality.load() == 1);
    assert(invoke({"--off", "--bass", "on"}) == 0);
    assert(c.master_enabled.load() == 0 && c.enabled.load() == 1);
    assert(invoke({"--bass", "off", "--on"}) == 0);
    assert(c.master_enabled.load() == 1 && c.enabled.load() == 0);
    assert(invoke({"--bass", "on", "gain=4", "--headphone", "level=9"}) == 2);
    assert(c.enabled.load() == 0 && c.strength_centi.load() == 300);
    assert(invoke({"--bass", "gain=3", "--headphone", "level=2.5"}) == 2);
    assert(c.strength_centi.load() == 300);
    assert(invoke({"--limiter", "on"}) == 2);
    // Mixing must fail before a legacy switch or scoped command can write.
    c.enabled.store(0);
    c.analogx_enabled.store(1);
    assert(invoke({"bass-on", "--analogx", "off"}) == 2);
    assert(invoke({"--bass", "on", "analogx-off"}) == 2);
    assert(invoke({"--on", "bass-on"}) == 2);
    assert(c.enabled.load() == 0 && c.analogx_enabled.load() == 1);
    // Only an initial unscoped bare master command is allowed to mix.
    assert(invoke({"on", "--bass", "on"}) == 0);
    assert(c.master_enabled.load() == 1 && c.enabled.load() == 1);
    // Restore both manipulated flags before the independent worker-serial
    // assertions below, which expect each switch to start disabled.
    c.enabled.store(0);
    c.analogx_enabled.store(0);
    // A numeric positional command is rejected even when placed after an effect.
    assert(invoke({"bass-gain", "2"}) == 2);
    assert(invoke({"--bass", "gain", "2"}) == 2);
    assert(invoke({"--bass", "on", "bass-gain", "2"}) == 2);
    assert(c.strength_centi.load() == 300 && c.enabled.load() == 0);

    // Only an explicit status query prints all six categories. Master switch
    // commands show one confirmation line; effect edits remain scoped.
    std::string displayed;
    assert(invoke({"bass-on"}, &displayed) == 0);
    assert(displayed.find("A. ViPER Bass - ON") != std::string::npos);
    assert(displayed.find("Headphone Surround+") == std::string::npos);
    assert(invoke({"headphone-on"}, &displayed) == 0);
    assert(displayed.find("B. Headphone Surround+ - ON") != std::string::npos);
    assert(displayed.find("ViPER Bass") == std::string::npos);
    assert(invoke({"status"}, &displayed) == 0);
    assert(invoke({"--status", "bass"}, &displayed) == 0);
    assert(displayed.find("ViPER Bass") != std::string::npos);
    assert(displayed.find("Headphone Surround+") == std::string::npos);
    assert(invoke({"status", "bass"}, &displayed) == 0);
    assert(displayed.find("ViPER Bass") != std::string::npos);
    assert(invoke({"--bass", "status", "--full"}, &displayed) == 0);
    assert(displayed.find("ViPER Bass") != std::string::npos);
    assert(displayed.find("Headphone Surround+") == std::string::npos);
    assert(invoke({"--status", "--full"}, &displayed) == 0);
    assert(displayed.find("Headphone Surround+") != std::string::npos);
    assert(invoke({"status"}, &displayed) == 0);
    for (const char* tab : {"1. Output", "2. Bass", "3. EQ & Tone", "4. Spatial",
                             "5. Dynamics", "6. Sound Correction"})
        assert(displayed.find(tab) != std::string::npos);
    assert(invoke({"off"}, &displayed) == 0);
    assert(displayed == "Master processing: BYPASSED\n");
    assert(invoke({"on"}, &displayed) == 0);
    assert(displayed == "Master processing: ON\n");
    assert(invoke({"all-off"}, &displayed) == 0);
    assert(displayed == "Master processing: BYPASSED\n");
    assert(invoke({"all-on"}, &displayed) == 0);
    assert(displayed == "Master processing: ON\n");

    // Scripting interface uses the same numbered levels as the GUI, while
    // shared memory / ViPERDSP retain their existing zero-based indices.
    for (int level = 1; level <= 5; ++level) {
        const std::string value = std::to_string(level);
        const std::string option = "level=" + value;
        assert(invoke({"--headphone", option.c_str()}) == 0);
        assert(c.headphone_quality.load() == level - 1);
        std::string status;
        assert(invoke({"status"}, &status) == 0);
        assert(status.find("Headphone Surround+ - ") != std::string::npos);
        assert(status.find("Surround level: " + value) != std::string::npos);
    }
    // Retired generic name must be rejected without modifying the live level.
    assert(invoke({"headphone-quality", "2"}) == 2);
    assert(c.headphone_quality.load() == 4);
    for (const char* invalid : {"0", "6", "-1", "2.5", "3.25", "abc", "2x", "nan"}) {
        const int before = c.headphone_quality.load();
        const std::string option = std::string("level=") + invalid;
        assert(invoke({"--headphone", option.c_str()}) == 2);
        assert(c.headphone_quality.load() == before);
    }
    assert(invoke({"headphone-level"}) == 2);
    assert(invoke({"headphone-level", "2", "unexpected"}) == 2);
    assert(c.headphone_quality.load() == 4);

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
        const std::string legacy(cmd);
        const auto dash = legacy.find('-');
        const std::string selector = "--" + legacy.substr(0, dash);
        const std::string option = legacy.substr(dash + 1) + "=" + arg;
        assert(invoke({selector.c_str(), option.c_str()}) == 0);
        assert(atom.load() == expected && serial.load() == before + 1);
        assert(invoke({selector.c_str(), option.c_str()}) == 0);
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
        const std::string option = std::string("bands=") + invalid;
        assert(invoke({"--mb", option.c_str()}) == 2);
        assert(c.mb_band_count.load() == previous && c.mb_request_serial.load() == serial);
    }
    for (const char* invalid : {"3", "1.5", "nan"}) {
        const auto previous = c.tube_model.load();
        const std::string option = std::string("model=") + invalid;
        assert(invoke({"--tube", option.c_str()}) == 2);
        assert(c.tube_model.load() == previous);
    }

    std::ostringstream help;
    usage(help);
    const std::string mainHelp = help.str();
    const auto globals = mainHelp.find("Global commands:");
    const auto effects = mainHelp.find("Effect selectors (alphabetical):");
    assert(globals != std::string::npos && globals < effects);
    assert(mainHelp.find("  --on ", globals) < effects);
    assert(mainHelp.find("  --off ", globals) < effects);
    assert(mainHelp.find("  --status ", globals) < effects);
    assert(mainHelp.find("  --help ", globals) < effects);
    assert(mainHelp.find("  --bass ", effects) > effects);
    assert(mainHelp.find("  --headphone ", effects) > effects);
    assert(mainHelp.find("v4l-cli bass --help") == std::string::npos);
    assert(mainHelp.find("bass-gain <") == std::string::npos);
    assert(mainHelp.find("Use man viper4linux-ctl") != std::string::npos);
    assert(mainHelp.find("legacy") == std::string::npos);
    assert(invoke({"--help"}, &displayed) == 0);
    assert(displayed.find("Global commands") != std::string::npos);
    assert(invoke({"--bass", "--help"}, &displayed) == 0);
    assert(invoke({"bass", "--help"}) == 2);
    assert(invoke({"status", "--help"}) == 2);
    assert(displayed.find("gain=0.5..10") != std::string::npos);
    assert(displayed.find("Bass gain (stored to hundredths)") != std::string::npos);
    assert(displayed.find("0 Natural, 1 Pure Bass+, 2 Subwoofer") != std::string::npos);
    assert(displayed.find("on|off|toggle") != std::string::npos);
    assert(displayed.find("legacy") == std::string::npos);
    assert(displayed.find("aliases") == std::string::npos);
    assert(invoke({"--headphone", "--help"}, &displayed) == 0);
    assert(displayed.find("level=1..5") != std::string::npos);
    assert(invoke({"--eq", "--help"}, &displayed) == 0);
    assert(displayed.find("level=BAND:DB") != std::string::npos);
    assert(displayed.find("presets") != std::string::npos);
    assert(invoke({"--field", "--help"}, &displayed) == 0);
    assert(displayed.find("depth=0|200..950") != std::string::npos);
    assert(invoke({"--limiter", "--help"}, &displayed) == 0);
    assert(displayed.find(" [on|off]") == std::string::npos);
    assert(invoke({"--ddc", "--help"}, &displayed) == 0);
    assert(displayed.find("VDC profile in the GUI") != std::string::npos);
    // Audit all registered numeric options and every effect page.
    for (const auto& spec : viperbass::cli_batch::specs)
        assert(viperbass::cli_help::descriptionFor(spec.command) != nullptr);
    for (const auto& effect : viperbass::cli_scope::effects) {
        const std::string selector = std::string("--") + effect.name;
        assert(invoke({selector.c_str(), "--help"}, &displayed) == 0);
        assert(displayed.find("Usage: v4l-cli " + selector) != std::string::npos);
        assert(displayed.find("status [--full]") != std::string::npos);
        assert(displayed.find("Numeric setting") == std::string::npos);
        assert(displayed.find("legacy") == std::string::npos);
    }
    assert(invoke({"--status", "--help"}, &displayed) == 0);
    assert(displayed.find("--status [EFFECT]") != std::string::npos);
    server.stop_server();

    // Exercise the no-audio case without touching /proc or live PipeWire.
    // A marked filter-chain process exists, but there is no DSP IPC page yet.
    char temp[] = "/tmp/viper-status-test-XXXXXX";
    const auto* directory = mkdtemp(temp);
    assert(directory != nullptr);
    const auto proc_root = std::filesystem::path(directory);
    std::filesystem::create_directories(proc_root / "12345");
    const auto put = [&](const std::filesystem::path& file, const std::string& value) {
        std::ofstream out(file, std::ios::binary);
        out.write(value.data(), static_cast<std::streamsize>(value.size()));
    };
    put(proc_root / "12345/cmdline", std::string("/usr/bin/pipewire\0-c\0filter-chain.conf\0", sizeof("/usr/bin/pipewire\0-c\0filter-chain.conf\0")-1));
    put(proc_root / "12345/environ", std::string("VIPER_CLEAN_INSTANCE=viperlinux-unified-v1\0", sizeof("VIPER_CLEAN_INSTANCE=viperlinux-unified-v1\0")-1));
    assert(setenv("VIPER_TEST_PROC_ROOT", directory, 1) == 0);
    std::string result;
    assert(invoke({"status"}, &result) == 0);
    assert(result.find("READY TO PROCESS AUDIO") != std::string::npos);
    assert(invoke({"--status"}, &result) == 0);
    assert(invoke({"--status", "bass"}) == 1); // No effect values without IPC.
    assert(result.find("PID 12345") != std::string::npos);
    assert(invoke({"off"}) == 1); // No silent success for disconnected effects.
    // A marked process that isn't PipeWire must never be treated as our backend.
    put(proc_root / "12345/cmdline", std::string("/usr/bin/not-pipewire\0-c\0filter-chain.conf\0", sizeof("/usr/bin/not-pipewire\0-c\0filter-chain.conf\0")-1));
    assert(viperbass::detected_backend_pid() == 0);
    assert(invoke({"status"}) == 1);
    put(proc_root / "12345/cmdline", std::string("/usr/bin/pipewire\0-c\0filter-chain.conf\0", sizeof("/usr/bin/pipewire\0-c\0filter-chain.conf\0")-1));
    put(proc_root / "12345/environ", std::string("OTHER_INSTANCE=1\0", sizeof("OTHER_INSTANCE=1\0")-1));
    assert(viperbass::detected_backend_pid() == 0);
    assert(invoke({"status"}) == 1);
    assert(unsetenv("VIPER_TEST_PROC_ROOT") == 0);
    std::filesystem::remove_all(proc_root);

    std::cout << "PASS: CLI effect controls and ready/idle/offline detection\n";
}
