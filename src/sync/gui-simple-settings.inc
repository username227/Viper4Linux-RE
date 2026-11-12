// Simple GUI preference pairs only: one declaration supplies both read and write.
// Included inside ViperWindow; special-case ordering and mappings stay explicit.
    enum class GuiSettingPhase { Intro, DynamicTail, Tone, Dynamics, SpatialOutput };
    struct GuiSettingBinding {
        GuiSettingPhase phase;
        const char* key;
        std::variant<QCheckBox*, QSlider*, QComboBox*> widget;
        int fallback;
        int lower = 0, upper = 0;
        bool bounded = false;
    };
    void syncSimpleGuiSettings(GuiSettingPhase phase, bool loading) {
        using Phase = GuiSettingPhase;
        const GuiSettingBinding bindings[] = {
            // Intro
            {Phase::Intro, "master_enabled", global_, 1},
            {Phase::Intro, "enabled", enabled_, 1},
            {Phase::Intro, "bass_mode", bass_mode_, 0, 0, 2, true},
            {Phase::Intro, "strength_tenths", strength_.slider, 100},
            {Phase::Intro, "frequency", frequency_.slider, 65},
            // DynamicTail
            {Phase::DynamicTail, "dynamic_x_high", dyn_xhi_.slider, 6000, 0, 12000, true},
            {Phase::DynamicTail, "dynamic_y_low", dyn_ylo_.slider, 100, 0, 200, true},
            {Phase::DynamicTail, "dynamic_y_high", dyn_yhi_.slider, 150, 0, 300, true},
            {Phase::DynamicTail, "dynamic_side_low_percent", dyn_side_lo_.slider, 50, 0, 100, true},
            {Phase::DynamicTail, "dynamic_side_high_percent", dyn_side_hi_.slider, 50, 0, 100, true},
            {Phase::DynamicTail, "dynamic_strength_percent", dyn_strength_.slider, 0, 0, 100, true},
            // Tone
            {Phase::Tone, "clarity_enabled", clarity_enabled_, 0},
            {Phase::Tone, "clarity_mode", clarity_mode_, 0, 0, 2, true},
            {Phase::Tone, "clarity_gain_tenths", clarity_gain_.slider, 15},
            {Phase::Tone, "spectrum_enabled", spectrum_enabled_, 0},
            {Phase::Tone, "spectrum_frequency_hundreds", spectrum_frequency_.slider, 76},
            {Phase::Tone, "spectrum_exciter_tenths", spectrum_exciter_.slider, 10},
            {Phase::Tone, "tube_enabled", tube_enabled_, 0},
            {Phase::Tone, "tube_model", tube_model_, 0, 0, 2, true},
            {Phase::Tone, "tube_drive_percent", tube_drive_.slider, 50, 0, 100, true},
            {Phase::Tone, "tube_mix_percent", tube_mix_.slider, 30, 0, 100, true},
            {Phase::Tone, "analogx_enabled", analogx_enabled_, 0},
            {Phase::Tone, "analogx_mode", analogx_mode_, 0, 0, 2, true},
            {Phase::Tone, "crossfeed_enabled", crossfeed_enabled_, 0},
            {Phase::Tone, "crossfeed_preset", crossfeed_mode_, 0, 0, 2, true},
            // Dynamics
            {Phase::Dynamics, "fet_enabled", fet_enabled_, 0},
            {Phase::Dynamics, "fet_threshold_db", fet_threshold_.slider, -18, -48, 0, true},
            {Phase::Dynamics, "fet_ratio_tenths", fet_ratio_.slider, 20, 10, 100, true},
            {Phase::Dynamics, "fet_knee_db", fet_knee_.slider, 6, 0, 12, true},
            {Phase::Dynamics, "fet_makeup_db", fet_gain_.slider, 0, 0, 24, true},
            {Phase::Dynamics, "fet_attack_ms", fet_attack_.slider, 10, 1, 100, true},
            {Phase::Dynamics, "fet_release_ms", fet_release_.slider, 100, 5, 500, true},
            {Phase::Dynamics, "imager_enabled", imager_enabled_, 0},
            {Phase::Dynamics, "imager_low_width_percent", imager_low_.slider, 100, 0, 200, true},
            {Phase::Dynamics, "imager_mid_width_percent", imager_mid_.slider, 100, 0, 200, true},
            {Phase::Dynamics, "imager_high_width_percent", imager_high_.slider, 100, 0, 200, true},
            {Phase::Dynamics, "imager_low_crossover_hz", imager_low_xover_.slider, 200, 80, 400, true},
            // SpatialOutput
            {Phase::SpatialOutput, "headphone_enabled", headphone_enabled_, 0},
            {Phase::SpatialOutput, "headphone_quality", headphone_quality_, 0, 0, 4, true},
            {Phase::SpatialOutput, "diff_enabled", diff_enabled_, 0},
            {Phase::SpatialOutput, "diff_delay_ms", diff_delay_.slider, 8, 1, 20, true},
            {Phase::SpatialOutput, "diff_reverse", diff_reverse_, 0},
            {Phase::SpatialOutput, "diff_mix_percent", diff_mix_.slider, 50, 0, 100, true},
            {Phase::SpatialOutput, "diff_lp_hundreds", diff_lowpass_.slider, 0, 0, 200, true},
            {Phase::SpatialOutput, "field_enabled", field_enabled_, 0},
            {Phase::SpatialOutput, "field_widen_tenths", field_widen_.slider, 10, 0, 80, true},
            {Phase::SpatialOutput, "field_mid_hundredths", field_mid_.slider, 100, 100, 200, true},
            {Phase::SpatialOutput, "field_depth_step", field_depth_.slider, 3, 0, 16, true},
            {Phase::SpatialOutput, "reverb_enabled", reverb_enabled_, 0},
            {Phase::SpatialOutput, "reverb_room_percent", reverb_room_.slider, 50, 0, 100, true},
            {Phase::SpatialOutput, "reverb_width_percent", reverb_width_.slider, 50, 0, 100, true},
            {Phase::SpatialOutput, "reverb_damp_percent", reverb_damp_.slider, 50, 0, 100, true},
            {Phase::SpatialOutput, "reverb_wet_percent", reverb_wet_.slider, 20, 0, 100, true},
            {Phase::SpatialOutput, "reverb_dry_percent", reverb_dry_.slider, 50, 0, 100, true},
            {Phase::SpatialOutput, "limiter_threshold_permille", limiter_threshold_.slider, 500, 300, 1000, true},
            {Phase::SpatialOutput, "limiter_output_percent", limiter_gain_.slider, 100, 1, 200, true},
            {Phase::SpatialOutput, "limiter_pan_percent", limiter_pan_.slider, 0, -100, 100, true},
            {Phase::SpatialOutput, "agc_enabled", agc_enabled_, 0},
            {Phase::SpatialOutput, "agc_strength_centi", agc_strength_.slider, 100, 50, 300, true},
            {Phase::SpatialOutput, "agc_max_gain_centi", agc_max_gain_.slider, 300, 100, 1000, true},
            {Phase::SpatialOutput, "agc_threshold_permille", agc_threshold_.slider, 900, 300, 1000, true},
            {Phase::SpatialOutput, "lufs_enabled", lufs_enabled_, 0},
            {Phase::SpatialOutput, "lufs_target_tenths", lufs_target_.slider, -140, -240, -80, true},
            {Phase::SpatialOutput, "lufs_max_gain_tenths", lufs_gain_.slider, 60, 0, 120, true},
            {Phase::SpatialOutput, "lufs_speed", lufs_speed_, 1, 0, 2, true},
        };
        for (const auto& binding : bindings) {
            if (binding.phase != phase) continue;
            const QString key = QString::fromLatin1(binding.key);
            // The widget's static class determines its behavior. A mismatched
            // control type is rejected by std::variant at compile time.
            std::visit([&](auto* widget) {
                using Widget = std::remove_pointer_t<decltype(widget)>;
                if (loading) {
                    if constexpr (std::is_same_v<Widget, QCheckBox>) {
                        widget->setChecked(
                            settings_.value(key, bool(binding.fallback)).toBool());
                    } else {
                        int value = settings_.value(key, binding.fallback).toInt();
                        if (binding.bounded)
                            value = std::clamp(value, binding.lower, binding.upper);
                        if constexpr (std::is_same_v<Widget, QSlider>)
                            widget->setValue(value);
                        else
                            widget->setCurrentIndex(value);
                    }
                } else {
                    if constexpr (std::is_same_v<Widget, QCheckBox>)
                        settings_.setValue(key, widget->isChecked());
                    else if constexpr (std::is_same_v<Widget, QSlider>)
                        settings_.setValue(key, widget->value());
                    else
                        settings_.setValue(key, widget->currentIndex());
                }
            }, binding.widget);
        }
    }
