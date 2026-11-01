#include "viper-control.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void usage() {
    std::cerr << "Usage: viper-ctl status | on | off | toggle | "
                 "all-on | all-off | bass-mode <0..2> | gain <0.5..10> | freq <15..150 Hz> | "
                 "mono-on | mono-off | mono-mode <0..2> | mono-strength <0.5..10> | mono-freq <15..150> | "
                 "psycho-on | psycho-off | psycho-cutoff <60..150> | psycho-intensity <0..100> | psycho-order <2..5> | psycho-original <0..100> | "
                 "dynamic-on | dynamic-off | dynamic-x-low <0..2400> | dynamic-x-high <0..12000> | dynamic-y-low <0..200> | dynamic-y-high <0..300> | dynamic-side-low <0..100> | dynamic-side-high <0..100> | dynamic-strength <0..100> | "
                 "clarity-on | clarity-off | clarity-gain <0..4.5> | clarity-mode <0..2> | "
                 "spectrum-on | spectrum-off | spectrum-freq <2200..8200 Hz> | spectrum-exciter <0..6> | "
                 "headphone-on | headphone-off | headphone-quality <0..4> | "
                 "diff-on | diff-off | diff-delay <1..20 ms> | diff-reverse <0|1> | "
                 "diff-mix <0..100 percent> | diff-lowpass <0..20000 Hz> | "
                 "field-on | field-off | field-widen <0..8> | "
                 "field-mid <1..2> | field-depth <0 or 200..950> | "
                 "reverb-on | reverb-off | reverb-room <0..100> | "
                 "reverb-width <0..100> | reverb-damp <0..100> | "
                 "reverb-wet <0..100> | reverb-dry <0..100> | "
                 "limiter-threshold <0.3..1> | limiter-gain <0.01..2> | limiter-pan <-1..1> | "
                 "agc-on | agc-off | agc-strength <0.5..3> | agc-max <1..10> | agc-threshold <0.3..1> | "
                 "lufs-on | lufs-off | lufs-target <-24..-8> | lufs-max <0..12> | lufs-speed <0..2> | "
                 "eq-on | eq-off | eq-bands <10|15|25|31> | eq-level <1..31> <-12..12 dB> | "
                 "convolver-on | convolver-off | convolver-cross <0..100 percent>\n";
}
float number(const char* value, float lower, float upper) {
    std::size_t parsed = 0;
    const float result = std::stof(value, &parsed);
    if (value[parsed] || !std::isfinite(result) || result < lower || result > upper)
        throw std::invalid_argument("Value outside permitted range");
    return result;
}
void show(viperbass::Controls& c) {
    std::cout << "ViPER processing: " << (c.master_enabled.load() ? "ON" : "BYPASSED")
              << " | Bass: " << (c.enabled.load() ? "ON" : "OFF")
              << " | Bass mode " << ([&]{ switch (c.bass_mode.load()) { case 0: return "Natural"; case 1: return "Pure Bass+"; case 2: return "Subwoofer"; default: return "Unknown"; } }())
              << " | strength " << (c.strength_centi.load() / 100.0f)
              << " | frequency " << c.frequency_hz.load() << " Hz"
              << " | Bass Mono: " << (c.bass_mono_enabled.load() ? "ON" : "OFF")
              << " | Psychoacoustic Bass: " << (c.psycho_enabled.load() ? "ON" : "OFF")
              << " | Dynamic System: " << (c.dynamic_enabled.load() ? "ON" : "OFF")
              << " | Clarity: " << (c.clarity_enabled.load() ? "ON" : "OFF")
              << " | Clarity mode " << c.clarity_mode.load()
              << " | Clarity strength " << (c.clarity_gain_milli.load() / 1000.0f)
              << " | Spectrum: " << (c.spectrum_enabled.load() ? "ON" : "OFF")
              << " | Spectrum reference " << c.spectrum_frequency_hz.load() << " Hz"
              << " | Harmonic strength " << (c.spectrum_exciter_milli.load() / 1000.0f)
              << " | Headphone Surround+: " << (c.headphone_enabled.load() ? "ON" : "OFF")
              << " | quality level " << c.headphone_quality.load()
              << " | Differential Surround: " << (c.diff_enabled.load() ? "ON" : "OFF")
              << " | delay " << c.diff_delay_ms.load() << " ms"
              << " | reverse " << c.diff_reverse.load()
              << " | mix " << c.diff_mix_percent.load() << "%"
              << " | low-pass " << c.diff_lp_hz.load() << " Hz"
              << " | Field Surround: " << (c.field_enabled.load() ? "ON" : "OFF")
              << " | widening " << c.field_widen_tenths.load() / 10.0f
              << " | mid image " << c.field_mid_hundredths.load() / 100.0f
              << " | depth " << c.field_depth.load()
              << " | Reverberation: " << (c.reverb_enabled.load() ? "ON" : "OFF")
              << " | room " << c.reverb_room_percent.load() << "%"
              << " | width " << c.reverb_width_percent.load() << "%"
              << " | damp " << c.reverb_damp_percent.load() << "%"
              << " | wet " << c.reverb_wet_percent.load() << "%"
              << " | dry " << c.reverb_dry_percent.load() << "%"
              << " | Limiter: ALWAYS ON | threshold " << c.limiter_threshold_permille.load() / 1000.0f
              << " | output " << c.limiter_output_percent.load() << "%"
              << " | pan " << c.limiter_pan_percent.load() << "%"
              << " | AGC: " << (c.agc_enabled.load() ? "ON" : "OFF")
              << " | strength " << c.agc_strength_centi.load() / 100.0f
              << " | max " << c.agc_max_gain_centi.load() / 100.0f
              << " | threshold " << c.agc_threshold_permille.load() / 1000.0f
              << " | LUFS: " << (c.lufs_enabled.load() ? "ON" : "OFF")
              << " | target " << c.lufs_target_tenths.load() / 10.0f
              << " | max adjustment " << c.lufs_max_gain_tenths.load() / 10.0f << " dB"
              << " | response speed " << c.lufs_speed.load()
              << " | EQ: " << (c.eq_enabled.load() ? "ON" : "OFF")
              << " | EQ bands " << c.eq_band_count.load()
              << " | EQ applied " << c.eq_applied_serial.load() << "/" << c.eq_request_serial.load()
              << " | Convolver: " << (c.convolver_enabled.load() ? "ON" : "OFF")
              << " | IR: " << ([&]{ switch(c.convolver_status.load()) {
                     case 1: return "READY";
                     case 2: return "LOADING";
                     case -1: return "INVALID";
                     case -2: return "RESAMPLE ERROR";
                     default: return "NONE";
                 } }())
              << " | cross " << c.convolver_cross_percent.load() << "%"
              << " | processing rate " << c.sample_rate_hz.load() << " Hz"
              << " | backend PID " << c.owner_pid.load()
              << " | audio blocks " << c.audio_callbacks.load() << '\n';
}
}
int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 2; }
    viperbass::Mapping mapping;
    if (!mapping.attach()) {
        std::cerr << "No clean ViPER control channel. If ./start.sh is running, start audio playback and see ./status.sh.\n";
        return 1;
    }
    auto& c = *mapping.get();
    const std::string cmd(argv[1]);
    try {
        if (cmd == "status" && argc == 2) { show(c); return 0; }
        if (cmd == "all-on" && argc == 2) c.master_enabled.store(1);
        else if (cmd == "all-off" && argc == 2) c.master_enabled.store(0);
        else if (cmd == "on" && argc == 2) c.enabled.store(1);
        else if (cmd == "off" && argc == 2) c.enabled.store(0);
        else if (cmd == "toggle" && argc == 2) c.enabled.store(!c.enabled.load());
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
        else if (cmd == "headphone-quality" && argc == 3)
            c.headphone_quality.store(static_cast<int>(std::lround(number(argv[2], 0.f, 4.f))));
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
    show(c);
    return 0;
}
