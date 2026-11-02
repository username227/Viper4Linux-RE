#pragma once
// Save successful CLI edits into the *same* QSettings store used by the GUI.
// This runs in the short-lived control program, never in a PipeWire callback.
// Only the affected effect group is persisted, to avoid overwriting unrelated
// GUI state or named presets. Qt handles concurrent QSettings file locking.
#include "viper-control.h"
#include <QSettings>
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>

namespace viperbass {
inline bool persistCliSettings(const std::string& cmd, const Controls& c) {
    if (cmd == "status") return true; // Never write state during a read-only query.
    QSettings s("ViPERforLinux", "ViPERClean");
    const auto value = [](const std::atomic<std::int32_t>& v) { return v.load(std::memory_order_relaxed); };
    const auto flag = [&](const char* key, const std::atomic<std::int32_t>& v) {
        s.setValue(key, value(v) != 0);
    };
    const auto num = [&](const char* key, const std::atomic<std::int32_t>& v) {
        s.setValue(key, value(v));
    };
    const auto oneOf = [&](std::initializer_list<const char*> options) {
        for (const auto* opt : options) if (cmd == opt) return true;
        return false;
    };
    const auto begins = [&](const char* prefix) { return cmd.compare(0, std::char_traits<char>::length(prefix), prefix) == 0; };

    if (oneOf({"on", "off", "all-on", "all-off"})) {
        flag("master_enabled", c.master_enabled);
    } else if (oneOf({"bass-on", "bass-off", "bass-toggle", "toggle", "bass-mode", "bass-gain", "bass-freq"})) {
        flag("enabled", c.enabled);
        num("bass_mode", c.bass_mode);
        s.setValue("strength_tenths", (value(c.strength_centi) + 5) / 10);
        num("frequency", c.frequency_hz);
    } else if (begins("mono-")) {
        flag("bass_mono_enabled", c.bass_mono_enabled);
        num("bass_mono_mode", c.bass_mono_mode);
        s.setValue("bass_mono_strength_centi", ((value(c.bass_mono_strength_centi) + 5) / 10) * 10);
        num("bass_mono_frequency_hz", c.bass_mono_frequency_hz);
    } else if (begins("psycho-")) {
        flag("psycho_enabled", c.psycho_enabled);
        num("psycho_cutoff_hz", c.psycho_cutoff_hz);
        num("psycho_intensity_percent", c.psycho_intensity_percent);
        num("psycho_order", c.psycho_order);
        num("psycho_original_percent", c.psycho_original_percent);
    } else if (begins("dynamic-")) {
        flag("dynamic_enabled", c.dynamic_enabled);
        num("dynamic_x_low", c.dynamic_x_low);
        num("dynamic_x_high", c.dynamic_x_high);
        num("dynamic_y_low", c.dynamic_y_low);
        num("dynamic_y_high", c.dynamic_y_high);
        num("dynamic_side_low_percent", c.dynamic_side_low_percent);
        num("dynamic_side_high_percent", c.dynamic_side_high_percent);
        num("dynamic_strength_percent", c.dynamic_strength_percent);
    } else if (begins("eq-")) {
        flag("eq_enabled", c.eq_enabled);
        const int bands = value(c.eq_band_count);
        s.setValue("eq_band_count", bands);
        if (cmd == "eq-bands" || cmd == "eq-level") {
            s.setValue("eq/selected_preset", "custom");
            std::string levels;
            for (int i = 0; i < bands; ++i) {
                if (i) levels += ',';
                levels += std::to_string(value(c.eq_levels_hundredths[i]));
            }
            s.setValue(("eq_levels_" + std::to_string(bands)).c_str(), levels.c_str());
        }
    } else if (begins("deq-")) {
        flag("deq_enabled", c.deq_enabled);
    } else if (begins("clarity-")) {
        flag("clarity_enabled", c.clarity_enabled);
        num("clarity_mode", c.clarity_mode);
        s.setValue("clarity_gain_tenths", (value(c.clarity_gain_milli) + 50) / 100);
    } else if (begins("spectrum-")) {
        flag("spectrum_enabled", c.spectrum_enabled);
        s.setValue("spectrum_frequency_hundreds", (value(c.spectrum_frequency_hz) + 50) / 100);
        s.setValue("spectrum_exciter_tenths", (value(c.spectrum_exciter_milli) + 50) / 100);
    } else if (begins("tube-")) {
        flag("tube_enabled", c.tube_enabled);
        num("tube_model", c.tube_model);
        num("tube_drive_percent", c.tube_drive_percent);
        num("tube_mix_percent", c.tube_mix_percent);
    } else if (begins("analogx-")) {
        flag("analogx_enabled", c.analogx_enabled);
        num("analogx_mode", c.analogx_mode);
    } else if (begins("crossfeed-")) {
        flag("crossfeed_enabled", c.crossfeed_enabled);
        num("crossfeed_preset", c.crossfeed_preset);
    } else if (begins("headphone-")) {
        flag("headphone_enabled", c.headphone_enabled);
        num("headphone_quality", c.headphone_quality);
    } else if (begins("diff-")) {
        flag("diff_enabled", c.diff_enabled);
        num("diff_delay_ms", c.diff_delay_ms);
        flag("diff_reverse", c.diff_reverse);
        num("diff_mix_percent", c.diff_mix_percent);
        s.setValue("diff_lp_hundreds", (value(c.diff_lp_hz) + 50) / 100);
    } else if (begins("field-")) {
        flag("field_enabled", c.field_enabled);
        num("field_widen_tenths", c.field_widen_tenths);
        num("field_mid_hundredths", c.field_mid_hundredths);
        const int depth = value(c.field_depth);
        s.setValue("field_depth_step", depth == 0 ? 0 : std::clamp((depth - 150 + 25) / 50, 1, 16));
    } else if (begins("reverb-")) {
        flag("reverb_enabled", c.reverb_enabled);
        num("reverb_room_percent", c.reverb_room_percent);
        num("reverb_width_percent", c.reverb_width_percent);
        num("reverb_damp_percent", c.reverb_damp_percent);
        num("reverb_wet_percent", c.reverb_wet_percent);
        num("reverb_dry_percent", c.reverb_dry_percent);
    } else if (begins("imager-")) {
        flag("imager_enabled", c.imager_enabled);
        num("imager_low_width_percent", c.imager_low_width_percent);
        num("imager_mid_width_percent", c.imager_mid_width_percent);
        num("imager_high_width_percent", c.imager_high_width_percent);
        num("imager_low_crossover_hz", c.imager_low_crossover_hz);
        num("imager_high_crossover_hz", c.imager_high_crossover_hz);
    } else if (begins("fet-")) {
        flag("fet_enabled", c.fet_enabled);
        num("fet_threshold_db", c.fet_threshold_db);
        num("fet_ratio_tenths", c.fet_ratio_tenths);
    } else if (begins("mb-")) {
        flag("mb_enabled", c.mb_enabled);
        num("mb_band_count", c.mb_band_count);
    } else if (begins("ddc-")) {
        flag("ddc_enabled", c.ddc_enabled);
    } else if (begins("speaker-")) {
        flag("speaker_enabled", c.speaker_enabled);
    } else if (begins("convolver-")) {
        flag("convolver_enabled", c.convolver_enabled);
        num("convolver_cross_percent", c.convolver_cross_percent);
    } else if (begins("limiter-")) {
        num("limiter_threshold_permille", c.limiter_threshold_permille);
        num("limiter_output_percent", c.limiter_output_percent);
        num("limiter_pan_percent", c.limiter_pan_percent);
    } else if (begins("agc-")) {
        flag("agc_enabled", c.agc_enabled);
        num("agc_strength_centi", c.agc_strength_centi);
        num("agc_max_gain_centi", c.agc_max_gain_centi);
        num("agc_threshold_permille", c.agc_threshold_permille);
    } else if (begins("lufs-")) {
        flag("lufs_enabled", c.lufs_enabled);
        num("lufs_target_tenths", c.lufs_target_tenths);
        num("lufs_max_gain_tenths", c.lufs_max_gain_tenths);
        num("lufs_speed", c.lufs_speed);
    } else {
        return false; // Fail closed if new CLI commands lack persistence coverage.
    }
    s.sync();
    return s.status() == QSettings::NoError;
}
} // namespace viperbass
