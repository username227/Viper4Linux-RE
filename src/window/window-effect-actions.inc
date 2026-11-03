// Private ViperWindow member definitions. Included once inside the class.
// Checkpoint 7 mechanical extraction; no additional Qt or DSP state owner.
    void applySettings(bool master, bool bass, int bass_mode, int strength, int frequency,
                       bool clarity_on, int clarity_mode, int clarity_gain,
                       bool spectrum_on, int spectrum_frequency, int spectrum_exciter,
                       bool headphone_on, int headphone_quality,
                       bool diff_on, int diff_delay, bool diff_reverse, int diff_mix, int diff_lp,
                       bool field_on, int field_widen, int field_mid, int field_depth_step,
                       bool reverb_on, int reverb_room, int reverb_width, int reverb_damp, int reverb_wet, int reverb_dry) {
        // Loading a complete named preset or resetting to defaults explicitly
        // replaces the previous full-precision CLI values.
        settings_.remove("precision");
        replace_precision_on_push_ = true;
        // Suppress intermediate updates: don't send four DSP changes per reset.
        const QSignalBlocker block0(global_);
        const QSignalBlocker block1(enabled_);
        const QSignalBlocker blockBassMode(bass_mode_);
        const QSignalBlocker block2(strength_.slider);
        const QSignalBlocker block3(frequency_.slider);
        const QSignalBlocker block5(clarity_enabled_);
        const QSignalBlocker block6(clarity_mode_);
        const QSignalBlocker block7(clarity_gain_.slider);
        const QSignalBlocker block8(spectrum_enabled_);
        const QSignalBlocker block9(spectrum_frequency_.slider);
        const QSignalBlocker block10(spectrum_exciter_.slider);
        const QSignalBlocker block11(headphone_enabled_);
        const QSignalBlocker block12(headphone_quality_);
        const QSignalBlocker block13(diff_enabled_);
        const QSignalBlocker block14(diff_delay_.slider);
        const QSignalBlocker block15(diff_reverse_);
        const QSignalBlocker block16(diff_mix_.slider);
        const QSignalBlocker block17(diff_lowpass_.slider);
        const QSignalBlocker block18(field_enabled_);
        const QSignalBlocker block19(field_widen_.slider);
        const QSignalBlocker block20(field_mid_.slider);
        const QSignalBlocker block21(field_depth_.slider);
        const QSignalBlocker block22(reverb_enabled_);
        const QSignalBlocker block23(reverb_room_.slider);
        const QSignalBlocker block24(reverb_width_.slider);
        const QSignalBlocker block25(reverb_damp_.slider);
        const QSignalBlocker block26(reverb_wet_.slider);
        const QSignalBlocker block27(reverb_dry_.slider);
        global_->setChecked(master);
        enabled_->setChecked(bass);
        bass_mode_->setCurrentIndex(std::clamp(bass_mode, 0, 2));
        strength_.slider->setValue(std::clamp(strength, 5, 100));
        frequency_.slider->setValue(std::clamp(frequency, 15, 150));
        clarity_enabled_->setChecked(clarity_on);
        clarity_mode_->setCurrentIndex(std::clamp(clarity_mode, 0, 2));
        clarity_gain_.slider->setValue(std::clamp(clarity_gain, 0, 45));
        spectrum_enabled_->setChecked(spectrum_on);
        spectrum_frequency_.slider->setValue(std::clamp(spectrum_frequency, 22, 82));
        spectrum_exciter_.slider->setValue(std::clamp(spectrum_exciter, 0, 60));
        headphone_enabled_->setChecked(headphone_on);
        headphone_quality_->setCurrentIndex(std::clamp(headphone_quality, 0, 4));
        diff_enabled_->setChecked(diff_on);
        diff_delay_.slider->setValue(std::clamp(diff_delay, 1, 20));
        diff_reverse_->setChecked(diff_reverse);
        diff_mix_.slider->setValue(std::clamp(diff_mix, 0, 100));
        diff_lowpass_.slider->setValue(std::clamp(diff_lp, 0, 200));
        field_enabled_->setChecked(field_on);
        field_widen_.slider->setValue(std::clamp(field_widen, 0, 80));
        field_mid_.slider->setValue(std::clamp(field_mid, 100, 200));
        field_depth_.slider->setValue(std::clamp(field_depth_step, 0, 16));
        reverb_enabled_->setChecked(reverb_on);
        reverb_room_.slider->setValue(std::clamp(reverb_room, 0, 100));
        reverb_width_.slider->setValue(std::clamp(reverb_width, 0, 100));
        reverb_damp_.slider->setValue(std::clamp(reverb_damp, 0, 100));
        reverb_wet_.slider->setValue(std::clamp(reverb_wet, 0, 100));
        reverb_dry_.slider->setValue(std::clamp(reverb_dry, 0, 100));
        updateLabels();
        push();
    }
    void saveBassPreset() {
        for (const auto& item : std::initializer_list<std::pair<QString, int>>{
            {"bass_mono_enabled", mono_enabled_->isChecked()}, {"bass_mono_mode", mono_mode_->currentIndex()},
            {"bass_mono_strength_centi", mono_strength_.slider->value()*10},
            {"bass_mono_frequency_hz", mono_frequency_.slider->value()},
            {"psycho_enabled", psycho_enabled_->isChecked()}, {"psycho_cutoff_hz", psycho_cutoff_.slider->value()},
            {"psycho_intensity_percent", psycho_intensity_.slider->value()}, {"psycho_order", psycho_order_->currentIndex()+2},
            {"psycho_original_percent", psycho_original_.slider->value()},
            {"dynamic_enabled", dyn_enabled_->isChecked()}, {"dynamic_x_low", dyn_xlo_.slider->value()},
            {"dynamic_x_high", dyn_xhi_.slider->value()}, {"dynamic_y_low", dyn_ylo_.slider->value()},
            {"dynamic_y_high", dyn_yhi_.slider->value()},
            {"dynamic_side_low_percent", dyn_side_lo_.slider->value()},
            {"dynamic_side_high_percent", dyn_side_hi_.slider->value()},
            {"dynamic_strength_percent", dyn_strength_.slider->value()} }) {
            settings_.setValue("preset/" + item.first, item.second);
        }
    }
    void applyBassExtra(int mono, int monoMode, int monoStrength, int monoFreq,
                        int psycho, int cutoff, int intensity, int order, int original,
                        int dynamic, int xlo, int xhi, int ylo, int yhi, int sideLo, int sideHi, int strength) {
        const QSignalBlocker blocks[] = {
            QSignalBlocker(mono_enabled_), QSignalBlocker(mono_mode_),
            QSignalBlocker(mono_strength_.slider), QSignalBlocker(mono_frequency_.slider),
            QSignalBlocker(psycho_enabled_), QSignalBlocker(psycho_cutoff_.slider),
            QSignalBlocker(psycho_intensity_.slider), QSignalBlocker(psycho_order_),
            QSignalBlocker(psycho_original_.slider), QSignalBlocker(dyn_enabled_),
            QSignalBlocker(dyn_xlo_.slider), QSignalBlocker(dyn_xhi_.slider),
            QSignalBlocker(dyn_ylo_.slider), QSignalBlocker(dyn_yhi_.slider),
            QSignalBlocker(dyn_side_lo_.slider), QSignalBlocker(dyn_side_hi_.slider),
            QSignalBlocker(dyn_strength_.slider)
        };
        mono_enabled_->setChecked(mono != 0);
        mono_mode_->setCurrentIndex(std::clamp(monoMode, 0, 2));
        mono_strength_.slider->setValue(std::clamp(monoStrength, 50, 1000)/10);
        mono_frequency_.slider->setValue(std::clamp(monoFreq, 15, 150));
        psycho_enabled_->setChecked(psycho != 0);
        psycho_cutoff_.slider->setValue(std::clamp(cutoff, 60, 150));
        psycho_intensity_.slider->setValue(std::clamp(intensity, 0, 100));
        psycho_order_->setCurrentIndex(std::clamp(order, 2, 5)-2);
        psycho_original_.slider->setValue(std::clamp(original, 0, 100));
        dyn_enabled_->setChecked(dynamic != 0);
        dyn_xlo_.slider->setValue(std::clamp(xlo, 0, 2400));
        dyn_xhi_.slider->setValue(std::clamp(xhi, 0, 12000));
        dyn_ylo_.slider->setValue(std::clamp(ylo, 0, 200));
        dyn_yhi_.slider->setValue(std::clamp(yhi, 0, 300));
        dyn_side_lo_.slider->setValue(std::clamp(sideLo, 0, 100));
        dyn_side_hi_.slider->setValue(std::clamp(sideHi, 0, 100));
        dyn_strength_.slider->setValue(std::clamp(strength, 0, 100));
        updateLabels();
        push();
    }
    void resetNewBassControls() {
        applyBassExtra(0,0,200,65,0,80,30,3,100,0,100,6000,100,150,50,50,0);
    }
    void restoreBassPreset() {
        auto get = [this](const char* key,int defaultValue){return settings_.value(QString("preset/")+key,defaultValue).toInt();};
        applyBassExtra(get("bass_mono_enabled",0),get("bass_mono_mode",0),get("bass_mono_strength_centi",200),
                       get("bass_mono_frequency_hz",65),get("psycho_enabled",0),get("psycho_cutoff_hz",80),
                       get("psycho_intensity_percent",30),get("psycho_order",3),get("psycho_original_percent",100),
                       get("dynamic_enabled",0),get("dynamic_x_low",100),get("dynamic_x_high",6000),
                       get("dynamic_y_low",100),get("dynamic_y_high",150),get("dynamic_side_low_percent",50),
                       get("dynamic_side_high_percent",50),get("dynamic_strength_percent",0));
    }
    // Output fields are separate from the older seven-effect preset schema;
    // restore them atomically from a logical UI operation, not one slider at a time.
    void applyOutputSettings(int threshold, int gain, int pan, bool agc, int agcStrength,
                             int agcMaxGain, int agcThreshold, bool lufs,
                             int lufsTarget, int lufsMaxGain, int speed) {
        const QSignalBlocker b0(limiter_threshold_.slider), b1(limiter_gain_.slider), b2(limiter_pan_.slider);
        const QSignalBlocker b3(agc_enabled_), b4(agc_strength_.slider), b5(agc_max_gain_.slider);
        const QSignalBlocker b6(agc_threshold_.slider), b7(lufs_enabled_), b8(lufs_target_.slider);
        const QSignalBlocker b9(lufs_gain_.slider), b10(lufs_speed_);
        limiter_threshold_.slider->setValue(std::clamp(threshold, 300, 1000));
        limiter_gain_.slider->setValue(std::clamp(gain, 1, 200));
        limiter_pan_.slider->setValue(std::clamp(pan, -100, 100));
        agc_enabled_->setChecked(agc);
        agc_strength_.slider->setValue(std::clamp(agcStrength, 50, 300));
        agc_max_gain_.slider->setValue(std::clamp(agcMaxGain, 100, 1000));
        agc_threshold_.slider->setValue(std::clamp(agcThreshold, 300, 1000));
        lufs_enabled_->setChecked(lufs);
        lufs_target_.slider->setValue(std::clamp(lufsTarget, -240, -80));
        lufs_gain_.slider->setValue(std::clamp(lufsMaxGain, 0, 120));
        lufs_speed_->setCurrentIndex(std::clamp(speed, 0, 2));
        updateLabels();
        push();
    }
    void saveOutputPreset() {
        settings_.setValue("preset/limiter_threshold_permille", limiter_threshold_.slider->value());
        settings_.setValue("preset/limiter_output_percent", limiter_gain_.slider->value());
        settings_.setValue("preset/limiter_pan_percent", limiter_pan_.slider->value());
        settings_.setValue("preset/agc_enabled", agc_enabled_->isChecked());
        settings_.setValue("preset/agc_strength_centi", agc_strength_.slider->value());
        settings_.setValue("preset/agc_max_gain_centi", agc_max_gain_.slider->value());
        settings_.setValue("preset/agc_threshold_permille", agc_threshold_.slider->value());
        settings_.setValue("preset/lufs_enabled", lufs_enabled_->isChecked());
        settings_.setValue("preset/lufs_target_tenths", lufs_target_.slider->value());
        settings_.setValue("preset/lufs_max_gain_tenths", lufs_gain_.slider->value());
        settings_.setValue("preset/lufs_speed", lufs_speed_->currentIndex());
    }
    void restoreOutputPreset() {
        applyOutputSettings(settings_.value("preset/limiter_threshold_permille", 500).toInt(),
            settings_.value("preset/limiter_output_percent", 100).toInt(),
            settings_.value("preset/limiter_pan_percent", 0).toInt(),
            settings_.value("preset/agc_enabled", false).toBool(),
            settings_.value("preset/agc_strength_centi", 100).toInt(),
            settings_.value("preset/agc_max_gain_centi", 300).toInt(),
            settings_.value("preset/agc_threshold_permille", 900).toInt(),
            settings_.value("preset/lufs_enabled", false).toBool(),
            settings_.value("preset/lufs_target_tenths", -140).toInt(),
            settings_.value("preset/lufs_max_gain_tenths", 60).toInt(),
            settings_.value("preset/lufs_speed", 1).toInt());
    }
    void resetOutputControls() {
        applyOutputSettings(500, 100, 0, false, 100, 300, 900, false, -140, 60, 1);
    }
