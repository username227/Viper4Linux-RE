// Implementation is included in ViperWindow to retain Qt UI ownership
// and the original 300ms QSettings debounce and persistence ordering.
    void persistCurrentSettings() {
        syncSimpleGuiSettings(GuiSettingPhase::Intro, false);
        saveEqLayout();
        settings_.setValue("eq_enabled", eq_enabled_->isChecked());
        settings_.setValue("eq_band_count", eq_active_bands_);
        settings_.setValue("eq/selected_preset", eq_selected_preset_);
        settings_.setValue("ddc_enabled", ddc_enabled_->isChecked());
        settings_.setValue("speaker_enabled", speaker_enabled_->isChecked());
        settings_.setValue("convolver_enabled", convolver_enabled_->isChecked());
        settings_.setValue("convolver_cross_percent", convolver_cross_.slider->value());
        settings_.setValue("bass_mono_enabled", mono_enabled_->isChecked());
        settings_.setValue("bass_mono_mode", mono_mode_->currentIndex());
        settings_.setValue("bass_mono_strength_centi", mono_strength_.slider->value()*10);
        settings_.setValue("bass_mono_frequency_hz", mono_frequency_.slider->value());
        settings_.setValue("psycho_enabled", psycho_enabled_->isChecked());
        settings_.setValue("psycho_cutoff_hz", psycho_cutoff_.slider->value());
        settings_.setValue("psycho_intensity_percent", psycho_intensity_.slider->value());
        settings_.setValue("psycho_order", psycho_order_->currentIndex()+2);
        settings_.setValue("psycho_original_percent", psycho_original_.slider->value());
        settings_.setValue("dynamic_enabled", dyn_enabled_->isChecked());
        settings_.setValue("dynamic_x_low", dyn_xlo_native_);
        syncSimpleGuiSettings(GuiSettingPhase::DynamicTail, false);
        syncSimpleGuiSettings(GuiSettingPhase::Tone, false);
        saveMultibandSettings("");
        syncSimpleGuiSettings(GuiSettingPhase::Dynamics, false);
        settings_.setValue("imager_high_crossover_hz", imager_high_xover_.slider->value() * 100);
        settings_.setValue("deq_enabled", deq_enabled_->isChecked());
        settings_.setValue("deq_band_count", deq_active_count_);
        for (int i=0; i<10; ++i) {
            const QString k = "deq_b"+QString::number(i)+"_";
            const DeqBand& b=deq_bands_[i];
            settings_.setValue(k+"freq",b.frequency);
            settings_.setValue(k+"user_modified", deq_user_modified_[i]);
            settings_.setValue(k+"q",b.q);
            settings_.setValue(k+"gain",b.gain);
            settings_.setValue(k+"threshold",b.threshold);
            settings_.setValue(k+"attack",b.attack);
            settings_.setValue(k+"release",b.release);
            settings_.setValue(k+"type",b.type);
        }
        syncSimpleGuiSettings(GuiSettingPhase::SpatialOutput, false);
        save_timer_->start(300); // batch writes while a slider is dragged
    }
