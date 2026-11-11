// Implementation is included in ViperWindow, not compiled independently.
// State transition order, QSignalBlocker lifetime and sender() are unchanged.
    bool ensureBackend() {
        return viper_gui_sync::ensureCurrent(mapping_);
    }

    bool publishToBackend() {
        if (!ensureBackend()) {
            pending_edit_ = true;
            showOffline();
            return false;
        }
        auto* c = mapping_.get();
        // Preserve native precision when a different GUI control is edited.
        // Only a deliberate move of this slider replaces the DSP's precise value.
        const QObject* editedControl = sender();
        const auto preciseSlider = [this, editedControl](QSlider* slider, int liveValue,
                                                         int sliderValue, const char* setting) {
            // An unrelated edit must not quantize another slider, even before
            // the next GUI poll has reflected a concurrent CLI/output change.
            const int result = viper_gui_sync::nativePrecision(
                replace_precision_on_push_, editedControl == slider, liveValue, sliderValue);
            settings_.setValue(setting, result);
            return result;
        };
        c->master_enabled.store(global_->isChecked() ? 1 : 0);
        c->enabled.store(enabled_->isChecked() ? 1 : 0);
        c->bass_mode.store(bass_mode_->currentIndex());
        c->strength_centi.store(preciseSlider(strength_.slider, c->strength_centi.load(),
            strength_.slider->value() * 10,
            "precision/strength_centi"));
        c->frequency_hz.store(frequency_.slider->value());
        bool eq_changed = false;
        const auto setEqValue = [&eq_changed](std::atomic<std::int32_t>& atom, int value) {
            if (atom.load(std::memory_order_relaxed) != value) {
                atom.store(value, std::memory_order_relaxed);
                eq_changed = true;
            }
        };
        setEqValue(c->eq_enabled, eq_enabled_->isChecked() ? 1 : 0);
        setEqValue(c->eq_band_count, eq_active_bands_);
        for (int i = 0; i < eq_active_bands_; ++i)
            setEqValue(c->eq_levels_hundredths[i], eq_faders_[i]->value());
        if (eq_changed) c->eq_request_serial.fetch_add(1, std::memory_order_release);
        const int ddc_request = ddc_enabled_->isChecked() ? 1 : 0;
        if (c->ddc_enabled.exchange(ddc_request) != ddc_request)
            c->ddc_request_serial.fetch_add(1, std::memory_order_release);
        const int speaker_request = speaker_enabled_->isChecked() ? 1 : 0;
        if (c->speaker_enabled.exchange(speaker_request) != speaker_request)
            c->speaker_request_serial.fetch_add(1, std::memory_order_release);
        c->convolver_enabled.store(convolver_enabled_->isChecked() ? 1 : 0);
        c->convolver_cross_percent.store(convolver_cross_.slider->value());
        c->bass_mono_enabled.store(mono_enabled_->isChecked() ? 1 : 0);
        c->bass_mono_mode.store(mono_mode_->currentIndex());
        c->bass_mono_strength_centi.store(preciseSlider(mono_strength_.slider,
            c->bass_mono_strength_centi.load(),
            mono_strength_.slider->value() * 10, "precision/bass_mono_strength_centi"));
        c->bass_mono_frequency_hz.store(mono_frequency_.slider->value());
        c->psycho_enabled.store(psycho_enabled_->isChecked() ? 1 : 0);
        c->psycho_cutoff_hz.store(psycho_cutoff_.slider->value());
        c->psycho_intensity_percent.store(psycho_intensity_.slider->value());
        c->psycho_order.store(psycho_order_->currentIndex()+2);
        c->psycho_original_percent.store(psycho_original_.slider->value());
        c->dynamic_enabled.store(dyn_enabled_->isChecked() ? 1 : 0);
        if (dyn_xlo_explicit_) {
            c->dynamic_x_low.store(dyn_xlo_native_);
        } else {
            // The backend is authoritative for unrelated GUI edits, including
            // CLI values below our slider's 120 Hz visual minimum.
            dyn_xlo_native_ = std::clamp(c->dynamic_x_low.load(), 0, 2400);
            settings_.setValue("dynamic_x_low", dyn_xlo_native_);
        }
        c->dynamic_x_high.store(dyn_xhi_.slider->value());
        c->dynamic_y_low.store(dyn_ylo_.slider->value());
        c->dynamic_y_high.store(dyn_yhi_.slider->value());
        c->dynamic_side_low_percent.store(dyn_side_lo_.slider->value());
        c->dynamic_side_high_percent.store(dyn_side_hi_.slider->value());
        c->dynamic_strength_percent.store(dyn_strength_.slider->value());
        c->clarity_enabled.store(clarity_enabled_->isChecked() ? 1 : 0);
        c->clarity_mode.store(clarity_mode_->currentIndex());
        c->clarity_gain_milli.store(preciseSlider(clarity_gain_.slider, c->clarity_gain_milli.load(),
            clarity_gain_.slider->value() * 100,
            "precision/clarity_gain_milli"));
        c->spectrum_enabled.store(spectrum_enabled_->isChecked() ? 1 : 0);
        c->spectrum_frequency_hz.store(preciseSlider(spectrum_frequency_.slider,
            c->spectrum_frequency_hz.load(),
            spectrum_frequency_.slider->value() * 100, "precision/spectrum_frequency_hz"));
        c->spectrum_exciter_milli.store(preciseSlider(spectrum_exciter_.slider,
            c->spectrum_exciter_milli.load(),
            spectrum_exciter_.slider->value() * 100, "precision/spectrum_exciter_milli"));
        bool tube_changed = false;
        const auto setTubeValue = [&tube_changed](std::atomic<std::int32_t>& atom, int value) {
            if (atom.load(std::memory_order_relaxed) != value) {
                atom.store(value, std::memory_order_relaxed);
                tube_changed = true;
            }
        };
        setTubeValue(c->tube_enabled, tube_enabled_->isChecked() ? 1 : 0);
        setTubeValue(c->tube_model, tube_model_->currentIndex());
        setTubeValue(c->tube_drive_percent, tube_drive_.slider->value());
        setTubeValue(c->tube_mix_percent, tube_mix_.slider->value());
        if (tube_changed) c->tube_request_serial.fetch_add(1, std::memory_order_release);
        bool analogx_changed = false;
        const auto setAnalogXValue = [&analogx_changed](std::atomic<std::int32_t>& atom, int value) {
            if (atom.load(std::memory_order_relaxed) != value) {
                atom.store(value, std::memory_order_relaxed);
                analogx_changed = true;
            }
        };
        setAnalogXValue(c->analogx_enabled, analogx_enabled_->isChecked() ? 1 : 0);
        setAnalogXValue(c->analogx_mode, analogx_mode_->currentIndex());
        if (analogx_changed) c->analogx_request_serial.fetch_add(1, std::memory_order_release);
        bool crossfeed_changed = false;
        const auto setCrossfeed = [&crossfeed_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed) != value) {
                dst.store(value, std::memory_order_relaxed);
                crossfeed_changed = true;
            }
        };
        setCrossfeed(c->crossfeed_enabled, crossfeed_enabled_->isChecked() ? 1 : 0);
        setCrossfeed(c->crossfeed_preset, crossfeed_mode_->currentIndex());
        if (crossfeed_changed) c->crossfeed_request_serial.fetch_add(1, std::memory_order_release);
        bool mb_changed = false;
        const auto setMb = [&mb_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed) != value) {
                dst.store(value, std::memory_order_relaxed);
                mb_changed = true;
            }
        };
        setMb(c->mb_enabled, mb_enabled_->isChecked() ? 1 : 0);
        setMb(c->mb_band_count, mb_band_count_->currentIndex() == 1 ? 5 : 3);
        for (int i=0; i<4; ++i) setMb(c->mb_crossovers_hz[i], mb_crossovers_[i].slider->value());
        for (int i=0; i<5; ++i) {
            const auto& src = mb_bands_[i];
            auto& dst = c->mb_bands[i];
            setMb(dst.enabled, src.enabled->isChecked() ? 1 : 0);
            setMb(dst.threshold_db, src.threshold.slider->value());
            setMb(dst.ratio_tenths, src.ratio.slider->value());
            setMb(dst.knee_db, src.knee.slider->value());
            setMb(dst.makeup_db, src.gain.slider->value());
            setMb(dst.attack_ms, src.attack.slider->value());
            setMb(dst.release_ms, src.release.slider->value());
        }
        if (mb_changed) c->mb_request_serial.fetch_add(1, std::memory_order_release);
        bool fet_changed = false;
        const auto setFet = [&fet_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed) != value) {
                dst.store(value, std::memory_order_relaxed);
                fet_changed = true;
            }
        };
        setFet(c->fet_enabled, fet_enabled_->isChecked() ? 1 : 0);
        setFet(c->fet_threshold_db, fet_threshold_.slider->value());
        setFet(c->fet_ratio_tenths, fet_ratio_.slider->value());
        setFet(c->fet_knee_db, fet_knee_.slider->value());
        setFet(c->fet_makeup_db, fet_gain_.slider->value());
        setFet(c->fet_attack_ms, fet_attack_.slider->value());
        setFet(c->fet_release_ms, fet_release_.slider->value());
        if (fet_changed) c->fet_request_serial.fetch_add(1, std::memory_order_release);
        bool imager_changed = false;
        const auto setImager = [&imager_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed) != value) {
                dst.store(value, std::memory_order_relaxed);
                imager_changed = true;
            }
        };
        setImager(c->imager_enabled, imager_enabled_->isChecked() ? 1 : 0);
        setImager(c->imager_low_width_percent, imager_low_.slider->value());
        setImager(c->imager_mid_width_percent, imager_mid_.slider->value());
        setImager(c->imager_high_width_percent, imager_high_.slider->value());
        setImager(c->imager_low_crossover_hz, imager_low_xover_.slider->value());
        setImager(c->imager_high_crossover_hz, imager_high_xover_.slider->value() * 100);
        if (imager_changed) c->imager_request_serial.fetch_add(1, std::memory_order_release);
        bool deq_changed = false;
        const auto setDeq = [&deq_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed)!=value) {
                dst.store(value,std::memory_order_relaxed);
                deq_changed=true;
            }
        };
        setDeq(c->deq_enabled,deq_enabled_->isChecked()?1:0);
        setDeq(c->deq_band_count,deq_active_count_);
        for(int i=0; i<10; ++i) {
            const auto& b=deq_bands_[i];
            auto& dst=c->deq_bands[i];
            setDeq(dst.frequency_hz,b.frequency);
            setDeq(dst.q_hundredths,b.q);
            setDeq(dst.gain_hundredths,b.gain);
            setDeq(dst.threshold_tenths,b.threshold);
            setDeq(dst.attack_ms,b.attack);
            setDeq(dst.release_ms,b.release);
            setDeq(dst.filter_type,b.type);
        }
        if (deq_changed) c->deq_request_serial.fetch_add(1,std::memory_order_release);
        c->headphone_enabled.store(headphone_enabled_->isChecked() ? 1 : 0);
        c->headphone_quality.store(headphone_quality_->currentIndex());
        c->diff_enabled.store(diff_enabled_->isChecked() ? 1 : 0);
        c->diff_delay_ms.store(diff_delay_.slider->value());
        c->diff_reverse.store(diff_reverse_->isChecked() ? 1 : 0);
        c->diff_mix_percent.store(diff_mix_.slider->value());
        c->diff_lp_hz.store(preciseSlider(diff_lowpass_.slider, c->diff_lp_hz.load(),
            diff_lowpass_.slider->value() * 100,
            "precision/diff_lp_hz"));
        c->field_enabled.store(field_enabled_->isChecked() ? 1 : 0);
        c->field_widen_tenths.store(field_widen_.slider->value());
        c->field_mid_hundredths.store(field_mid_.slider->value());
        const int liveFieldDepth = c->field_depth.load();
        c->field_depth.store(preciseSlider(field_depth_.slider, liveFieldDepth,
            field_depth_.slider->value() == 0 ? 0 : 150 + 50 * field_depth_.slider->value(),
            "precision/field_depth"));
        c->reverb_enabled.store(reverb_enabled_->isChecked() ? 1 : 0);
        c->reverb_room_percent.store(reverb_room_.slider->value());
        c->reverb_width_percent.store(reverb_width_.slider->value());
        c->reverb_damp_percent.store(reverb_damp_.slider->value());
        c->reverb_wet_percent.store(reverb_wet_.slider->value());
        c->reverb_dry_percent.store(reverb_dry_.slider->value());
        c->limiter_threshold_permille.store(limiter_threshold_.slider->value());
        c->limiter_output_percent.store(limiter_gain_.slider->value());
        c->limiter_pan_percent.store(limiter_pan_.slider->value());
        c->agc_enabled.store(agc_enabled_->isChecked() ? 1 : 0);
        c->agc_strength_centi.store(agc_strength_.slider->value());
        c->agc_max_gain_centi.store(agc_max_gain_.slider->value());
        c->agc_threshold_permille.store(agc_threshold_.slider->value());
        c->lufs_enabled.store(lufs_enabled_->isChecked() ? 1 : 0);
        c->lufs_target_tenths.store(lufs_target_.slider->value());
        c->lufs_max_gain_tenths.store(lufs_gain_.slider->value());
        c->lufs_speed.store(lufs_speed_->currentIndex());
        // A backend can exit while we're writing. Detect the unlink/replacement
        // so the edit will be sent again when a new processor becomes available.
        if (!mapping_.current()) {
            mapping_.close();
            pending_edit_ = true;
            showOffline();
            return false;
        }
        pending_edit_ = false;
        dyn_xlo_explicit_ = false;
        replace_precision_on_push_ = false;
        return true;
    }

    std::vector<int> liveUiSnapshot() const {
        std::vector<int> values;
        for (const auto* widget : findChildren<QCheckBox*>())
            values.push_back(widget->isChecked() ? 1 : 0);
        for (const auto* widget : findChildren<QComboBox*>())
            values.push_back(widget->currentIndex());
        for (const auto* widget : findChildren<QSlider*>())
            values.push_back(widget->value());
        values.push_back(deq_active_count_);
        for (const auto& b : deq_bands_) {
            values.insert(values.end(), {b.frequency, b.q, b.gain, b.threshold,
                                         b.attack, b.release, b.type});
        }
        return values;
    }

    void readBackend() {
        if (!viperbass::running(mapping_.get())) return;
        auto* c = mapping_.get();
        const auto before = liveUiSnapshot();
        const int beforeNativeLowX = dyn_xlo_native_;
        // Reflect changes made from the CLI; avoid tug-of-war while dragging sliders.
        const QSignalBlocker block0(global_);
        const QSignalBlocker block1(enabled_);
        const QSignalBlocker blockBassMode(bass_mode_);
        const QSignalBlocker block2(clarity_enabled_);
        const QSignalBlocker block3(clarity_mode_);
        const QSignalBlocker block4(spectrum_enabled_);
        const QSignalBlocker blockTube(tube_enabled_), blockTubeModel(tube_model_);
        const QSignalBlocker blockAnalogX(analogx_enabled_), blockAnalogMode(analogx_mode_);
        const QSignalBlocker blockCrossfeed(crossfeed_enabled_), blockCrossfeedMode(crossfeed_mode_);
        const QSignalBlocker blockImager(imager_enabled_);
        const QSignalBlocker blockFet(fet_enabled_);
        const QSignalBlocker blockMb(mb_enabled_), blockMbCount(mb_band_count_);
        const QSignalBlocker block5(headphone_enabled_);
        const QSignalBlocker block6(headphone_quality_);
        const QSignalBlocker block7(diff_enabled_);
        const QSignalBlocker block8(diff_reverse_);
        const QSignalBlocker block9(field_enabled_);
        const QSignalBlocker blockReverb(reverb_enabled_);
        const QSignalBlocker blockMono(mono_enabled_), blockMonoMode(mono_mode_);
        const QSignalBlocker blockPsycho(psycho_enabled_), blockPsychoOrder(psycho_order_);
        const QSignalBlocker blockDynamic(dyn_enabled_);
        const QSignalBlocker blockAgc(agc_enabled_), blockLufs(lufs_enabled_);
        const QSignalBlocker blockSpeed(lufs_speed_);
        const QSignalBlocker blockConv(convolver_enabled_);
        const QSignalBlocker blockDdc(ddc_enabled_), blockSpeaker(speaker_enabled_);
        const QSignalBlocker blockDeq(deq_enabled_);
        const QSignalBlocker blockEq(eq_enabled_);
        settings_.sync();
        const QString remoteEqPreset = settings_.value("eq/selected_preset", "custom").toString();
        eq_enabled_->setChecked(c->eq_enabled.load() != 0);
        const int remoteEqBands = validEqBands(c->eq_band_count.load());
        if (remoteEqBands != eq_active_bands_) {
            saveEqLayout();
            eq_active_bands_ = remoteEqBands;
            const QSignalBlocker blockMode(eq_mode_);
            eq_mode_->setCurrentIndex(eq_mode_->findData(remoteEqBands));
            loadEqLayout();
            // Preserve a preset chosen by the CLI; ordinary EQ-band edits
            // already save "custom" to the same settings key.
            selectEqPreset(remoteEqPreset);
        }
        bool eqRemoteEdit = false;
        for (int i = 0; i < eq_active_bands_; ++i) {
            if (eq_faders_[i]->isSliderDown()) continue;
            const int level = std::clamp(c->eq_levels_hundredths[i].load(), -1200, 1200);
            if (eq_faders_[i]->value() == level) continue;
            const QSignalBlocker blocker(eq_faders_[i]);
            eq_faders_[i]->setValue(level);
            eq_values_[i]->setText(QString::number(level / 100.0, 'f', 2));
            eqRemoteEdit = true;
        }
        if (eqRemoteEdit) { saveEqLayout(); selectEqPreset(remoteEqPreset); updateEqOverview(); }
        if (remoteEqPreset != eq_selected_preset_) selectEqPreset(remoteEqPreset);
        tube_enabled_->setChecked(c->tube_enabled.load() != 0);
        {
            const QSignalBlocker a(crossfeed_enabled_), b(crossfeed_mode_);
            crossfeed_enabled_->setChecked(c->crossfeed_enabled.load() != 0);
            crossfeed_mode_->setCurrentIndex(std::clamp(c->crossfeed_preset.load(), 0, 2));
        }
        mb_enabled_->setChecked(c->mb_enabled.load() != 0);
        const int newCount = c->mb_band_count.load() == 5 ? 1 : 0;
        if (mb_band_count_->currentIndex() != newCount) {
            mb_band_count_->setCurrentIndex(newCount);
            updateMultibandVisibility();
        }
        for (int i=0; i<4; ++i) {
            auto* slider = mb_crossovers_[i].slider;
            if (!slider->isSliderDown()) {
                const QSignalBlocker lock(slider);
                slider->setValue(std::clamp(c->mb_crossovers_hz[i].load(),80,15000));
            }
        }
        for (int i=0; i<5; ++i) {
            auto& b = mb_bands_[i];
            const auto& src = c->mb_bands[i];
            {
                const QSignalBlocker lock(b.enabled);
                b.enabled->setChecked(src.enabled.load() != 0);
            }
            for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
                {b.threshold, std::clamp(src.threshold_db.load(),-48,0)},
                {b.ratio, std::clamp(src.ratio_tenths.load(),10,100)},
                {b.knee, std::clamp(src.knee_db.load(),0,12)},
                {b.gain, std::clamp(src.makeup_db.load(),0,24)},
                {b.attack, std::clamp(src.attack_ms.load(),1,100)},
                {b.release, std::clamp(src.release_ms.load(),5,500)}}) {
                if (!item.first.slider->isSliderDown()) {
                    const QSignalBlocker lock(item.first.slider);
                    item.first.slider->setValue(item.second);
                }
            }
        }
        fet_enabled_->setChecked(c->fet_enabled.load() != 0);
        deq_enabled_->setChecked(c->deq_enabled.load() != 0);
        updateDeqOverview();
        for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
            {fet_threshold_, std::clamp(c->fet_threshold_db.load(), -48, 0)},
            {fet_ratio_, std::clamp(c->fet_ratio_tenths.load(), 10, 100)},
            {fet_knee_, std::clamp(c->fet_knee_db.load(), 0, 12)},
            {fet_gain_, std::clamp(c->fet_makeup_db.load(), 0, 24)},
            {fet_attack_, std::clamp(c->fet_attack_ms.load(), 1, 100)},
            {fet_release_, std::clamp(c->fet_release_ms.load(), 5, 500)}}) {
            if (!item.first.slider->isSliderDown()) {
                const QSignalBlocker blocker(item.first.slider);
                item.first.slider->setValue(item.second);
            }
        }
        imager_enabled_->setChecked(c->imager_enabled.load() != 0);
        for (const auto& item : std::initializer_list<std::pair<SliderRow, int>>{
            {imager_low_, std::clamp(c->imager_low_width_percent.load(), 0, 200)},
            {imager_mid_, std::clamp(c->imager_mid_width_percent.load(), 0, 200)},
            {imager_high_, std::clamp(c->imager_high_width_percent.load(), 0, 200)},
            {imager_low_xover_, std::clamp(c->imager_low_crossover_hz.load(), 80, 400)},
            {imager_high_xover_, std::clamp(c->imager_high_crossover_hz.load()/100, 20, 80)}}) {
            if (!item.first.slider->isSliderDown()) {
                const QSignalBlocker block(item.first.slider);
                item.first.slider->setValue(item.second);
            }
        }
        tube_model_->setCurrentIndex(std::clamp(c->tube_model.load(), 0, 2));
        analogx_enabled_->setChecked(c->analogx_enabled.load() != 0);
        analogx_mode_->setCurrentIndex(std::clamp(c->analogx_mode.load(), 0, 2));
        if (!tube_drive_.slider->isSliderDown()) {
            const QSignalBlocker blocker(tube_drive_.slider);
            tube_drive_.slider->setValue(std::clamp(c->tube_drive_percent.load(), 0, 100));
        }
        if (!tube_mix_.slider->isSliderDown()) {
            const QSignalBlocker blocker(tube_mix_.slider);
            tube_mix_.slider->setValue(std::clamp(c->tube_mix_percent.load(), 0, 100));
        }
        spectrum_enabled_->setChecked(c->spectrum_enabled.load() != 0);
        headphone_enabled_->setChecked(c->headphone_enabled.load() != 0);
        headphone_quality_->setCurrentIndex(std::clamp(c->headphone_quality.load(), 0, 4));
        diff_enabled_->setChecked(c->diff_enabled.load() != 0);
        diff_reverse_->setChecked(c->diff_reverse.load() != 0);
        field_enabled_->setChecked(c->field_enabled.load() != 0);
        reverb_enabled_->setChecked(c->reverb_enabled.load() != 0);
        mono_enabled_->setChecked(c->bass_mono_enabled.load() != 0);
        mono_mode_->setCurrentIndex(std::clamp(c->bass_mono_mode.load(), 0, 2));
        psycho_enabled_->setChecked(c->psycho_enabled.load() != 0);
        psycho_order_->setCurrentIndex(std::clamp(c->psycho_order.load(), 2, 5)-2);
        dyn_enabled_->setChecked(c->dynamic_enabled.load() != 0);
        agc_enabled_->setChecked(c->agc_enabled.load() != 0);
        lufs_enabled_->setChecked(c->lufs_enabled.load() != 0);
        ddc_enabled_->setChecked(c->ddc_enabled.load() != 0);
        speaker_enabled_->setChecked(c->speaker_enabled.load() != 0);
        convolver_enabled_->setChecked(c->convolver_enabled.load() != 0);
        const int ddcStatus = c->ddc_status.load();
        const bool pendingDdc = c->ddc_request_serial.load() != c->ddc_applied_serial.load();
        ddc_status_->setText(pendingDdc || ddcStatus == 2 ? "Loading DDC profile…" :
            ddcStatus == 1 ? (ddc_enabled_->isChecked() ? "Native DDC correction active" : "DDC profile loaded (disabled)") :
            ddcStatus == -2 ? "DDC unavailable at this audio rate (requires 44.1 or 48 kHz)" :
            ddcStatus == -1 ? "Invalid/unsupported DDC profile; correction bypassed" :
            "No DDC profile loaded; correction bypassed");
        const int convstatus = c->convolver_status.load();
        const bool pendingIr = c->convolver_request_serial.load() != c->convolver_applied_serial.load();
        convolver_status_->setText(pendingIr || convstatus == 2 ? "Loading IRS in the running backend…" :
            convstatus == 1 ? "Native IR kernel loaded and ready" :
            convstatus == -2 ? "IR resampling failed; previous kernel may remain active" :
            convstatus == -1 ? "Invalid/unsupported IR; previous kernel may remain active" :
            "No impulse response loaded.");
        if (!convolver_cross_.slider->isSliderDown()) {
            const QSignalBlocker b(convolver_cross_.slider);
            convolver_cross_.slider->setValue(std::clamp(c->convolver_cross_percent.load(), 0, 100));
        }
        lufs_speed_->setCurrentIndex(std::clamp(c->lufs_speed.load(), 0, 2));
        const struct { SliderRow* row; std::int32_t value; int lo; int hi; } bassRows[] = {
            {&mono_strength_, c->bass_mono_strength_centi.load()/10, 5, 100},
            {&mono_frequency_, c->bass_mono_frequency_hz.load(), 15, 150},
            {&psycho_cutoff_, c->psycho_cutoff_hz.load(), 60, 150},
            {&psycho_intensity_, c->psycho_intensity_percent.load(), 0, 100},
            {&psycho_original_, c->psycho_original_percent.load(), 0, 100},
                        {&dyn_xhi_, c->dynamic_x_high.load(), 0, 12000},
            {&dyn_ylo_, c->dynamic_y_low.load(), 0, 200},
            {&dyn_yhi_, c->dynamic_y_high.load(), 0, 300},
            {&dyn_side_lo_, c->dynamic_side_low_percent.load(), 0, 100},
            {&dyn_side_hi_, c->dynamic_side_high_percent.load(), 0, 100},
            {&dyn_strength_, c->dynamic_strength_percent.load(), 0, 100}
        };
        if (!dyn_xlo_.slider->isSliderDown()) {
            dyn_xlo_native_ = std::clamp(c->dynamic_x_low.load(), 0, 2400);
            const QSignalBlocker block(dyn_xlo_.slider);
            dyn_xlo_.slider->setValue(std::max(120, dyn_xlo_native_));
        }
        for (const auto& item : bassRows) {
            if (item.row->slider->isSliderDown()) continue;
            const QSignalBlocker blocker(item.row->slider);
            item.row->slider->setValue(std::clamp(item.value,item.lo,item.hi));
        }
        const struct { SliderRow* row; std::int32_t value; int lo; int hi; } outputRows[] = {
            {&limiter_threshold_, c->limiter_threshold_permille.load(), 300, 1000},
            {&limiter_gain_, c->limiter_output_percent.load(), 1, 200},
            {&limiter_pan_, c->limiter_pan_percent.load(), -100, 100},
            {&agc_strength_, c->agc_strength_centi.load(), 50, 300},
            {&agc_max_gain_, c->agc_max_gain_centi.load(), 100, 1000},
            {&agc_threshold_, c->agc_threshold_permille.load(), 300, 1000},
            {&lufs_target_, c->lufs_target_tenths.load(), -240, -80},
            {&lufs_gain_, c->lufs_max_gain_tenths.load(), 0, 120}
        };
        for (const auto& item : outputRows) {
            if (item.row->slider->isSliderDown()) continue;
            const QSignalBlocker blocker(item.row->slider);
            item.row->slider->setValue(std::clamp(item.value, item.lo, item.hi));
        }
        const struct { SliderRow* row; std::int32_t value; } reverberation[] = {
            {&reverb_room_, c->reverb_room_percent.load()},
            {&reverb_width_, c->reverb_width_percent.load()},
            {&reverb_damp_, c->reverb_damp_percent.load()},
            {&reverb_wet_, c->reverb_wet_percent.load()},
            {&reverb_dry_, c->reverb_dry_percent.load()}
        };
        for (const auto& item : reverberation) {
            if (item.row->slider->isSliderDown()) continue;
            const QSignalBlocker blocker(item.row->slider);
            item.row->slider->setValue(std::clamp(item.value, 0, 100));
        }
        if (!field_widen_.slider->isSliderDown()) {
            const QSignalBlocker block(field_widen_.slider);
            field_widen_.slider->setValue(std::clamp(c->field_widen_tenths.load(), 0, 80));
        }
        if (!field_mid_.slider->isSliderDown()) {
            const QSignalBlocker block(field_mid_.slider);
            field_mid_.slider->setValue(std::clamp(c->field_mid_hundredths.load(), 100, 200));
        }
        if (!field_depth_.slider->isSliderDown()) {
            const QSignalBlocker block(field_depth_.slider);
            const int depth = c->field_depth.load();
            field_depth_.slider->setValue(depth == 0 ? 0 :
                std::clamp(static_cast<int>(std::lround((depth - 150) / 50.0)), 1, 16));
        }
        if (!diff_delay_.slider->isSliderDown()) {
            const QSignalBlocker block(diff_delay_.slider);
            diff_delay_.slider->setValue(std::clamp(c->diff_delay_ms.load(), 1, 20));
        }
        if (!diff_mix_.slider->isSliderDown()) {
            const QSignalBlocker block(diff_mix_.slider);
            diff_mix_.slider->setValue(std::clamp(c->diff_mix_percent.load(), 0, 100));
        }
        if (!diff_lowpass_.slider->isSliderDown()) {
            const QSignalBlocker block(diff_lowpass_.slider);
            diff_lowpass_.slider->setValue(std::clamp(c->diff_lp_hz.load() / 100, 0, 200));
        }
        const int rate = c->sample_rate_hz.load();
        if (rate != 0 && rate != 44100 && rate != 48000) {
            const QString warning = QString(
                "Current processing rate %1 Hz is unsupported by upstream VHE; "
                "Headphone Surround+ is inactive. Supported rates: 44.1 and 48 kHz.").arg(rate);
            headphone_quality_->setToolTip(warning);
            headphone_enabled_->setToolTip(warning);
        } else {
            const QString details =
                "Five distinct upstream VHE surround profiles, not a quality ranking. "
                "Supported at 44.1 and 48 kHz; switching levels may rebuild filters.";
            headphone_quality_->setToolTip(details);
            headphone_enabled_->setToolTip(details);
        }
        global_->setChecked(c->master_enabled.load() != 0);
        enabled_->setChecked(c->enabled.load() != 0);
        bass_mode_->setCurrentIndex(std::clamp(c->bass_mode.load(), 0, 2));
        clarity_enabled_->setChecked(c->clarity_enabled.load() != 0);
        clarity_mode_->setCurrentIndex(std::clamp(c->clarity_mode.load(), 0, 2));
        if (!clarity_gain_.slider->isSliderDown()) {
            const QSignalBlocker block(clarity_gain_.slider);
            clarity_gain_.slider->setValue(std::clamp(c->clarity_gain_milli.load() / 100, 0, 45));
        }
        if (!spectrum_frequency_.slider->isSliderDown()) {
            const QSignalBlocker block(spectrum_frequency_.slider);
            spectrum_frequency_.slider->setValue(std::clamp(c->spectrum_frequency_hz.load() / 100, 22, 82));
        }
        if (!spectrum_exciter_.slider->isSliderDown()) {
            const QSignalBlocker block(spectrum_exciter_.slider);
            spectrum_exciter_.slider->setValue(std::clamp(c->spectrum_exciter_milli.load() / 100, 0, 60));
        }
        if (!strength_.slider->isSliderDown()) {
            const QSignalBlocker block(strength_.slider);
            strength_.slider->setValue(std::clamp(c->strength_centi.load() / 10, 5, 100));
        }
        if (!frequency_.slider->isSliderDown()) {
            const QSignalBlocker block(frequency_.slider);
            frequency_.slider->setValue(std::clamp(c->frequency_hz.load(), 15, 150));
        }
        updateLabels();
        // CLI edits become visible within the polling interval and are also
        // retained in QSettings for the next backend startup. Persist only on
        // actual UI changes: otherwise the 500ms poll would keep resetting the
        // batched disk-save timer and prevent it from ever firing.
        if (before != liveUiSnapshot() || beforeNativeLowX != dyn_xlo_native_) persistCurrentSettings();
    }

    void pollBackend() {
        if (!ensureBackend()) {
            showOffline();
            return;
        }
        // If the user changed a control during disconnection, send that change
        // instead of overwriting it with an old backend snapshot.
        if (pending_edit_) {
            if (!publishToBackend()) return;
        } else {
            // No pending GUI edit: the backend is authoritative, including
            // modifications made with the command-line control utility.
            readBackend();
        }
        // Compare callback and non-silent input-block counters across 500ms
        // polls; always-process callbacks can continue with silent input.
        const auto* control = mapping_.get();
        const auto state = audio_activity_.sample(
            static_cast<pid_t>(control->owner_pid.load(std::memory_order_relaxed)),
            control->audio_callbacks.load(std::memory_order_relaxed),
            control->input_signal_blocks.load(std::memory_order_relaxed));
        switch (state) {
        case viperbass::BackendAudioActivity::Active:
            status_->setText("● Processing audio — input signal detected");
            status_->setStyleSheet("color: #209454;");
            break;
        case viperbass::BackendAudioActivity::NoSignal:
            status_->setText("○ Connected — no audio signal (DSP ready)");
            // Blue distinguishes connected silence from green input activity and muted idle.
            status_->setStyleSheet(palette().color(QPalette::Window).lightness() < 130
                ? "color: #75B9F4;" : "color: #2B75B8;");
            break;
        case viperbass::BackendAudioActivity::Idle:
            status_->setText("○ Connected — idle (effects processor ready)");
            status_->setStyleSheet(QString("color:%1;").arg(theme_muted_color_));
            break;
        case viperbass::BackendAudioActivity::Checking:
            status_->setText("○ Connected — checking audio activity…");
            status_->setStyleSheet(QString("color:%1;").arg(theme_muted_color_));
            break;
        }
    }
