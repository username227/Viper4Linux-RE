// Run only with VIPER_MOCK_TEST_IPC; never attach to live user audio.
#define main viper_output_state_entry
#include "../src/viper-output-state.cpp"
#undef main
#include <cassert>
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    viperbass::Mapping server;
    assert(server.create_server());
    auto* c = server.get();
    QSettings settings("ViPERforLinux", "ViPERClean");
    QTemporaryDir profileDirectory;
    assert(profileDirectory.isValid());
    const QString ddc = profileDirectory.path() + "/device.vdc";
    const QString ir = profileDirectory.path() + "/device.irs";
    for (const auto& path : {ddc, ir}) {
        QFile file(path);
        assert(file.open(QIODevice::WriteOnly));
        assert(file.write("test profile") > 0);
    }
    c->bass_mode.store(2);
    c->strength_centi.store(475);
    c->eq_band_count.store(15);
    c->eq_levels_hundredths[0].store(-145);
    c->deq_bands[2].gain_hundredths.store(350);
    c->mb_bands[2].threshold_db.store(-27);
    c->imager_high_crossover_hz.store(5500);
    c->tube_model.store(2);
    c->tube_drive_percent.store(73);
    c->analogx_mode.store(2);
    c->crossfeed_preset.store(2);
    c->fet_threshold_db.store(-27);
    c->mb_band_count.store(5);
    c->mb_crossovers_hz[0].store(350);
    c->deq_band_count.store(4);
    c->deq_bands[2].filter_type.store(2);
    c->headphone_enabled.store(1);
    c->headphone_quality.store(4);
    c->speaker_enabled.store(0);
    settings.setValue("ddc_profile", ddc);
    settings.setValue("convolver_kernel", ir);
    settings.setValue("eq/selected_preset", "builtin:Acoustic");
    settings.sync();
    const auto saved = captureState(*c, settings);
    assert(validateState(saved));
    assert(saved.value("values").toObject().value("bass_mode").toInt() == 2);
    assert(saved.value("eq_levels").toArray()[0].toInt() == -145);
    assert(saved.value("ui").toObject().value("eq_selected_preset").toString() == "builtin:Acoustic");
    assert(saved.value("deq_bands").toArray()[2].toObject().value("gain_hundredths").toInt() == 350);

    c->bass_mode.store(0);
    c->strength_centi.store(1000);
    c->eq_levels_hundredths[0].store(0);
    c->deq_bands[2].gain_hundredths.store(0);
    c->mb_bands[2].threshold_db.store(-18);
    c->imager_high_crossover_hz.store(4000);
    c->tube_model.store(0);
    c->tube_drive_percent.store(50);
    c->analogx_mode.store(0);
    c->crossfeed_preset.store(0);
    c->fet_threshold_db.store(-18);
    c->mb_band_count.store(3);
    c->mb_crossovers_hz[0].store(200);
    c->deq_band_count.store(3);
    c->deq_bands[2].filter_type.store(0);
    c->headphone_enabled.store(0);
    c->headphone_quality.store(1);
    c->speaker_enabled.store(1);
    settings.setValue("ddc_profile", "");
    settings.setValue("convolver_kernel", "");
    settings.sync();
    const auto oldEqSerial = c->eq_request_serial.load();
    const auto oldMbSerial = c->mb_request_serial.load();
    const auto oldDeqSerial = c->deq_request_serial.load();
    const auto oldTubeSerial = c->tube_request_serial.load();
    const auto oldAnalogSerial = c->analogx_request_serial.load();
    const auto oldCrossfeedSerial = c->crossfeed_request_serial.load();
    const auto oldFetSerial = c->fet_request_serial.load();
    const auto oldImagerSerial = c->imager_request_serial.load();
    const auto oldDdcSerial = c->ddc_request_serial.load();
    const auto oldConvolverSerial = c->convolver_request_serial.load();
    const auto oldSpeakerSerial = c->speaker_request_serial.load();
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
    assert(c->tube_request_serial.load() > oldTubeSerial);
    assert(c->analogx_request_serial.load() > oldAnalogSerial);
    assert(c->crossfeed_request_serial.load() > oldCrossfeedSerial);
    assert(c->fet_request_serial.load() > oldFetSerial);
    assert(c->imager_request_serial.load() > oldImagerSerial);
    assert(c->ddc_request_serial.load() > oldDdcSerial);
    assert(c->convolver_request_serial.load() > oldConvolverSerial);
    assert(c->speaker_request_serial.load() > oldSpeakerSerial);
    settings.sync();
    assert(settings.value("mb_x0_hz").toInt() == 200);
    assert(settings.value("eq_levels_15").toString().startsWith("-145,"));
    assert(settings.value("eq/selected_preset").toString() == "builtin:Acoustic");
    assert(c->tube_model.load() == 2 && c->tube_drive_percent.load() == 73);
    assert(c->analogx_mode.load() == 2 && c->crossfeed_preset.load() == 2);
    assert(c->fet_threshold_db.load() == -27 && c->mb_band_count.load() == 5);
    assert(c->mb_crossovers_hz[0].load() == 350);
    assert(c->deq_band_count.load() == 4 && c->deq_bands[2].filter_type.load() == 2);
    assert(c->headphone_enabled.load() == 1 && c->headphone_quality.load() == 4);
    assert(c->speaker_enabled.load() == 0);
    assert(settings.value("ddc_profile").toString() == ddc);
    assert(settings.value("convolver_kernel").toString() == ir);

    // Old saved outputs must still restore safely, defaulting to Custom.
    auto legacy = saved;
    auto legacyUi = legacy.value("ui").toObject();
    legacyUi.remove("eq_selected_preset");
    legacy.insert("ui", legacyUi);
    assert(validateState(legacy));
    assert(restoreState(*c, legacy, settings));
    settings.sync();
    assert(settings.value("eq/selected_preset").toString() == "custom");

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
    auto invalidUi = invalid.value("ui").toObject();
    invalidUi.insert("eq_selected_preset", "invalid label");
    invalid.insert("ui", invalidUi);
    assert(!restoreState(*c, invalid, settings));
    assert(c->bass_mode.load() == 1);
    invalid = saved;
    auto paths = invalid.value("files").toObject();
    paths.insert("convolver_kernel", "/does/not/exist.irs");
    invalid.insert("files", paths);
    assert(!restoreState(*c, invalid, settings));

    server.stop_server();
    std::cout << "PASS: native output snapshots include EQ labels, DEQ, compressors, "
                 "native rebuild triggers and IRS/DDC reloads, "
                 "full restored IPC and invalid-state rejection\n";
}
