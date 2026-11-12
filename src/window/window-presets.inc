// Private ViperWindow member definitions. Included once inside the class.
// Checkpoint 7 mechanical extraction; no additional Qt or DSP state owner.
    // Stable per-user location for portable, named effect snapshots. We keep
    // QSettings's existing org/app config store intact for preferences and
    // automatic session state, including older one-slot presets.
    QString presetLibraryDirectory() const {
        return viper_presets::presetLibraryDirectory();
    }
    using NamedPreset = viper_presets::NamedPreset;
    QList<NamedPreset> namedPresets() const {
        return viper_presets::namedPresets();
    }
    static bool acceptablePresetName(const QString& name) {
        return viper_presets::acceptablePresetName(name);
    }
    bool writeNamedPreset(const QString& name, const QString& path, QString* error) {
        return viper_presets::writeNamedPreset(settings_, name, path, error);
    }
    bool migrateLegacyPreset(QString* error) {
        return viper_presets::migrateLegacyPreset(settings_, error);
    }
    bool stageNamedPreset(const QString& path, QString* error) {
        return viper_presets::stageNamedPreset(settings_, path, error);
    }
    void snapshotToPresetStaging() {
            settings_.setValue("preset/valid", true);
            settings_.setValue("preset/master_enabled", global_->isChecked());
            settings_.setValue("preset/enabled", enabled_->isChecked());
            settings_.setValue("preset/bass_mode", bass_mode_->currentIndex());
            settings_.setValue("preset/strength_tenths", strength_.slider->value());
            settings_.setValue("preset/frequency", frequency_.slider->value());
            saveEqLayout();
            settings_.setValue("preset/eq_enabled", eq_enabled_->isChecked());
            settings_.setValue("preset/eq_selected_preset", eq_selected_preset_);
            settings_.setValue("preset/eq_band_count", eq_active_bands_);
            for (int n : {10, 15, 25, 31})
                settings_.setValue("preset/eq_levels_" + QString::number(n),
                    settings_.value("eq_levels_" + QString::number(n), flatEqCsv(n)));
            settings_.setValue("preset/ddc_enabled", ddc_enabled_->isChecked());
            settings_.setValue("preset/ddc_profile", settings_.value("ddc_profile", ""));
            settings_.setValue("preset/speaker_enabled", speaker_enabled_->isChecked());
            settings_.setValue("preset/convolver_enabled", convolver_enabled_->isChecked());
            settings_.setValue("preset/convolver_cross_percent", convolver_cross_.slider->value());
            settings_.setValue("preset/convolver_kernel", settings_.value("convolver_kernel"));
            saveBassPreset();
            settings_.setValue("preset/clarity_enabled", clarity_enabled_->isChecked());
            settings_.setValue("preset/clarity_mode", clarity_mode_->currentIndex());
            settings_.setValue("preset/clarity_gain_tenths", clarity_gain_.slider->value());
            settings_.setValue("preset/tube_enabled", tube_enabled_->isChecked());
            settings_.setValue("preset/tube_model", tube_model_->currentIndex());
            settings_.setValue("preset/tube_drive_percent", tube_drive_.slider->value());
            settings_.setValue("preset/tube_mix_percent", tube_mix_.slider->value());
            settings_.setValue("preset/analogx_enabled", analogx_enabled_->isChecked());
            settings_.setValue("preset/analogx_mode", analogx_mode_->currentIndex());
            settings_.setValue("preset/crossfeed_enabled", crossfeed_enabled_->isChecked());
            settings_.setValue("preset/crossfeed_preset", crossfeed_mode_->currentIndex());
            saveMultibandSettings("preset/");
            settings_.setValue("preset/fet_enabled", fet_enabled_->isChecked());
            settings_.setValue("preset/fet_threshold_db", fet_threshold_.slider->value());
            settings_.setValue("preset/fet_ratio_tenths", fet_ratio_.slider->value());
            settings_.setValue("preset/fet_knee_db", fet_knee_.slider->value());
            settings_.setValue("preset/fet_makeup_db", fet_gain_.slider->value());
            settings_.setValue("preset/fet_attack_ms", fet_attack_.slider->value());
            settings_.setValue("preset/fet_release_ms", fet_release_.slider->value());
            settings_.setValue("preset/imager_enabled", imager_enabled_->isChecked());
            settings_.setValue("preset/imager_low_width_percent", imager_low_.slider->value());
            settings_.setValue("preset/imager_mid_width_percent", imager_mid_.slider->value());
            settings_.setValue("preset/imager_high_width_percent", imager_high_.slider->value());
            settings_.setValue("preset/imager_low_crossover_hz", imager_low_xover_.slider->value());
            settings_.setValue("preset/imager_high_crossover_hundreds", imager_high_xover_.slider->value());
            saveDeqPreset();
            settings_.setValue("preset/spectrum_enabled", spectrum_enabled_->isChecked());
            settings_.setValue("preset/spectrum_frequency_hundreds", spectrum_frequency_.slider->value());
            settings_.setValue("preset/spectrum_exciter_tenths", spectrum_exciter_.slider->value());
            settings_.setValue("preset/headphone_enabled", headphone_enabled_->isChecked());
            settings_.setValue("preset/headphone_quality", headphone_quality_->currentIndex());
            settings_.setValue("preset/diff_enabled", diff_enabled_->isChecked());
            settings_.setValue("preset/diff_delay_ms", diff_delay_.slider->value());
            settings_.setValue("preset/diff_reverse", diff_reverse_->isChecked());
            settings_.setValue("preset/diff_mix_percent", diff_mix_.slider->value());
            settings_.setValue("preset/diff_lp_hundreds", diff_lowpass_.slider->value());
            settings_.setValue("preset/field_enabled", field_enabled_->isChecked());
            settings_.setValue("preset/field_widen_tenths", field_widen_.slider->value());
            settings_.setValue("preset/field_mid_hundredths", field_mid_.slider->value());
            settings_.setValue("preset/field_depth_step", field_depth_.slider->value());
            settings_.setValue("preset/reverb_enabled", reverb_enabled_->isChecked());
            settings_.setValue("preset/reverb_room_percent", reverb_room_.slider->value());
            settings_.setValue("preset/reverb_width_percent", reverb_width_.slider->value());
            settings_.setValue("preset/reverb_damp_percent", reverb_damp_.slider->value());
            settings_.setValue("preset/reverb_wet_percent", reverb_wet_.slider->value());
            settings_.setValue("preset/reverb_dry_percent", reverb_dry_.slider->value());
            saveOutputPreset();
            settings_.sync();
    }
    void restoreFromPresetStaging() {
            if (!settings_.value("preset/valid", false).toBool()) return;
            {
                // Existing restoration helpers call push() between effect groups.
                // Publish the complete named preset rather than partial states.
                const QScopedValueRollback<bool> deferPublishes(suppress_named_preset_push_, true);
                applyStagedPresetControls();
            }
            push();
    }
    void applyStagedPresetControls() {
            applySettings(settings_.value("preset/master_enabled", true).toBool(),
                          settings_.value("preset/enabled", true).toBool(),
                          settings_.value("preset/bass_mode", 0).toInt(),
                          settings_.value("preset/strength_tenths", 100).toInt(),
                          settings_.value("preset/frequency", 65).toInt(),
                          settings_.value("preset/clarity_enabled", false).toBool(),
                          settings_.value("preset/clarity_mode", 0).toInt(),
                          settings_.value("preset/clarity_gain_tenths", 15).toInt(),
                          settings_.value("preset/spectrum_enabled", false).toBool(),
                          settings_.value("preset/spectrum_frequency_hundreds", 76).toInt(),
                          settings_.value("preset/spectrum_exciter_tenths", 10).toInt(),
                          settings_.value("preset/headphone_enabled", false).toBool(),
                          settings_.value("preset/headphone_quality", 0).toInt(),
                          settings_.value("preset/diff_enabled", false).toBool(),
                          settings_.value("preset/diff_delay_ms", 8).toInt(),
                          settings_.value("preset/diff_reverse", false).toBool(),
                          settings_.value("preset/diff_mix_percent", 50).toInt(),
                          settings_.value("preset/diff_lp_hundreds", 0).toInt(),
                          settings_.value("preset/field_enabled", false).toBool(),
                          settings_.value("preset/field_widen_tenths", 10).toInt(),
                          settings_.value("preset/field_mid_hundredths", 100).toInt(),
                          settings_.value("preset/field_depth_step", 3).toInt(),
                          settings_.value("preset/reverb_enabled", false).toBool(),
                          settings_.value("preset/reverb_room_percent", 50).toInt(),
                          settings_.value("preset/reverb_width_percent", 50).toInt(),
                          settings_.value("preset/reverb_damp_percent", 50).toInt(),
                          settings_.value("preset/reverb_wet_percent", 20).toInt(),
                          settings_.value("preset/reverb_dry_percent", 50).toInt());
            for (int n : {10, 15, 25, 31})
                settings_.setValue("eq_levels_" + QString::number(n),
                    settings_.value("preset/eq_levels_" + QString::number(n),
                        flatEqCsv(n)));
            {
                const QSignalBlocker blockEq(eq_enabled_), blockMode(eq_mode_);
                eq_enabled_->setChecked(settings_.value("preset/eq_enabled", false).toBool());
                eq_active_bands_ = validEqBands(settings_.value("preset/eq_band_count", 10).toInt());
                eq_mode_->setCurrentIndex(eq_mode_->findData(eq_active_bands_));
                loadEqLayout();
                selectEqPreset(settings_.value("preset/eq_selected_preset", "custom").toString());
            }
            restoreOutputPreset();
            restoreBassPreset();
            applyTubeSettings(settings_.value("preset/tube_enabled", false).toBool(),
                settings_.value("preset/tube_model", 0).toInt(),
                settings_.value("preset/tube_drive_percent", 50).toInt(),
                settings_.value("preset/tube_mix_percent", 30).toInt());
            applyAnalogXSettings(settings_.value("preset/analogx_enabled", false).toBool(),
                settings_.value("preset/analogx_mode", 0).toInt());
            {
                const QSignalBlocker a(crossfeed_enabled_), b(crossfeed_mode_);
                crossfeed_enabled_->setChecked(settings_.value("preset/crossfeed_enabled", false).toBool());
                crossfeed_mode_->setCurrentIndex(std::clamp(settings_.value("preset/crossfeed_preset", 0).toInt(), 0, 2));
            }
            {
                const QSignalBlocker a(imager_enabled_);
                imager_enabled_->setChecked(settings_.value("preset/imager_enabled", false).toBool());
                for (const auto& item : std::initializer_list<std::pair<SliderRow, int>>{
                    {imager_low_, settings_.value("preset/imager_low_width_percent", 100).toInt()},
                    {imager_mid_, settings_.value("preset/imager_mid_width_percent", 100).toInt()},
                    {imager_high_, settings_.value("preset/imager_high_width_percent", 100).toInt()},
                    {imager_low_xover_, settings_.value("preset/imager_low_crossover_hz", 200).toInt()},
                    {imager_high_xover_, settings_.value("preset/imager_high_crossover_hundreds", 40).toInt()}}) {
                    const QSignalBlocker block(item.first.slider);
                    item.first.slider->setValue(item.second);
                }
                updateLabels();
            }
            {
                const QSignalBlocker b(ddc_enabled_);
                ddc_enabled_->setChecked(settings_.value("preset/ddc_enabled", false).toBool());
                settings_.setValue("ddc_profile", settings_.value("preset/ddc_profile", ""));
                updateDdcFileName(settings_.value("ddc_profile", "").toString());
                settings_.sync();
                requestDdcReload();
            }
            {
                const QSignalBlocker b(speaker_enabled_);
                speaker_enabled_->setChecked(settings_.value("preset/speaker_enabled", false).toBool());
            }
            restoreFetSettings("preset/");
            loadMultibandSettings("preset/");
            restoreDeqPreset();
            {
                const QSignalBlocker b1(convolver_enabled_), b2(convolver_cross_.slider);
                convolver_enabled_->setChecked(settings_.value("preset/convolver_enabled", false).toBool());
                convolver_cross_.slider->setValue(settings_.value("preset/convolver_cross_percent", 0).toInt());
                settings_.setValue("convolver_kernel", settings_.value("preset/convolver_kernel", ""));
                settings_.sync();
                refreshIrsLibrary();
                requestIrsLoad();
            }
    }
    void resetEffectsWithConfirmation() {
            QMessageBox confirm(this);
            confirm.setIcon(QMessageBox::Warning);
            confirm.setWindowTitle("Reset Effect Settings");
            confirm.setText("Reset all effect settings?");
            confirm.setInformativeText(
                "This will replace your current effect settings with the application's defaults "
                "(master processing and limiter at defaults; all optional effects off).\n\n"
                "Your named presets will not be deleted.");
            auto* resetButton = confirm.addButton("Reset Effects", QMessageBox::DestructiveRole);
            auto* cancelButton = confirm.addButton(QMessageBox::Cancel);
            confirm.setDefaultButton(qobject_cast<QPushButton*>(cancelButton));
            confirm.exec();
            if (confirm.clickedButton() != resetButton) return;

            applySettings(true, false, 0, 100, 65, false, 0, 15, false, 76, 10,
                          false, 0, false, 8, false, 50, 0,
                          false, 10, 100, 3,
                          false, 50, 50, 50, 20, 50);
            resetOutputControls();
            resetNewBassControls();
            applyTubeSettings(false, 0, 50, 30);
            applyAnalogXSettings(false, 0);
            {
                const QSignalBlocker a(crossfeed_enabled_), b(crossfeed_mode_);
                crossfeed_enabled_->setChecked(false);
                crossfeed_mode_->setCurrentIndex(0);
            }
            {
                const QSignalBlocker a(imager_enabled_);
                imager_enabled_->setChecked(false);
                for (const auto& item : std::initializer_list<std::pair<SliderRow, int>>{
                    {imager_low_, 100}, {imager_mid_, 100}, {imager_high_, 100},
                    {imager_low_xover_, 200}, {imager_high_xover_, 40}}) {
                    const QSignalBlocker block(item.first.slider);
                    item.first.slider->setValue(item.second);
                }
                updateLabels();
            }
            {
                const QSignalBlocker b(ddc_enabled_);
                ddc_enabled_->setChecked(false);
            }
            {
                const QSignalBlocker b(speaker_enabled_);
                speaker_enabled_->setChecked(false);
            }
            resetFetSettings();
            resetMultibandSettings();
            resetDeq();
            {
                const QSignalBlocker blockEq(eq_enabled_), blockMode(eq_mode_);
                eq_enabled_->setChecked(false);
                for (int n : {10, 15, 25, 31})
                    settings_.setValue("eq_levels_" + QString::number(n),
                        flatEqCsv(n));
                eq_active_bands_ = 10;
                eq_mode_->setCurrentIndex(eq_mode_->findData(10));
                loadEqLayout();
            }

            {
                const QSignalBlocker b1(convolver_enabled_), b2(convolver_cross_.slider);
                convolver_enabled_->setChecked(false);
                convolver_cross_.slider->setValue(0);
            }
            push();
            statusMessage("Effect settings reset to defaults; named presets preserved.");
    }
    void showPresetManager() {
        viper_presets_ui::showPresetManager(this, {
            [this](QString* error) -> bool { return migrateLegacyPreset(error); },
            [this]() -> QList<NamedPreset> { return namedPresets(); },
            [this](const QString& name) -> bool { return acceptablePresetName(name); },
            [this]() -> QString { return presetLibraryDirectory(); },
            [this]() { snapshotToPresetStaging(); },
            [this](const QString& name, const QString& path, QString* error) -> bool { return writeNamedPreset(name, path, error); },
            [this](const QString& message) { statusMessage(message); },
            [this]() -> QString { return outputMemoryDirectory(); },
            [this]() { updateAudioOutputDisplay(); },
            [this](const QString& path, QString* error) -> bool { return stageNamedPreset(path, error); },
            [this]() { restoreFromPresetStaging(); },
            [this]() { resetEffectsWithConfirmation(); }
        });
    }
