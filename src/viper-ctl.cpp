#include "viper-control.h"
#include "viper-cli-report.h"
#include "viper-backend-state.h"
#include "viper-cli-settings.h"
#include "viper-cli-eq.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace {
void usage(std::ostream& out = std::cerr) {
    out << "Usage: viper4linux-ctl COMMAND [ARGUMENTS...]\n"
           "\n"
           "Global:        status [--full], on, off\n"
           "Bass:          bass-on/off, bass-toggle, bass-mode <0..2>, gain <0.5..10>, freq <15..150 Hz>\n"
           "Bass Mono:     mono-on/off, mono-mode <0..2>, mono-strength <0.5..10>, mono-freq <15..150>\n"
           "Psycho Bass:   psycho-on/off, psycho-cutoff/intensity/order/original <value>\n"
           "Dynamic Bass:  dynamic-on/off, dynamic-x-low/high, dynamic-y-low/high,\n"
           "               dynamic-side-low/high, dynamic-strength <value>\n"
           "EQ:            eq-on/off, eq-bands <10|15|25|31>, eq-level <band> <dB>\n"
           "               eq-presets, eq-preset <NAME>\n"
           "Dynamic EQ:    deq-on/off (edit individual bands in GUI)\n"
           "Clarity:       clarity-on/off, clarity-gain <0..4.5>, clarity-mode <0..2>\n"
           "Spectrum:      spectrum-on/off, spectrum-freq <2200..8200>, spectrum-exciter <0..6>\n"
           "Tube:          tube-on/off, tube-model <0..2>, tube-drive/mix <0..100>\n"
           "AnalogX:       analogx-on/off, analogx-mode <0..2>\n"
           "Crossfeed:     crossfeed-on/off, crossfeed-mode <0..2>\n"
           "Surround+:     headphone-on/off, headphone-level <1..5>\n"
           "Differential:  diff-on/off, diff-delay/reverse/mix/lowpass <value>\n"
           "Field:         field-on/off, field-widen/mid/depth <value>\n"
           "Reverb:        reverb-on/off, reverb-room/width/damp/wet/dry <0..100>\n"
           "Stereo Imager: imager-on/off, imager-low/mid/high <0..200>,\n"
           "               imager-xover-low <80..400>, imager-xover-high <2000..8000>\n"
           "Compressors:   fet-on/off, fet-threshold <-48..0>, fet-ratio <1..10>,\n"
           "               mb-on/off, mb-bands <3|5>\n"
           "Correction:    ddc-on/off, speaker-on/off, convolver-on/off,\n"
           "               convolver-cross <0..100>\n"
           "Output:        limiter-threshold/gain/pan <value>, agc-on/off,\n"
           "               agc-strength/max/threshold <value>, lufs-on/off,\n"
           "               lufs-target/max/speed <value>\n"
           "\n"
           "All on/off pairs are individual commands (for example, tube-on and tube-off).\n"
           "Use 'man viper4linux-ctl' for exact argument ranges and units.\n";
}

