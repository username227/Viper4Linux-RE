// Verify the same CLI parser and real QSettings persistence with an isolated
// mock control channel and XDG_CONFIG_HOME. Never operates on live audio.
#define main viper_ctl_entry
#include "../src/viper-ctl.cpp"
#undef main
#include <QSettings>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static int execute(std::initializer_list<const char*> args) {
    std::vector<std::string> words = {"viper4linux-ctl"};
    for (const char* a : args) words.emplace_back(a);
    std::vector<char*> argv;
    for (auto& w : words) argv.push_back(w.data());
    return viper_ctl_entry(static_cast<int>(words.size()), argv.data());
}
int main() {
    viperbass::Mapping server;
    assert(server.create_server());
    QSettings s("ViPERforLinux", "ViPERClean");
    assert(execute({"status"}) == 0);
    s.sync();
    assert(!s.contains("master_enabled")); // Status is strictly read-only.
    assert(execute({"off"}) == 0);
    s.sync();
    assert(!s.value("master_enabled", true).toBool());
    assert(execute({"bass-gain", "2.38"}) == 0);
    s.sync();
    assert(s.value("strength_tenths").toInt() == 24); // Round to the GUI's tenths.
    assert(execute({"bass-freq", "74.6"}) == 0);
    s.sync();
    assert(s.value("frequency").toInt() == 75);
    // Generic names were deliberately removed as a CLI-breaking cleanup.
    assert(execute({"gain", "9"}) == 2);
    assert(execute({"freq", "100"}) == 2);
    s.sync();
    assert(s.value("strength_tenths").toInt() == 24);
    assert(s.value("frequency").toInt() == 75);
    assert(execute({"psycho-intensity", "34.6"}) == 0);
    s.sync();
    assert(s.value("psycho_intensity_percent").toInt() == 35);
    assert(execute({"spectrum-freq", "4300"}) == 0);
    s.sync();
    assert(s.value("spectrum_frequency_hundreds").toInt() == 43);
    assert(execute({"tube-model", "2"}) == 0);
    s.sync();
    assert(s.value("tube_model").toInt() == 2);
    assert(execute({"eq-bands", "15"}) == 0);
    assert(execute({"eq-level", "2", "1.75"}) == 0);
    s.sync();
    assert(s.value("eq_band_count").toInt() == 15);
    assert(s.value("eq/selected_preset").toString() == "custom");
    assert(s.value("eq_levels_15").toString().startsWith("0,175,"));
    assert(execute({"headphone-level", "3"}) == 0);
    s.sync();
    assert(s.value("headphone_quality").toInt() == 2);
    assert(execute({"field-depth", "550"}) == 0);
    s.sync();
    assert(s.value("field_depth_step").toInt() == 8);
    assert(execute({"imager-xover-high", "6000"}) == 0);
    s.sync();
    assert(s.value("imager_high_crossover_hz").toInt() == 6000);
    const auto before = s.value("psycho_intensity_percent").toInt();
    assert(execute({"psycho-intensity", "101"}) == 2);
    s.sync();
    assert(s.value("psycho_intensity_percent").toInt() == before);
    assert(execute({"mb-bands", "5"}) == 0);
    s.sync();
    assert(s.value("mb_band_count").toInt() == 5);
    server.stop_server();
    std::cout << "PASS: CLI settings survive without GUI, invalid edits are not saved\n";
}
