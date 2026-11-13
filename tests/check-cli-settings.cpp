// Verify the same CLI parser and real QSettings persistence with an isolated
// mock control channel and XDG_CONFIG_HOME. Never operates on live audio.
#define main viper_ctl_entry
#include "../src/viper-ctl.cpp"
#undef main
#include <QSettings>
#include <QStringList>
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
    // A CLI change must leave unrelated UI preferences, staged presets and
    // other effects' exact native precision untouched.
    s.setValue("ui/theme", "classic-v4a");
    s.setValue("preset/valid", true);
    s.setValue("preset/bass_mode", 2);
    s.setValue("psycho_intensity_percent", 22);
    s.setValue("precision/clarity_gain_milli", 4321);
    s.sync();
    const QStringList statusKeys = s.allKeys();
    assert(execute({"status"}) == 0);
    assert(execute({"--status", "bass"}) == 0);
    assert(execute({"--bass", "status", "--full"}) == 0);
    s.sync();
    assert(s.allKeys() == statusKeys);
    assert(execute({"--bass", "gain=2.38"}) == 0);
    s.sync();
    assert(s.value("ui/theme").toString() == "classic-v4a");
    assert(s.value("preset/valid").toBool());
    assert(s.value("preset/bass_mode").toInt() == 2);
    assert(s.value("psycho_intensity_percent").toInt() == 22);
    assert(s.value("precision/clarity_gain_milli").toInt() == 4321);
    s.sync();
    assert(s.value("strength_tenths").toInt() == 23);
    assert(s.value("precision/strength_centi").toInt() == 238); // Full native precision, with GUI coarse projection.
    assert(execute({"--bass", "freq=74.6"}) == 0);
    s.sync();
    assert(s.value("frequency").toInt() == 75);
    assert(execute({"--limiter", "threshold-db=-6.0"}) == 0);
    s.sync();
    assert(s.value("limiter_threshold_permille").toInt() == 501);
    // Generic names were deliberately removed as a CLI-breaking cleanup.
    assert(execute({"gain", "9"}) == 2);
    assert(execute({"freq", "100"}) == 2);
    s.sync();
    assert(s.value("strength_tenths").toInt() == 23);
    assert(s.value("precision/strength_centi").toInt() == 238);
    assert(s.value("frequency").toInt() == 75);
    assert(execute({"--psycho", "intensity=34.6"}) == 0);
    s.sync();
    assert(s.value("psycho_intensity_percent").toInt() == 35);
    assert(execute({"--spectrum", "freq=4300"}) == 0);
    s.sync();
    assert(s.value("spectrum_frequency_hundreds").toInt() == 43);
    assert(s.value("precision/spectrum_frequency_hz").toInt() == 4300);
    assert(execute({"--spectrum", "freq=8024"}) == 0);
    s.sync();
    assert(s.value("spectrum_frequency_hundreds").toInt() == 80);
    assert(s.value("precision/spectrum_frequency_hz").toInt() == 8024);
    assert(execute({"spectrum-on"}) == 0); // An unrelated value in the same effect group.
    s.sync();
    assert(s.value("precision/spectrum_frequency_hz").toInt() == 8024);
    assert(execute({"--clarity", "gain=2.675"}) == 0);
    s.sync();
    assert(s.value("clarity_gain_tenths").toInt() == 26);
    assert(s.value("precision/clarity_gain_milli").toInt() == 2675);
    assert(execute({"--mono", "strength=2.47"}) == 0);
    s.sync();
    assert(s.value("precision/bass_mono_strength_centi").toInt() == 247);
    assert(execute({"--spectrum", "exciter=2.345"}) == 0);
    s.sync();
    assert(s.value("precision/spectrum_exciter_milli").toInt() == 2345);
    assert(execute({"--diff", "lowpass=4267"}) == 0);
    s.sync();
    assert(s.value("diff_lp_hundreds").toInt() == 42);
    assert(s.value("precision/diff_lp_hz").toInt() == 4267);
    assert(execute({"--tube", "model=2"}) == 0);
    s.sync();
    assert(s.value("tube_model").toInt() == 2);
    assert(execute({"--eq", "bands=15"}) == 0);
    assert(execute({"--eq", "level=2:1.75"}) == 0);
    s.sync();
    assert(s.value("eq_band_count").toInt() == 15);
    assert(s.value("eq/selected_preset").toString() == "custom");
    assert(s.value("eq_levels_15").toString().startsWith("0,175,"));
    assert(execute({"--headphone", "level=3"}) == 0);
    s.sync();
    assert(s.value("headphone_quality").toInt() == 2);
    assert(execute({"--field", "depth=550"}) == 0);
    s.sync();
    assert(s.value("field_depth_step").toInt() == 8);
    assert(s.value("precision/field_depth").toInt() == 550);
    assert(execute({"--field", "depth=537"}) == 0);
    s.sync();
    assert(s.value("precision/field_depth").toInt() == 537);
    assert(s.value("field_depth_step").toInt() == 8);
    assert(execute({"--imager", "xover-high=6000"}) == 0);
    s.sync();
    assert(s.value("imager_high_crossover_hz").toInt() == 6000);
    const auto before = s.value("psycho_intensity_percent").toInt();
    assert(execute({"--psycho", "intensity=101"}) == 2);
    s.sync();
    assert(s.value("psycho_intensity_percent").toInt() == before);
    assert(execute({"--mb", "bands=5"}) == 0);
    s.sync();
    assert(s.value("mb_band_count").toInt() == 5);
    server.stop_server();
    std::cout << "PASS: CLI settings survive without GUI, invalid edits are not saved\n";
}
