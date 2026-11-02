// Run only with VIPER_MOCK_TEST_IPC; never attach to live user audio.
#define main viper_output_state_entry
#include "../src/viper-output-state.cpp"
#undef main
#include <cassert>
#include <QCoreApplication>
#include <QSettings>
#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    viperbass::Mapping server;
    assert(server.create_server());
    auto* c = server.get();
    QSettings settings("ViPERforLinux", "ViPERClean");
    c->bass_mode.store(2);
    c->strength_centi.store(475);
    c->eq_band_count.store(15);
    c->eq_levels_hundredths[0].store(-145);
    c->deq_bands[2].gain_hundredths.store(350);
    c->mb_bands[2].threshold_db.store(-27);
    c->imager_high_crossover_hz.store(5500);
    settings.setValue("ddc_profile", "");
    settings.setValue("convolver_kernel", "");
    settings.sync();
    const auto saved = captureState(*c, settings);
    assert(validateState(saved));
    assert(saved.value("values").toObject().value("bass_mode").toInt() == 2);
    assert(saved.value("eq_levels").toArray()[0].toInt() == -145);
    assert(saved.value("deq_bands").toArray()[2].toObject().value("gain_hundredths").toInt() == 350);

    c->bass_mode.store(0);
    c->strength_centi.store(1000);
    c->eq_levels_hundredths[0].store(0);
    c->deq_bands[2].gain_hundredths.store(0);
    c->mb_bands[2].threshold_db.store(-18);
    c->imager_high_crossover_hz.store(4000);
    const auto oldEqSerial = c->eq_request_serial.load();
    const auto oldMbSerial = c->mb_request_serial.load();
    const auto oldDeqSerial = c->deq_request_serial.load();
    assert(restoreState(*c, saved, settings));
    assert(c->bass_mode.load() == 2);
    assert(c->strength_centi.load() == 475);
    assert(c->eq_levels_hundredths[0].load() == -145);
    assert(c->deq_bands[2].gain_hundredths.load() == 350);
    assert(c->mb_bands[2].threshold_db.load() == -27);
    assert(c->imager_high_crossover_hz.load() == 5500);
    assert(c->eq_request_serial.load() > oldEqSerial);
    assert(c->mb_request_serial.load() > oldMbSerial);
    assert(c->deq_request_serial.load() > oldDeqSerial);
    settings.sync();
    assert(settings.value("mb_x0_hz").toInt() == 200);
    assert(settings.value("eq_levels_15").toString().startsWith("-145,"));

    // Invalid snapshots are rejected before touching any DSP controls.
    auto invalid = saved;
    invalid.insert("control_version", 25);
    c->bass_mode.store(1);
    assert(!restoreState(*c, invalid, settings));
    assert(c->bass_mode.load() == 1);
    invalid = saved;
    auto values = invalid.value("values").toObject();
    values.insert("limiter_output_percent", 50000);
    invalid.insert("values", values);
    assert(!restoreState(*c, invalid, settings));
    assert(c->bass_mode.load() == 1);
    invalid = saved;
    auto paths = invalid.value("files").toObject();
    paths.insert("convolver_kernel", "/does/not/exist.irs");
    invalid.insert("files", paths);
    assert(!restoreState(*c, invalid, settings));

    server.stop_server();
    std::cout << "PASS: native output snapshots include EQ, DEQ, compressors, file paths, "
                 "full restored IPC and invalid-state rejection\n";
}