float number(const char* value, float lower, float upper) {
    std::size_t parsed = 0;
    const float result = std::stof(value, &parsed);
    if (value[parsed] || !std::isfinite(result) || result < lower || result > upper)
        throw std::invalid_argument("Value outside permitted range");
    return result;
}
int whole(const char* value, int lo, int hi) {
    const auto n = number(value, static_cast<float>(lo), static_cast<float>(hi));
    if (std::trunc(n) != n) throw std::invalid_argument("Expected a whole number");
    return static_cast<int>(n);
}
void workerToggle(std::atomic<std::int32_t>& effect,
                  std::atomic<std::uint32_t>& serial, int value) {
    // Rebuilds/reset filters on the backend's non-RT worker, not the audio callback.
    if (effect.exchange(value, std::memory_order_relaxed) != value)
        serial.fetch_add(1, std::memory_order_release);
}
void workerValue(std::atomic<std::int32_t>& parameter,
                 std::atomic<std::uint32_t>& serial, int value) {
    if (parameter.exchange(value, std::memory_order_relaxed) != value)
        serial.fetch_add(1, std::memory_order_release);
}
void show(viperbass::Controls& c, const std::string& command = "status",
          const char* bandText = nullptr, bool expanded = false) {
    const int band = command == "eq-level" && bandText ? std::atoi(bandText) : -1;
    viperbass::cli_report::FullDetails info;
    if (expanded) {
        QSettings settings("ViPERforLinux", "ViPERClean");
        settings.sync();
        info.selectedPreset = settings.value("eq/selected_preset", "custom").toString().toStdString();
        info.ddcProfile = settings.value("ddc_profile").toString().toStdString();
        info.convolverKernel = settings.value("convolver_kernel").toString().toStdString();
    }
    viperbass::printCliReport(std::cout, c, command, band, expanded, info);
}
}
int main(int argc, char** argv) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        usage(std::cout);
        return 0;
    }
    if (argc < 2) { usage(); return 2; }
    if (argc == 2 && std::string(argv[1]) == "eq-presets") {
        QSettings settings("ViPERforLinux", "ViPERClean");
        viperbass::cli_eq::list(settings, std::cout);
        return 0;
    }
    viperbass::Mapping mapping;
    if (!mapping.attach()) {
        const auto pid = viperbass::detected_backend_pid();
        if (std::string(argv[1]) == "status" &&
            (argc == 2 || (argc == 3 && std::string(argv[2]) == "--full")) && pid > 0) {
            std::cout << "ViPER backend: READY TO PROCESS AUDIO (PID " << pid
                      << ") | DSP control channel: not active yet\n";
            return 0; // Process is running; a script can distinguish ready from offline.
        }
        if (pid > 0) {
            std::cerr << "ViPER backend is ready to process audio (PID " << pid
                      << "), but the DSP control channel is not active yet. "
                         "Play audio and retry the effect command.\n";
        } else {
            std::cerr << "ViPER backend: OFFLINE. Start viper4linux and retry.\n";
        }
        return 1; // An effect command must never claim success without a live channel.
    }
    auto& c = *mapping.get();
    const std::string cmd(argv[1]);
    try {
        if (cmd == "status" && (argc == 2 ||
                (argc == 3 && std::string(argv[2]) == "--full"))) {
            show(c, cmd, nullptr, argc == 3);
            return 0;
        }
        if ((cmd == "on" || cmd == "all-on") && argc == 2) c.master_enabled.store(1);
        else if ((cmd == "off" || cmd == "all-off") && argc == 2) c.master_enabled.store(0);
        else if (cmd == "eq-preset" && argc == 3) {
            QSettings settings("ViPERforLinux", "ViPERClean");
            std::string error;
            if (!viperbass::cli_eq::choose(settings, c, QString::fromUtf8(argv[2]), error)) {
                std::cerr << "EQ preset: " << error << '\n';
                return error.find("Could not save") != std::string::npos ? 1 : 2;
            }
        }
        else if (cmd == "bass-on" && argc == 2) c.enabled.store(1);
        else if (cmd == "bass-off" && argc == 2) c.enabled.store(0);
        else if ((cmd == "bass-toggle" || cmd == "toggle") && argc == 2)
            c.enabled.store(!c.enabled.load());
        else if (cmd == "bass-mode" && argc == 3) {
            const auto v = number(argv[2], 0.f, 2.f);
            if (std::trunc(v) != v) throw std::invalid_argument("Bass mode must be an integer 0, 1, or 2");
            c.bass_mode.store(static_cast<int>(v));
        }
        else if (cmd == "gain" && argc == 3)
            c.strength_centi.store(static_cast<int>(std::lround(number(argv[2], 0.5f, 10.f) * 100.f)));
        else if (cmd == "freq" && argc == 3)
            c.frequency_hz.store(static_cast<int>(std::lround(number(argv[2], 15.f, 150.f))));
        else if (cmd == "eq-on" && argc == 2) {
            c.eq_enabled.store(1);
            c.eq_request_serial.fetch_add(1, std::memory_order_release);
        }
        else if (cmd == "eq-off" && argc == 2) {
            c.eq_enabled.store(0);
            c.eq_request_serial.fetch_add(1, std::memory_order_release);
        }
        else if (cmd == "eq-bands" && argc == 3) {
            const auto v = number(argv[2], 10.f, 31.f);
            if (v != 10.f && v != 15.f && v != 25.f && v != 31.f)
                throw std::invalid_argument("EQ mode must be 10, 15, 25, or 31");
            c.eq_band_count.store(static_cast<int>(v));
            // CLI changes use existing level indices; the GUI maintains
            // independent saved layouts when selecting a different mode.
            c.eq_request_serial.fetch_add(1, std::memory_order_release);
        }
        else if (cmd == "eq-level" && argc == 4) {
            const auto index = number(argv[2], 1.f, 31.f);
            if (std::trunc(index) != index || index > c.eq_band_count.load())
                throw std::invalid_argument("EQ band index exceeds active band count");
            const auto db = number(argv[3], -12.f, 12.f);
            c.eq_levels_hundredths[static_cast<int>(index)-1].store(static_cast<int>(std::lround(db*100.f)));
            c.eq_request_serial.fetch_add(1, std::memory_order_release);
        }
        else if (cmd == "convolver-on" && argc == 2) c.convolver_enabled.store(1);
        else if (cmd == "convolver-off" && argc == 2) c.convolver_enabled.store(0);
        else if (cmd == "convolver-cross" && argc == 3)
            c.convolver_cross_percent.store(static_cast<int>(std::lround(number(argv[2], 0.f, 100.f))));
        // The worker-managed native effects must receive a request serial with
        // every changed control. Directly storing the enable field alone can
        // leave their current filters unchanged until an unrelated GUI edit.
        else if (cmd == "deq-on" && argc == 2) workerToggle(c.deq_enabled, c.deq_request_serial, 1);
        else if (cmd == "deq-off" && argc == 2) workerToggle(c.deq_enabled, c.deq_request_serial, 0);
        else if (cmd == "tube-on" && argc == 2) workerToggle(c.tube_enabled, c.tube_request_serial, 1);
        else if (cmd == "tube-off" && argc == 2) workerToggle(c.tube_enabled, c.tube_request_serial, 0);
        else if (cmd == "tube-model" && argc == 3) workerValue(c.tube_model, c.tube_request_serial, whole(argv[2], 0, 2));
        else if (cmd == "tube-drive" && argc == 3) workerValue(c.tube_drive_percent, c.tube_request_serial, whole(argv[2], 0, 100));
        else if (cmd == "tube-mix" && argc == 3) workerValue(c.tube_mix_percent, c.tube_request_serial, whole(argv[2], 0, 100));
        else if (cmd == "analogx-on" && argc == 2) workerToggle(c.analogx_enabled, c.analogx_request_serial, 1);
        else if (cmd == "analogx-off" && argc == 2) workerToggle(c.analogx_enabled, c.analogx_request_serial, 0);
        else if (cmd == "analogx-mode" && argc == 3) workerValue(c.analogx_mode, c.analogx_request_serial, whole(argv[2], 0, 2));
        else if (cmd == "crossfeed-on" && argc == 2) workerToggle(c.crossfeed_enabled, c.crossfeed_request_serial, 1);
        else if (cmd == "crossfeed-off" && argc == 2) workerToggle(c.crossfeed_enabled, c.crossfeed_request_serial, 0);
        else if (cmd == "crossfeed-mode" && argc == 3) workerValue(c.crossfeed_preset, c.crossfeed_request_serial, whole(argv[2], 0, 2));
        else if (cmd == "imager-on" && argc == 2) workerToggle(c.imager_enabled, c.imager_request_serial, 1);
        else if (cmd == "imager-off" && argc == 2) workerToggle(c.imager_enabled, c.imager_request_serial, 0);
        else if (cmd == "imager-low" && argc == 3) workerValue(c.imager_low_width_percent, c.imager_request_serial, whole(argv[2], 0, 200));
        else if (cmd == "imager-mid" && argc == 3) workerValue(c.imager_mid_width_percent, c.imager_request_serial, whole(argv[2], 0, 200));
        else if (cmd == "imager-high" && argc == 3) workerValue(c.imager_high_width_percent, c.imager_request_serial, whole(argv[2], 0, 200));
        else if (cmd == "imager-xover-low" && argc == 3) workerValue(c.imager_low_crossover_hz, c.imager_request_serial, whole(argv[2], 80, 400));
        else if (cmd == "imager-xover-high" && argc == 3) workerValue(c.imager_high_crossover_hz, c.imager_request_serial, whole(argv[2], 2000, 8000));
        else if (cmd == "fet-on" && argc == 2) workerToggle(c.fet_enabled, c.fet_request_serial, 1);
        else if (cmd == "fet-off" && argc == 2) workerToggle(c.fet_enabled, c.fet_request_serial, 0);
        else if (cmd == "fet-threshold" && argc == 3) workerValue(c.fet_threshold_db, c.fet_request_serial, whole(argv[2], -48, 0));
        else if (cmd == "fet-ratio" && argc == 3)
            workerValue(c.fet_ratio_tenths, c.fet_request_serial, static_cast<int>(std::lround(number(argv[2], 1.f, 10.f) * 10.f)));
        else if (cmd == "mb-on" && argc == 2) workerToggle(c.mb_enabled, c.mb_request_serial, 1);
        else if (cmd == "mb-off" && argc == 2) workerToggle(c.mb_enabled, c.mb_request_serial, 0);
        else if (cmd == "mb-bands" && argc == 3) {
            const int value = whole(argv[2], 3, 5);
            if (value != 3 && value != 5) throw std::invalid_argument("Multiband must use 3 or 5 bands");
            workerValue(c.mb_band_count, c.mb_request_serial, value);
        }
        else if (cmd == "ddc-on" && argc == 2) workerToggle(c.ddc_enabled, c.ddc_request_serial, 1);
        else if (cmd == "ddc-off" && argc == 2) workerToggle(c.ddc_enabled, c.ddc_request_serial, 0);
        else if (cmd == "speaker-on" && argc == 2) workerToggle(c.speaker_enabled, c.speaker_request_serial, 1);
        else if (cmd == "speaker-off" && argc == 2) workerToggle(c.speaker_enabled, c.speaker_request_serial, 0);
        else if (cmd == "mono-on" && argc == 2) c.bass_mono_enabled.store(1);
        else if (cmd == "mono-off" && argc == 2) c.bass_mono_enabled.store(0);
        else if (cmd == "mono-mode" && argc == 3) c.bass_mono_mode.store(static_cast<int>(std::lround(number(argv[2],0,2))));
        else if (cmd == "mono-strength" && argc == 3) c.bass_mono_strength_centi.store(static_cast<int>(std::lround(number(argv[2],.5f,10.f)*100)));
        else if (cmd == "mono-freq" && argc == 3) c.bass_mono_frequency_hz.store(static_cast<int>(std::lround(number(argv[2],15,150))));
        else if (cmd == "psycho-on" && argc == 2) c.psycho_enabled.store(1);
        else if (cmd == "psycho-off" && argc == 2) c.psycho_enabled.store(0);
        else if (cmd == "psycho-cutoff" && argc == 3) c.psycho_cutoff_hz.store(static_cast<int>(std::lround(number(argv[2],60,150))));
        else if (cmd == "psycho-intensity" && argc == 3) c.psycho_intensity_percent.store(static_cast<int>(std::lround(number(argv[2],0,100))));
        else if (cmd == "psycho-order" && argc == 3) c.psycho_order.store(static_cast<int>(std::lround(number(argv[2],2,5))));
        else if (cmd == "psycho-original" && argc == 3) c.psycho_original_percent.store(static_cast<int>(std::lround(number(argv[2],0,100))));
        else if (cmd == "dynamic-on" && argc == 2) c.dynamic_enabled.store(1);
        else if (cmd == "dynamic-off" && argc == 2) c.dynamic_enabled.store(0);
        else if (cmd == "dynamic-x-low" && argc == 3) c.dynamic_x_low.store(static_cast<int>(std::lround(number(argv[2],0,2400))));
        else if (cmd == "dynamic-x-high" && argc == 3) c.dynamic_x_high.store(static_cast<int>(std::lround(number(argv[2],0,12000))));
        else if (cmd == "dynamic-y-low" && argc == 3) c.dynamic_y_low.store(static_cast<int>(std::lround(number(argv[2],0,200))));
        else if (cmd == "dynamic-y-high" && argc == 3) c.dynamic_y_high.store(static_cast<int>(std::lround(number(argv[2],0,300))));
        else if (cmd == "dynamic-side-low" && argc == 3) c.dynamic_side_low_percent.store(static_cast<int>(std::lround(number(argv[2],0,100))));
        else if (cmd == "dynamic-side-high" && argc == 3) c.dynamic_side_high_percent.store(static_cast<int>(std::lround(number(argv[2],0,100))));
        else if (cmd == "dynamic-strength" && argc == 3) c.dynamic_strength_percent.store(static_cast<int>(std::lround(number(argv[2],0,100))));
        else if (cmd == "clarity-on" && argc == 2) c.clarity_enabled.store(1);
        else if (cmd == "clarity-off" && argc == 2) c.clarity_enabled.store(0);
        else if (cmd == "clarity-gain" && argc == 3)
            c.clarity_gain_milli.store(static_cast<int>(std::lround(number(argv[2], 0.f, 4.5f) * 1000.f)));
        else if (cmd == "clarity-mode" && argc == 3)
            c.clarity_mode.store(static_cast<int>(std::lround(number(argv[2], 0.f, 2.f))));
        else if (cmd == "spectrum-on" && argc == 2) c.spectrum_enabled.store(1);
        else if (cmd == "spectrum-off" && argc == 2) c.spectrum_enabled.store(0);
        else if (cmd == "spectrum-freq" && argc == 3)
            c.spectrum_frequency_hz.store(static_cast<int>(std::lround(number(argv[2], 2200.f, 8200.f))));
        else if (cmd == "spectrum-exciter" && argc == 3)
            c.spectrum_exciter_milli.store(static_cast<int>(std::lround(number(argv[2], 0.f, 6.f) * 1000.f)));
        else if (cmd == "headphone-on" && argc == 2) c.headphone_enabled.store(1);
        else if (cmd == "headphone-off" && argc == 2) c.headphone_enabled.store(0);
        else if ((cmd == "headphone-level" || cmd == "headphone-quality") && argc == 3) {
            // CLI and GUI both use Levels 1..5. The shared DSP control uses
            // 0..4 internally; keep that representation unchanged for presets.
            const float level = number(argv[2], 1.f, 5.f);
            if (std::trunc(level) != level)
                throw std::invalid_argument("Headphone Surround+ level must be an integer from 1 to 5");
            c.headphone_quality.store(static_cast<int>(level) - 1);
        }
        else if (cmd == "diff-on" && argc == 2) c.diff_enabled.store(1);
        else if (cmd == "diff-off" && argc == 2) c.diff_enabled.store(0);
        else if (cmd == "diff-delay" && argc == 3)
            c.diff_delay_ms.store(static_cast<int>(std::lround(number(argv[2], 1.f, 20.f))));
        else if (cmd == "diff-reverse" && argc == 3) {
            const auto v = number(argv[2], 0.f, 1.f);
            if (std::trunc(v) != v) throw std::invalid_argument("Reverse must be 0 or 1");
            c.diff_reverse.store(static_cast<int>(v));
        }
        else if (cmd == "diff-mix" && argc == 3)
            c.diff_mix_percent.store(static_cast<int>(std::lround(number(argv[2], 0.f, 100.f))));
        else if (cmd == "diff-lowpass" && argc == 3)
            c.diff_lp_hz.store(static_cast<int>(std::lround(number(argv[2], 0.f, 20000.f))));
        else if (cmd == "limiter-threshold" && argc == 3)
            c.limiter_threshold_permille.store(static_cast<int>(std::lround(number(argv[2], 0.3f, 1.f) * 1000.f)));
        else if (cmd == "limiter-gain" && argc == 3)
            c.limiter_output_percent.store(static_cast<int>(std::lround(number(argv[2], 0.01f, 2.f) * 100.f)));
        else if (cmd == "limiter-pan" && argc == 3)
            c.limiter_pan_percent.store(static_cast<int>(std::lround(number(argv[2], -1.f, 1.f) * 100.f)));
        else if (cmd == "agc-on" && argc == 2) c.agc_enabled.store(1);
        else if (cmd == "agc-off" && argc == 2) c.agc_enabled.store(0);
        else if (cmd == "agc-strength" && argc == 3)
            c.agc_strength_centi.store(static_cast<int>(std::lround(number(argv[2], 0.5f, 3.f) * 100.f)));
        else if (cmd == "agc-max" && argc == 3)
            c.agc_max_gain_centi.store(static_cast<int>(std::lround(number(argv[2], 1.f, 10.f) * 100.f)));
        else if (cmd == "agc-threshold" && argc == 3)
            c.agc_threshold_permille.store(static_cast<int>(std::lround(number(argv[2], 0.3f, 1.f) * 1000.f)));
        else if (cmd == "lufs-on" && argc == 2) c.lufs_enabled.store(1);
        else if (cmd == "lufs-off" && argc == 2) c.lufs_enabled.store(0);
        else if (cmd == "lufs-target" && argc == 3)
            c.lufs_target_tenths.store(static_cast<int>(std::lround(number(argv[2], -24.f, -8.f) * 10.f)));
        else if (cmd == "lufs-max" && argc == 3)
            c.lufs_max_gain_tenths.store(static_cast<int>(std::lround(number(argv[2], 0.f, 12.f) * 10.f)));
        else if (cmd == "lufs-speed" && argc == 3) {
            const float value = number(argv[2], 0.f, 2.f);
            if (std::trunc(value) != value) throw std::invalid_argument("LUFS speed must be 0, 1, or 2");
            c.lufs_speed.store(static_cast<int>(value));
        }
        else if (cmd == "field-on" && argc == 2) c.field_enabled.store(1);
        else if (cmd == "field-off" && argc == 2) c.field_enabled.store(0);
        else if (cmd == "field-widen" && argc == 3)
            c.field_widen_tenths.store(static_cast<int>(std::lround(number(argv[2], 0.f, 8.f) * 10.f)));
        else if (cmd == "field-mid" && argc == 3)
            c.field_mid_hundredths.store(static_cast<int>(std::lround(number(argv[2], 1.f, 2.f) * 100.f)));
        else if (cmd == "field-depth" && argc == 3) {
            const float v = number(argv[2], 0.f, 950.f);
            if (v != 0.f && v < 200.f) throw std::invalid_argument("Field depth must be 0 or 200..950");
            c.field_depth.store(static_cast<int>(std::lround(v)));
        }
        else if (cmd == "reverb-on" && argc == 2) c.reverb_enabled.store(1);
        else if (cmd == "reverb-off" && argc == 2) c.reverb_enabled.store(0);
        else if (cmd == "reverb-room" && argc == 3)
            c.reverb_room_percent.store(static_cast<int>(std::lround(number(argv[2], 0.f, 100.f))));
        else if (cmd == "reverb-width" && argc == 3)
            c.reverb_width_percent.store(static_cast<int>(std::lround(number(argv[2], 0.f, 100.f))));
        else if (cmd == "reverb-damp" && argc == 3)
            c.reverb_damp_percent.store(static_cast<int>(std::lround(number(argv[2], 0.f, 100.f))));
        else if (cmd == "reverb-wet" && argc == 3)
            c.reverb_wet_percent.store(static_cast<int>(std::lround(number(argv[2], 0.f, 100.f))));
        else if (cmd == "reverb-dry" && argc == 3)
            c.reverb_dry_percent.store(static_cast<int>(std::lround(number(argv[2], 0.f, 100.f))));
        else { usage(); return 2; }
    } catch (const std::exception& e) {
        std::cerr << "Invalid value: " << e.what() << '\n';
        return 2;
    }
    if (cmd != "status" && cmd != "eq-preset" && !viperbass::persistCliSettings(cmd, c)) {
        std::cerr << "Live DSP controls updated, but the saved CLI settings could not be written.\n";
        return 1;
    }
    show(c, cmd, argc > 2 ? argv[2] : nullptr);
    if (cmd == "eq-preset") {
        QSettings settings("ViPERforLinux", "ViPERClean");
        std::cout << "Selected EQ preset: "
                  << settings.value("eq/selected_preset").toString().toStdString() << '\n';
    }
    return 0;
}
