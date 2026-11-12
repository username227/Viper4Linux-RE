// Constructor initialization responsibilities, extracted without behavior changes.
// Included inside ViperWindow: these methods use the established widget and settings members.
// Keep the constructor order: restore settings, wire effect signals, then set up theme monitoring.

    void loadSavedEffectSettings() {
        // The original Stage 5 GUI saved these keys on every adjustment.
        // Keep the format and keys compatible so old settings are preserved.
        syncSimpleGuiSettings(GuiSettingPhase::Intro, true);
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
        syncSimpleGuiSettings(GuiSettingPhase::DynamicTail, true);
        syncSimpleGuiSettings(GuiSettingPhase::Tone, true);
        loadMultibandSettings("");
        syncSimpleGuiSettings(GuiSettingPhase::Dynamics, true);
        imager_high_xover_.slider->setValue(std::clamp(settings_.value("imager_high_crossover_hz", 4000).toInt()/100, 20, 80));
        loadDeqSettings();
        syncSimpleGuiSettings(GuiSettingPhase::SpatialOutput, true);
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
