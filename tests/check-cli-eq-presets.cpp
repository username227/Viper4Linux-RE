// Preset selection, persistence, complete status, and read-only listing.
// The mock IPC channel and temporary XDG_CONFIG_HOME protect the live backend.
#define main viper_ctl_entry
#include "../src/viper-ctl.cpp"
#undef main
#include <QSettings>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static int invoke(std::initializer_list<const char*> args, std::string* result = nullptr) {
    std::vector<std::string> words{"viper4linux-ctl"};
    for (const auto* arg : args) words.emplace_back(arg);
    std::vector<char*> argv;
    for (auto& word : words) argv.push_back(word.data());
    std::ostringstream out, err;
    auto* saveOut = std::cout.rdbuf(out.rdbuf());
    auto* saveErr = std::cerr.rdbuf(err.rdbuf());
    const auto code = viper_ctl_entry(static_cast<int>(words.size()), argv.data());
    std::cout.rdbuf(saveOut);
    std::cerr.rdbuf(saveErr);
    if (result) *result = out.str();
    return code;
}
int main() {
    QSettings s("ViPERforLinux", "ViPERClean");
    std::string output;
    assert(invoke({"eq-presets"}, &output) == 0);
    assert(output.find("Acoustic") != std::string::npos);
    assert(output.find("Flat") != std::string::npos);

    viperbass::Mapping server;
    assert(server.create_server());
    auto& c = *server.get();
    c.eq_band_count.store(15);
    c.eq_enabled.store(0);
    assert(invoke({"--eq", "preset=Acoustic"}, &output) == 0);
    assert(c.eq_enabled.load() == 0); // Picking a preset does not enable EQ.
    assert(c.eq_levels_hundredths[0].load() == 450);
    assert(c.eq_levels_hundredths[3].load() == 400);
    s.sync();
    assert(s.value("eq/selected_preset").toString() == "builtin:Acoustic");
    assert(s.value("eq_levels_15").toString().split(',').size() == 15);
    assert(output.find("Selected EQ preset: builtin:Acoustic") != std::string::npos);

    assert(invoke({"--eq", "preset=Flat"}) == 0);
    for (int i=0; i<15; ++i) assert(c.eq_levels_hundredths[i].load() == 0);
    const auto serial = c.eq_request_serial.load();
    assert(invoke({"--eq", "preset=No such preset"}) == 2);
    assert(c.eq_request_serial.load() == serial);
    // An invalid preset later in a batch must not apply earlier edits.
    c.master_enabled.store(1);
    assert(invoke({"off", "--eq", "preset=No such preset"}) == 2);
    assert(c.master_enabled.load() == 1);

    // User EQ presets are different from GUI's named full-effect snapshots.
    s.setValue("eq/user_presets/My Curve/bands", 10);
    s.setValue("eq/user_presets/My Curve/levels", "0,100,200,300,400,500,400,300,200,100");
    s.sync();
    assert(invoke({"eq-presets"}, &output) == 0);
    assert(output.find("My Curve") != std::string::npos);
    assert(invoke({"--eq", "preset=My Curve"}) == 0);
    s.sync();
    assert(s.value("eq/selected_preset").toString() == "user:My Curve");
    assert(c.eq_levels_hundredths[1].load() != 0);

    // A named user preset stays selected when switching layouts and is
    // interpolated to the new set of frequencies, as in the GUI.
    const int userGain15 = c.eq_levels_hundredths[1].load();
    assert(invoke({"--eq", "bands=31"}) == 0);
    assert(c.eq_band_count.load() == 31);
    s.sync();
    assert(s.value("eq/selected_preset").toString() == "user:My Curve");
    assert(s.value("eq_levels_31").toString().split(',').size() == 31);
    assert(invoke({"--eq", "bands=15"}) == 0);
    assert(c.eq_levels_hundredths[1].load() == userGain15);
    s.sync();
    assert(s.value("eq/selected_preset").toString() == "user:My Curve");

    // An unedited 31-band Flat layout remains separate from a custom 10-band
    // boost, and both custom curves survive round-tripping between layouts.
    assert(invoke({"--eq", "bands=31", "preset=Flat"}) == 0);
    assert(invoke({"--eq", "bands=10", "level=9:3"}) == 0);
    assert(c.eq_band_count.load() == 10);
    assert(c.eq_levels_hundredths[8].load() == 300);
    // Never-used 25-band custom layout begins flat, not a copied 10-band curve.
    assert(invoke({"--eq", "bands=25"}) == 0);
    assert(c.eq_levels_hundredths[8].load() == 0);
    assert(invoke({"--eq", "bands=10"}) == 0);
    assert(c.eq_levels_hundredths[8].load() == 300);
    assert(invoke({"--eq", "bands=31"}) == 0);
    assert(c.eq_levels_hundredths[8].load() == 0);
    assert(invoke({"--eq", "level=9:-2"}) == 0);
    assert(invoke({"--eq", "bands=10"}) == 0);
    assert(c.eq_levels_hundredths[8].load() == 300);
    assert(invoke({"--eq", "bands=31"}) == 0);
    assert(c.eq_levels_hundredths[8].load() == -200);
    s.sync();
    assert(s.value("eq_levels_10").toString().split(',').at(8) == "300");
    assert(s.value("eq_levels_31").toString().split(',').at(8) == "-200");
    assert(s.value("eq/selected_preset").toString() == "custom");

    // Built-ins use their exact 10/31-band tables, not interpolated values.
    assert(invoke({"--eq", "preset=Acoustic"}) == 0);
    assert(c.eq_levels_hundredths[5].load() == 450); // 31-band band 6
    assert(invoke({"--eq", "bands=10"}) == 0);
    assert(c.eq_levels_hundredths[5].load() == 50);  // 10-band band 6
    s.sync();
    assert(s.value("eq/selected_preset").toString() == "builtin:Acoustic");
    assert(invoke({"--eq", "bands=31"}) == 0);
    assert(c.eq_levels_hundredths[5].load() == 450);
    assert(invoke({"--eq", "preset=My Curve"}) == 0); // Restore status test context.

    assert(invoke({"status"}, &output) == 0);
    assert(output.find("Band 1 (") == std::string::npos);
    assert(invoke({"status", "--full"}, &output) == 0);
    assert(output.find("Band 1 (") != std::string::npos);
    assert(output.find("Selected preset: user:My Curve") != std::string::npos);
    assert(output.find("4. Spatial") != std::string::npos);
    assert(output.find("6. Sound Correction") != std::string::npos);
    c.ddc_status.store(-2);
    assert(invoke({"status"}, &output) == 0);
    assert(output.find("UNSUPPORTED AUDIO RATE") != std::string::npos);
    assert(output.find("RESAMPLE ERROR") == std::string::npos);
    c.convolver_status.store(-2);
    assert(invoke({"status"}, &output) == 0);
    assert(output.find("RESAMPLE ERROR") != std::string::npos);
    assert(invoke({"off"}, &output) == 0);
    assert(output == "Master processing: BYPASSED\n");
    assert(invoke({"status", "--full"}, &output) == 0);
    assert(output.find("Master Limiter - BYPASSED (Master OFF)") != std::string::npos);
    assert(invoke({"on"}, &output) == 0);
    assert(invoke({"status", "--full"}, &output) == 0);
    assert(output.find("Master Limiter - ON (mandatory)") != std::string::npos);
    server.stop_server();
    assert(invoke({"eq-presets"}) == 0); // Does not need a DSP channel.
    assert(invoke({"--eq", "preset=Acoustic"}) == 1);
    std::cout << "PASS: EQ built-in/user presets, full status, error mapping, and offline listing\n";
}
