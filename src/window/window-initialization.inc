// Constructor initialization responsibilities, extracted without behavior changes.
// Included inside ViperWindow: these methods use the established widget and settings members.
// Keep the constructor order: restore settings, wire effect signals, then set up theme monitoring.

    void loadSavedEffectSettings() {
        // The original Stage 5 GUI saved these keys on every adjustment.
        // Keep the format and keys compatible so old settings are preserved.
        global_->setChecked(settings_.value("master_enabled", true).toBool());
        enabled_->setChecked(settings_.value("enabled", true).toBool());
        bass_mode_->setCurrentIndex(std::clamp(settings_.value("bass_mode", 0).toInt(), 0, 2));
        strength_.slider->setValue(settings_.value("strength_tenths", 100).toInt());
        frequency_.slider->setValue(settings_.value("frequency", 65).toInt());
        // Remove obsolete settings so no hidden input attenuation survives.
        settings_.remove("input_percent");
        settings_.remove("preset/input_percent");
        eq_enabled_->setChecked(settings_.value("eq_enabled", false).toBool());
        const int savedEqMode = validEqBands(settings_.value("eq_band_count", 10).toInt());
        eq_active_bands_ = savedEqMode;
        eq_mode_->setCurrentIndex(eq_mode_->findData(savedEqMode));
        loadEqLayout();
        eq_selected_preset_ = settings_.value("eq/selected_preset", "custom").toString();
        selectEqPreset(eq_selected_preset_);
        ddc_enabled_->setChecked(settings_.value("ddc_enabled", false).toBool());
        updateDdcFileName(settings_.value("ddc_profile").toString());
        speaker_enabled_->setChecked(settings_.value("speaker_enabled", false).toBool());
        convolver_enabled_->setChecked(settings_.value("convolver_enabled", false).toBool());
        convolver_cross_.slider->setValue(std::clamp(settings_.value("convolver_cross_percent", 0).toInt(), 0, 100));
        refreshIrsLibrary();
        mono_enabled_->setChecked(settings_.value("bass_mono_enabled", false).toBool());
        mono_mode_->setCurrentIndex(std::clamp(settings_.value("bass_mono_mode", 0).toInt(), 0, 2));
        mono_strength_.slider->setValue(std::clamp(settings_.value("bass_mono_strength_centi", 200).toInt()/10, 5, 100));
        mono_frequency_.slider->setValue(std::clamp(settings_.value("bass_mono_frequency_hz", 65).toInt(), 15, 150));
        psycho_enabled_->setChecked(settings_.value("psycho_enabled", false).toBool());
        psycho_cutoff_.slider->setValue(std::clamp(settings_.value("psycho_cutoff_hz", 80).toInt(), 60, 150));
        psycho_intensity_.slider->setValue(std::clamp(settings_.value("psycho_intensity_percent", 30).toInt(), 0, 100));
        psycho_order_->setCurrentIndex(std::clamp(settings_.value("psycho_order", 3).toInt()-2, 0, 3));
        psycho_original_.slider->setValue(std::clamp(settings_.value("psycho_original_percent", 100).toInt(), 0, 100));
        dyn_enabled_->setChecked(settings_.value("dynamic_enabled", false).toBool());
        dyn_xlo_native_ = std::clamp(settings_.value("dynamic_x_low", 100).toInt(), 0, 2400);
        dyn_xlo_.slider->setValue(std::max(120, dyn_xlo_native_));
        dyn_xhi_.slider->setValue(std::clamp(settings_.value("dynamic_x_high", 6000).toInt(), 0, 12000));
        dyn_ylo_.slider->setValue(std::clamp(settings_.value("dynamic_y_low", 100).toInt(), 0, 200));
        dyn_yhi_.slider->setValue(std::clamp(settings_.value("dynamic_y_high", 150).toInt(), 0, 300));
        dyn_side_lo_.slider->setValue(std::clamp(settings_.value("dynamic_side_low_percent", 50).toInt(), 0, 100));
        dyn_side_hi_.slider->setValue(std::clamp(settings_.value("dynamic_side_high_percent", 50).toInt(), 0, 100));
        dyn_strength_.slider->setValue(std::clamp(settings_.value("dynamic_strength_percent", 0).toInt(), 0, 100));
        clarity_enabled_->setChecked(settings_.value("clarity_enabled", false).toBool());
        clarity_mode_->setCurrentIndex(std::clamp(settings_.value("clarity_mode", 0).toInt(), 0, 2));
        clarity_gain_.slider->setValue(settings_.value("clarity_gain_tenths", 15).toInt());
        spectrum_enabled_->setChecked(settings_.value("spectrum_enabled", false).toBool());
        spectrum_frequency_.slider->setValue(settings_.value("spectrum_frequency_hundreds", 76).toInt());
        spectrum_exciter_.slider->setValue(settings_.value("spectrum_exciter_tenths", 10).toInt());
        tube_enabled_->setChecked(settings_.value("tube_enabled", false).toBool());
        tube_model_->setCurrentIndex(std::clamp(settings_.value("tube_model", 0).toInt(), 0, 2));
        tube_drive_.slider->setValue(std::clamp(settings_.value("tube_drive_percent", 50).toInt(), 0, 100));
        tube_mix_.slider->setValue(std::clamp(settings_.value("tube_mix_percent", 30).toInt(), 0, 100));
        analogx_enabled_->setChecked(settings_.value("analogx_enabled", false).toBool());
        analogx_mode_->setCurrentIndex(std::clamp(settings_.value("analogx_mode", 0).toInt(), 0, 2));
        crossfeed_enabled_->setChecked(settings_.value("crossfeed_enabled", false).toBool());
        crossfeed_mode_->setCurrentIndex(std::clamp(settings_.value("crossfeed_preset", 0).toInt(), 0, 2));
        loadMultibandSettings("");
        fet_enabled_->setChecked(settings_.value("fet_enabled", false).toBool());
        fet_threshold_.slider->setValue(std::clamp(settings_.value("fet_threshold_db", -18).toInt(), -48, 0));
        fet_ratio_.slider->setValue(std::clamp(settings_.value("fet_ratio_tenths", 20).toInt(), 10, 100));
        fet_knee_.slider->setValue(std::clamp(settings_.value("fet_knee_db", 6).toInt(), 0, 12));
        fet_gain_.slider->setValue(std::clamp(settings_.value("fet_makeup_db", 0).toInt(), 0, 24));
        fet_attack_.slider->setValue(std::clamp(settings_.value("fet_attack_ms", 10).toInt(), 1, 100));
        fet_release_.slider->setValue(std::clamp(settings_.value("fet_release_ms", 100).toInt(), 5, 500));
        imager_enabled_->setChecked(settings_.value("imager_enabled", false).toBool());
        imager_low_.slider->setValue(std::clamp(settings_.value("imager_low_width_percent", 100).toInt(), 0, 200));
        imager_mid_.slider->setValue(std::clamp(settings_.value("imager_mid_width_percent", 100).toInt(), 0, 200));
        imager_high_.slider->setValue(std::clamp(settings_.value("imager_high_width_percent", 100).toInt(), 0, 200));
        imager_low_xover_.slider->setValue(std::clamp(settings_.value("imager_low_crossover_hz", 200).toInt(), 80, 400));
        imager_high_xover_.slider->setValue(std::clamp(settings_.value("imager_high_crossover_hz", 4000).toInt()/100, 20, 80));
        loadDeqSettings();
        headphone_enabled_->setChecked(settings_.value("headphone_enabled", false).toBool());
        headphone_quality_->setCurrentIndex(std::clamp(settings_.value("headphone_quality", 0).toInt(), 0, 4));
        diff_enabled_->setChecked(settings_.value("diff_enabled", false).toBool());
        diff_delay_.slider->setValue(std::clamp(settings_.value("diff_delay_ms", 8).toInt(), 1, 20));
        diff_reverse_->setChecked(settings_.value("diff_reverse", false).toBool());
        diff_mix_.slider->setValue(std::clamp(settings_.value("diff_mix_percent", 50).toInt(), 0, 100));
        diff_lowpass_.slider->setValue(std::clamp(settings_.value("diff_lp_hundreds", 0).toInt(), 0, 200));
        field_enabled_->setChecked(settings_.value("field_enabled", false).toBool());
        field_widen_.slider->setValue(std::clamp(settings_.value("field_widen_tenths", 10).toInt(), 0, 80));
        field_mid_.slider->setValue(std::clamp(settings_.value("field_mid_hundredths", 100).toInt(), 100, 200));
        field_depth_.slider->setValue(std::clamp(settings_.value("field_depth_step", 3).toInt(), 0, 16));
        reverb_enabled_->setChecked(settings_.value("reverb_enabled", false).toBool());
        reverb_room_.slider->setValue(std::clamp(settings_.value("reverb_room_percent", 50).toInt(), 0, 100));
        reverb_width_.slider->setValue(std::clamp(settings_.value("reverb_width_percent", 50).toInt(), 0, 100));
        reverb_damp_.slider->setValue(std::clamp(settings_.value("reverb_damp_percent", 50).toInt(), 0, 100));
        reverb_wet_.slider->setValue(std::clamp(settings_.value("reverb_wet_percent", 20).toInt(), 0, 100));
        reverb_dry_.slider->setValue(std::clamp(settings_.value("reverb_dry_percent", 50).toInt(), 0, 100));
        limiter_threshold_.slider->setValue(std::clamp(settings_.value("limiter_threshold_permille", 500).toInt(), 300, 1000));
        limiter_gain_.slider->setValue(std::clamp(settings_.value("limiter_output_percent", 100).toInt(), 1, 200));
        limiter_pan_.slider->setValue(std::clamp(settings_.value("limiter_pan_percent", 0).toInt(), -100, 100));
        agc_enabled_->setChecked(settings_.value("agc_enabled", false).toBool());
        agc_strength_.slider->setValue(std::clamp(settings_.value("agc_strength_centi", 100).toInt(), 50, 300));
        agc_max_gain_.slider->setValue(std::clamp(settings_.value("agc_max_gain_centi", 300).toInt(), 100, 1000));
        agc_threshold_.slider->setValue(std::clamp(settings_.value("agc_threshold_permille", 900).toInt(), 300, 1000));
        lufs_enabled_->setChecked(settings_.value("lufs_enabled", false).toBool());
        lufs_target_.slider->setValue(std::clamp(settings_.value("lufs_target_tenths", -140).toInt(), -240, -80));
        lufs_gain_.slider->setValue(std::clamp(settings_.value("lufs_max_gain_tenths", 60).toInt(), 0, 120));
        lufs_speed_->setCurrentIndex(std::clamp(settings_.value("lufs_speed", 1).toInt(), 0, 2));
        updateLabels();
    }

    void connectEffectSignals(QPushButton* eq_reset, QPushButton* deq_spread) {
        connect(global_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(bass_mode_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(strength_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(frequency_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(eq_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(eq_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (index < 0) return;
            saveEqLayout();
            eq_active_bands_ = validEqBands(eq_mode_->itemData(index).toInt());
            if (eq_selected_preset_ != "custom") applyEqPreset(eq_selected_preset_);
            else { loadEqLayout(); push(); }
            // A visible nonmodal editor must change its columns immediately.
            // applyEqPreset() updates levels, not column visibility.
            syncGraphicEqEditorColumns();
        });
        for (int i = 0; i < 31; ++i) {
            connect(eq_faders_[i], &QSlider::valueChanged, this, [this, i](int value) {
                eq_values_[i]->setText(QString::number(value / 100.0, 'f', 2));
                eq_faders_[i]->setToolTip(eq_frequencies_[i]->text() + ": " +
                    QString::number(value / 100.0, 'f', 2) + " dB");
                saveEqLayout();
                selectEqPreset("custom");
                updateEqOverview();
                push();
            });
        }
        connect(eq_reset, &QPushButton::clicked, this, [this] {
            applyEqPreset("builtin:Flat");
            selectEqPreset("builtin:Flat");
        });
        connect(ddc_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(speaker_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(convolver_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(convolver_cross_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(mono_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(mono_mode_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(psycho_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(psycho_order_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(dyn_enabled_, &QCheckBox::toggled, this, [this]{ updateDynamicSystemControlSensitivity(); push(); });
        connect(dyn_xlo_.slider, &QSlider::valueChanged, this, [this] {
            // A deliberate GUI movement selects a value in the GUI's 120–2400 Hz range.
            dyn_xlo_native_ = dyn_xlo_.slider->value();
            dyn_xlo_explicit_ = true; // Keep this intent through an offline/restart queue.
            updateLabels();
            push();
        });
        for (auto* slider : {mono_strength_.slider, mono_frequency_.slider,
                             psycho_cutoff_.slider, psycho_intensity_.slider, psycho_original_.slider,
                             dyn_xhi_.slider, dyn_ylo_.slider, dyn_yhi_.slider,
                             dyn_side_lo_.slider, dyn_side_hi_.slider, dyn_strength_.slider}) {
            connect(slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        }
        connect(clarity_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(clarity_mode_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(clarity_gain_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(spectrum_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(spectrum_frequency_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(spectrum_exciter_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(tube_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(tube_model_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ push(); });
        connect(tube_drive_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(tube_mix_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(analogx_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(analogx_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ push(); });
        connect(deq_enabled_, &QCheckBox::toggled, this, [this]{
            updateDeqOverview(); push();
        });
        connect(deq_spread, &QPushButton::clicked, this,
                [this]{ spreadDeqFrequencies(); });
        connect(deq_count_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int n){ changeDeqBandCount(n); });
        connect(deq_editor_count_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int n){ changeDeqBandCount(n); });
        connect(deq_band_select_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index){ if (index >= 0) loadDeqEditorBand(index); });
        for (QSpinBox* s : {deq_frequency_, deq_attack_, deq_release_})
            connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ commitDeqEditorBand(); });
        for (QDoubleSpinBox* s : {deq_q_, deq_gain_, deq_threshold_})
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ commitDeqEditorBand(); });
        connect(deq_type_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int){ commitDeqEditorBand(); });
        connect(crossfeed_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(crossfeed_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ push(); });
        connect(mb_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(mb_band_count_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
            if (index < 0) return;
            const std::array<int,4> defaults = index == 0
                ? std::array<int,4>{200, 4000, 5000, 8000}
                : std::array<int,4>{120, 500, 4000, 8000};
            for (int i=0; i<4; ++i) {
                const QSignalBlocker block(mb_crossovers_[i].slider);
                mb_crossovers_[i].slider->setValue(defaults[i]);
            }
            updateMultibandVisibility(); updateLabels(); push();
        });
        for (int i=0; i<4; ++i)
            connect(mb_crossovers_[i].slider, &QSlider::valueChanged, this, [this]{
                updateMultibandFrequencyLabels(); updateLabels(); push();
            });
        for (auto& band : mb_bands_) {
            connect(band.enabled, &QCheckBox::toggled, this, [this]{ push(); });
            for (const auto& r : {band.threshold, band.ratio, band.knee, band.gain, band.attack, band.release})
                connect(r.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        }
        connect(fet_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        for (const auto& row : {fet_threshold_, fet_ratio_, fet_knee_, fet_gain_, fet_attack_, fet_release_})
            connect(row.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(imager_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        for (const auto& r : {imager_low_, imager_mid_, imager_high_, imager_low_xover_, imager_high_xover_})
            connect(r.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(headphone_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(headphone_quality_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(diff_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(diff_delay_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(diff_reverse_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(diff_mix_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(diff_lowpass_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(field_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(field_widen_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(field_mid_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(field_depth_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(reverb_room_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_width_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_damp_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_wet_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_dry_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        for (auto* slider : {limiter_threshold_.slider, limiter_gain_.slider, limiter_pan_.slider,
                              agc_strength_.slider, agc_max_gain_.slider, agc_threshold_.slider,
                              lufs_target_.slider, lufs_gain_.slider}) {
            connect(slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        }
        connect(agc_enabled_, &QCheckBox::toggled, this, [this]{ updateLabels(); push(); });
        connect(lufs_enabled_, &QCheckBox::toggled, this, [this]{ updateLabels(); push(); });
        connect(lufs_speed_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
    }

    void setupThemeMonitoring() {
        // Theme selection changes only the GUI, never DSP settings.
        connect(theme_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            settings_.setValue("ui/theme", index == 1 ? "classic-v4a" : "modern");
            applyTheme(index == 1);
        });
        // On Cinnamon/X11 Qt can report a dark colorScheme even while GTK
        // desktop windows use a light theme. The authoritative choice there is
        // Cinnamon's GTK theme setting, read asynchronously (no UI blocking).
        // Until discovered, the required fallback is LIGHT.
        if (runningCinnamon()) {
            cinnamon_theme_query_ = new QProcess(this);
            connect(cinnamon_theme_query_, &QProcess::finished,
                    this, [this](int code, QProcess::ExitStatus exitStatus) {
                if (exitStatus != QProcess::NormalExit || code != 0) return;
                const QString value = QString::fromUtf8(cinnamon_theme_query_->readAllStandardOutput()).trimmed();
                if (value.isEmpty() || !value.contains('\'')) return;
                const bool isDark = value.contains("dark", Qt::CaseInsensitive);
                if (!cinnamon_theme_known_ || cinnamon_theme_dark_ != isDark) {
                    cinnamon_theme_known_ = true;
                    cinnamon_theme_dark_ = isDark;
                    applyTheme(theme_selector_->currentIndex() == 1);
                }
            });
            auto* desktopThemeTimer = new QTimer(this);
            connect(desktopThemeTimer, &QTimer::timeout, this, [this] {
                if (cinnamon_theme_query_->state() == QProcess::NotRunning) {
                    cinnamon_theme_query_->start("gsettings",
                        {"get", "org.cinnamon.desktop.interface", "gtk-theme"});
                }
            });
            desktopThemeTimer->start(5000);
            // First query happens right away, without waiting five seconds.
            cinnamon_theme_query_->start("gsettings",
                {"get", "org.cinnamon.desktop.interface", "gtk-theme"});
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged,
                this, [this](Qt::ColorScheme) {
            QTimer::singleShot(0, this, [this] {
                applyTheme(theme_selector_->currentIndex() == 1);
            });
        });
#endif
        applyTheme(theme_selector_->currentIndex() == 1);
    }
