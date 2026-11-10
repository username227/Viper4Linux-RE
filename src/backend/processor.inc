struct Processor {
    explicit Processor(unsigned long sample_rate)
        : rate(static_cast<uint32_t>(sample_rate)),
          wet_audio(2 * kChunk, 0.0f), dry_audio(2 * kChunk, 0.0f) {
        wet.SetSamplingRate(rate);
        dry.SetSamplingRate(rate);
        wet.ResetAllEffects();
        dry.ResetAllEffects();
        // Keep Bass disabled until the first run has read the saved controls.
        configure_bass(false, 0, 10.0f, 65.0f);
        configure_bass_mono(false, 0, 2.0f, 65);
        configure_psycho(false, 80, 0.30f, 3, 1.0f);
        configure_dynamic(false, 100, 6000, 100, 150, 0.5f, 0.5f, 0.0f);
        configure_clarity(false, 0, 1.5f);
        configure_spectrum(false, 7600, 1.0f);
        configure_tube(false, 0, 0.5f, 0.3f);
        configure_analogx(false, 0);
        configure_crossfeed(false, 0);
        configure_fet(false, -18, 20, 6, 0, 10, 100);
        configure_multiband(false, 3, {200, 4000, 5000, 8000}, {});
        configure_imager(false, 100, 100, 100, 200, 4000);
        configure_speaker(false);
        configure_ddc(false);
        configure_dynamic_eq(false, 0, {});
        configure_headphone(false, 0);
        configure_diff(false, 8, false, 0.5f, 0);
        configure_field(false, 1.0f, 1.0f, 300);
        configure_reverb(false, 0.5f, 0.5f, 0.5f, 0.2f, 0.5f);
        configure_limiter(0.5f, 1.0f, 0.0f);
        configure_agc(false, 1.0f, 3.0f, 0.9f);
        configure_lufs(false, -14.0f, 6.0f, 1);
        configure_convolver(false, 0.0f);
        configure_eq(false, 10, {});
        controls = acquire_controls();
        if (controls) {
            controls->sample_rate_hz.store(static_cast<int>(rate), std::memory_order_relaxed);
            apply_eq_from_controls();
            apply_tube_from_controls();
            apply_analogx_from_controls();
            apply_crossfeed_from_controls();
            apply_fet_from_controls();
            apply_multiband_from_controls();
            apply_imager_from_controls();
            apply_speaker_from_controls();
            load_ddc_profile(saved_ddc_path());
            apply_dynamic_eq_from_controls();
            const auto path = saved_ir_path();
            if (!path.empty()) {
                auto wave = viper_ir::read(path);
                bool resample_failed = false;
                if (wave.error.empty() && wave.rate != rate) {
                    const auto source_rate = wave.rate;
                    resample_failed = !viper_ir::resample_to(wave, rate);
                    if (!resample_failed)
                        std::fprintf(stderr, "ViPER: Convolver: resampled %s from %u to %u Hz\n",
                            path.c_str(), source_rate, rate);
                }
                if (!wave.error.empty()) {
                    std::fprintf(stderr, "ViPER: Convolver: %s: %s\n", path.c_str(), wave.error.c_str());
                    controls->convolver_status.store(resample_failed ? -2 : -1);
                } else {
                    const auto ok = wet.LoadConvolverKernel(wave.samples.data(),
                        static_cast<std::uint32_t>(wave.samples.size()/wave.channels), wave.channels, 1);
                    controls->convolver_ready.store(ok.has_value() ? 1 : 0);
                    controls->convolver_status.store(ok.has_value() ? 1 : -1);
                    if (!ok) std::fprintf(stderr, "ViPER: Convolver kernel rejected by native DSP.\n");
                }
            }
        }
        // One non-RT loader per LADSPA instance. The worker reads only a GUI
        // committed settings file; it never touches PipeWire's process buffers.
        if (controls) ir_worker = std::thread([this] { ir_worker_loop(); });
    }
    ~Processor() {
        ir_worker_stop.store(true, std::memory_order_release);
        if (ir_worker.joinable()) ir_worker.join();
        if (controls) release_controls();
    }

    void ir_worker_loop() {
#ifdef VIPER_MOCK_TEST_IPC
        // Regression hook: delay worker initialization so the mock can queue
        // requests first. Production builds do not contain this code.
        if (const char* delay = std::getenv("VIPER_MOCK_WORKER_START_DELAY_MS")) {
            const int ms = std::clamp(std::atoi(delay), 0, 1000);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
#endif
        // Start at the last acknowledged request, NOT the newest request.
        // Otherwise a command queued after instantiate() but before this
        // thread is scheduled can be silently mistaken for completed work.
        auto observed = controls->convolver_applied_serial.load(std::memory_order_acquire);
        auto observed_eq = controls->eq_applied_serial.load(std::memory_order_acquire);
        auto observed_tube = controls->tube_applied_serial.load(std::memory_order_acquire);
        auto observed_analogx = controls->analogx_applied_serial.load(std::memory_order_acquire);
        auto observed_crossfeed = controls->crossfeed_applied_serial.load(std::memory_order_acquire);
        auto observed_imager = controls->imager_applied_serial.load(std::memory_order_acquire);
        auto observed_speaker = controls->speaker_applied_serial.load(std::memory_order_acquire);
        auto observed_ddc = controls->ddc_applied_serial.load(std::memory_order_acquire);
        auto observed_fet = controls->fet_applied_serial.load(std::memory_order_acquire);
        auto observed_multiband = controls->mb_applied_serial.load(std::memory_order_acquire);
        auto observed_deq = controls->deq_applied_serial.load(std::memory_order_acquire);
        while (!ir_worker_stop.load(std::memory_order_acquire)) {
            const auto requested = controls->convolver_request_serial.load(std::memory_order_acquire);
            if (requested != observed) {
                observed = requested;
                const auto path = saved_ir_path();
                controls->convolver_status.store(2, std::memory_order_release); // loading
                // Validate/decode completely outside the audio path and mutex.
                viper_ir::Wave wave{};
                if (!path.empty()) wave = viper_ir::read(path);
                bool resample_failed = false;
                if (!path.empty() && wave.error.empty() && wave.rate != rate) {
                    const auto source_rate = wave.rate;
                    resample_failed = !viper_ir::resample_to(wave, rate);
                    if (!resample_failed)
                        std::fprintf(stderr, "ViPER: Convolver: resampled %s from %u to %u Hz\n",
                            path.c_str(), source_rate, rate);
                }
                if (!path.empty() && !wave.error.empty()) {
                    std::fprintf(stderr, "ViPER: Convolver: %s: %s\n", path.c_str(), wave.error.c_str());
                    controls->convolver_status.store(resample_failed ? -2 : -1, std::memory_order_release);
                } else {
                    ir_transition.store(true, std::memory_order_release);
                    // The audio callback ramps toward the safe dry path over 20ms.
                    std::this_thread::sleep_for(std::chrono::milliseconds(35));
                    {
                        std::lock_guard<std::mutex> lock(wet_mutex);
                        // The callback uses try_lock and never waits for this.
                        configure_convolver(false, 0.f);
                        wet.UnloadConvolverKernel();
                        bool ok = path.empty();
                        if (!path.empty()) {
                            ok = wet.LoadConvolverKernel(wave.samples.data(),
                                static_cast<std::uint32_t>(wave.samples.size()/wave.channels),
                                wave.channels, ++kernel_id).has_value();
                        }
                        controls->convolver_ready.store(!path.empty() && ok ? 1 : 0, std::memory_order_release);
                        controls->convolver_status.store(path.empty() ? 0 : (ok ? 1 : -1), std::memory_order_release);
                        // Discard delayed samples from the prior IR to keep the
                        // wet engine aligned after a dry-only loading interval.
                        wet.ResetBuffers();
                        // Enabling after the newly prepared kernel happens in run().
                        last_convolver_cross = -1.f;
                    }
                    ir_transition.store(false, std::memory_order_release);
                }
                controls->convolver_applied_serial.store(observed, std::memory_order_release);
            }
            // Coalesce rapid fader edits. The same worker that services IRS
            // requests owns EQ coefficient updates, never the audio callback.
            const auto eq_request = controls->eq_request_serial.load(std::memory_order_acquire);
            if (eq_request != observed_eq) {
                observed_eq = eq_request;
                apply_eq_from_controls();
                controls->eq_applied_serial.store(observed_eq, std::memory_order_release);
            }
            // Native tube model and drive reset filters: not on PipeWire's callback.
            const auto requested_tube = controls->tube_request_serial.load(std::memory_order_acquire);
            if (requested_tube != observed_tube) {
                observed_tube = requested_tube;
                apply_tube_from_controls();
                controls->tube_applied_serial.store(observed_tube, std::memory_order_release);
            }
            const auto requested_analogx = controls->analogx_request_serial.load(std::memory_order_acquire);
            if (requested_analogx != observed_analogx) {
                observed_analogx = requested_analogx;
                apply_analogx_from_controls();
                controls->analogx_applied_serial.store(observed_analogx, std::memory_order_release);
            }
            const auto crossfeed_request = controls->crossfeed_request_serial.load(std::memory_order_acquire);
            if (crossfeed_request != observed_crossfeed) {
                observed_crossfeed = crossfeed_request;
                apply_crossfeed_from_controls();
                controls->crossfeed_applied_serial.store(observed_crossfeed, std::memory_order_release);
            }
            const auto fet_request = controls->fet_request_serial.load(std::memory_order_acquire);
            if (fet_request != observed_fet) {
                observed_fet = fet_request;
                apply_fet_from_controls();
                controls->fet_applied_serial.store(observed_fet, std::memory_order_release);
            }
            const auto mb_request = controls->mb_request_serial.load(std::memory_order_acquire);
            if (mb_request != observed_multiband) {
                observed_multiband = mb_request;
                apply_multiband_from_controls();
                controls->mb_applied_serial.store(observed_multiband, std::memory_order_release);
            }
            const auto imager_request = controls->imager_request_serial.load(std::memory_order_acquire);
            if (imager_request != observed_imager) {
                observed_imager = imager_request;
                apply_imager_from_controls();
                controls->imager_applied_serial.store(observed_imager, std::memory_order_release);
            }
            const auto ddc_request = controls->ddc_request_serial.load(std::memory_order_acquire);
            if (ddc_request != observed_ddc) {
                observed_ddc = ddc_request;
                load_ddc_profile(saved_ddc_path());
                controls->ddc_applied_serial.store(observed_ddc, std::memory_order_release);
            }
            const auto speaker_request = controls->speaker_request_serial.load(std::memory_order_acquire);
            if (speaker_request != observed_speaker) {
                observed_speaker = speaker_request;
                apply_speaker_from_controls();
                controls->speaker_applied_serial.store(observed_speaker, std::memory_order_release);
            }
            const auto deq_request = controls->deq_request_serial.load(std::memory_order_acquire);
            if (deq_request != observed_deq) {
                observed_deq = deq_request;
                apply_dynamic_eq_from_controls();
                controls->deq_applied_serial.store(observed_deq, std::memory_order_release);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(35));
        }
    }

    void configure_ddc(bool enabled) {
        viper::DdcParams p{};
        p.enable = enabled;
        wet.ApplyDdc(p);
    }
    void load_ddc_profile(const std::string& path) {
        if (!controls) return;
        controls->ddc_status.store(2, std::memory_order_release);
        // Parse and stability-check entirely off PipeWire's RT callback.
        viper_ddc::Profile profile;
        if (!path.empty()) profile = viper_ddc::read(path);
        const bool valid = !path.empty() && static_cast<bool>(profile);
        const bool rate_ok = rate == 44100 || rate == 48000;
        ir_transition.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(35));
        {
            std::lock_guard<std::mutex> lock(wet_mutex);
            configure_ddc(false);
            // Clear old coefficients on invalid/removed profiles, preventing
            // stale correction after a failed profile switch.
            wet.LoadDdcCoefficients(nullptr, nullptr, 0);
            if (valid && rate_ok) {
                wet.LoadDdcCoefficients(profile.at_44100.data(), profile.at_48000.data(),
                    static_cast<std::uint32_t>(profile.at_44100.size()));
                configure_ddc(controls->ddc_enabled.load(std::memory_order_relaxed) != 0);
            }
        }
        ir_transition.store(false, std::memory_order_release);
        controls->ddc_ready.store(valid && rate_ok ? 1 : 0, std::memory_order_release);
        controls->ddc_status.store(path.empty() ? 0 : (!valid ? -1 : (rate_ok ? 1 : -2)), std::memory_order_release);
        if (!valid && !path.empty())
            std::fprintf(stderr, "ViPER: DDC: %s: %s\n", path.c_str(), profile.error.c_str());
        if (valid && !rate_ok)
            std::fprintf(stderr, "ViPER: DDC unsupported at %u Hz (use 44100/48000).\n", rate);
    }

    void configure_speaker(bool enabled) {
        viper::SpeakerCorrectionParams p{};
        p.enable = enabled;
        wet.ApplySpeakerCorrection(p);
        last_speaker_enabled = enabled;
    }
    void apply_speaker_from_controls() {
        if (!controls) return;
        const auto request = controls->speaker_request_serial.load(std::memory_order_acquire);
        const bool enabled = controls->speaker_enabled.load(std::memory_order_relaxed) != 0;
        if (request != controls->speaker_request_serial.load(std::memory_order_acquire)) return;
        if (enabled == last_speaker_enabled) return;
        std::lock_guard<std::mutex> lock(wet_mutex);
        configure_speaker(enabled); // Native Reset runs on the worker, not audio callback.
    }

    static int valid_eq_bands(int n) {
        return (n == 10 || n == 15 || n == 25 || n == 31) ? n : 10;
    }
    void configure_eq(bool enabled, int band_count, const std::array<float, 31>& levels) {
        viper::EqualizerParams p{};
        p.enable = enabled;
        p.band_count = static_cast<std::uint32_t>(valid_eq_bands(band_count));
        p.band_levels = levels;
        wet.ApplyEqualizer(p);
        eq_worker_bands = static_cast<int>(p.band_count);
    }
    void apply_eq_from_controls() {
        if (!controls) return;
        const auto request = controls->eq_request_serial.load(std::memory_order_acquire);
        const auto count = valid_eq_bands(controls->eq_band_count.load(std::memory_order_relaxed));
        const bool enabled = controls->eq_enabled.load(std::memory_order_relaxed) != 0;
        std::array<float, 31> levels{};
        for (int i = 0; i < count; ++i)
            levels[i] = std::clamp(controls->eq_levels_hundredths[i].load(std::memory_order_relaxed), -1200, 1200) / 100.f;
        // If the GUI is changing mode at the same time, discard this snapshot
        // and handle the newest committed request on the next worker pass.
        if (request != controls->eq_request_serial.load(std::memory_order_acquire)) return;
        const bool changing_layout = count != eq_worker_bands;
        if (changing_layout) {
            ir_transition.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        {
            std::lock_guard<std::mutex> guard(wet_mutex);
            configure_eq(enabled, count, levels);
        }
        if (changing_layout) ir_transition.store(false, std::memory_order_release);
    }

    void configure_tube(bool enabled, int model, float drive, float mix_level) {
        viper::TubeSimulatorParams p{};
        p.enable = enabled;
        p.model = std::clamp(model, 0, 2);
        p.drive = std::clamp(drive, 0.f, 1.f);
        p.mix = std::clamp(mix_level, 0.f, 1.f);
        wet.ApplyTubeSimulator(p); // The upstream DSP, no substitute effect.
        last_tube_enabled = p.enable;
        last_tube_model = p.model;
        last_tube_drive = p.drive;
        last_tube_mix = p.mix;
    }
    void apply_tube_from_controls() {
        if (!controls) return;
        const auto request = controls->tube_request_serial.load(std::memory_order_acquire);
        const bool enabled = controls->tube_enabled.load(std::memory_order_relaxed) != 0;
        const int model = std::clamp(controls->tube_model.load(std::memory_order_relaxed), 0, 2);
        const float drive = std::clamp(controls->tube_drive_percent.load(std::memory_order_relaxed), 0, 100) / 100.f;
        const float mix_level = std::clamp(controls->tube_mix_percent.load(std::memory_order_relaxed), 0, 100) / 100.f;
        if (request != controls->tube_request_serial.load(std::memory_order_acquire)) return;
        if (enabled == last_tube_enabled && model == last_tube_model &&
            drive == last_tube_drive && mix_level == last_tube_mix) return;
        // Silence the wet side briefly during native filter resets, without
        // locking the audio callback. The existing dry engine remains running.
        ir_transition.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        {
            std::lock_guard<std::mutex> lock(wet_mutex);
            configure_tube(enabled, model, drive, mix_level);
        }
        ir_transition.store(false, std::memory_order_release);
    }

    void configure_analogx(bool enabled, int mode) {
        viper::AnalogXParams p{};
        p.enable = enabled;
        p.mode = std::clamp(mode, 0, 2);
        wet.ApplyAnalogX(p); // Native ViPERDSP processing, no replacement DSP.
        last_analogx_enabled = p.enable;
        last_analogx_mode = p.mode;
    }
    void apply_analogx_from_controls() {
        if (!controls) return;
        const auto request = controls->analogx_request_serial.load(std::memory_order_acquire);
        const bool enabled = controls->analogx_enabled.load(std::memory_order_relaxed) != 0;
        const int mode = std::clamp(controls->analogx_mode.load(std::memory_order_relaxed), 0, 2);
        if (request != controls->analogx_request_serial.load(std::memory_order_acquire)) return;
        if (enabled == last_analogx_enabled && mode == last_analogx_mode) return;
        // Upstream AnalogX::Reset rebuilds filters on model/enable changes.
        // Let PipeWire use the dry path while the worker updates the wet engine.
        ir_transition.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        {
            std::lock_guard<std::mutex> lock(wet_mutex);
            configure_analogx(enabled, mode);
        }
        ir_transition.store(false, std::memory_order_release);
    }

    void configure_crossfeed(bool enabled, int preset) {
        viper::CureParams p{};
        p.enable = enabled;
        p.crossfeed_preset = std::clamp(preset, 0, 2);
        wet.ApplyCure(p); // The native ViPERDSP Cure crossfeed.
        last_crossfeed_enabled = p.enable;
        last_crossfeed_preset = p.crossfeed_preset;
    }
    void apply_crossfeed_from_controls() {
        if (!controls) return;
        const auto serial = controls->crossfeed_request_serial.load(std::memory_order_acquire);
        const bool enabled = controls->crossfeed_enabled.load(std::memory_order_relaxed) != 0;
        const int preset = std::clamp(controls->crossfeed_preset.load(std::memory_order_relaxed), 0, 2);
        if (serial != controls->crossfeed_request_serial.load(std::memory_order_acquire)) return;
        // A disabled preset edit changes saved settings but must not interrupt other effects.
        if (!enabled && !last_crossfeed_enabled) return;
        if (enabled == last_crossfeed_enabled && preset == last_crossfeed_preset) return;
        // Native Crossfeed presets/reset are updated outside the real-time callback.
        std::lock_guard<std::mutex> lock(wet_mutex);
        configure_crossfeed(enabled, preset);
    }

    // Native FET compressor expects logarithmic amplitude and a signed
    // compression slope (0 = 1:1, -0.5 = 2:1, -0.9 = 10:1).
    // Every call happens either at instance creation or under the worker lock.
    static viper::FetCompressorParams native_fet_params(
        bool enabled, int threshold_db, int ratio_tenths, int knee_db,
        int makeup_db, int attack_ms, int release_ms) {
        constexpr float kDbToNaturalLog = 0.11512925464970229f; // ln(10)/20
        viper::FetCompressorParams p{};
        p.enable = enabled;
        p.threshold = std::clamp(threshold_db, -48, 0) * kDbToNaturalLog;
        p.ratio = -(1.f - 10.f / std::clamp(ratio_tenths, 10, 100));
        p.knee = std::clamp(knee_db, 0, 12) * kDbToNaturalLog;
        p.knee_auto = false;
        p.gain = std::clamp(makeup_db, 0, 24) * kDbToNaturalLog;
        p.gain_auto = false;
        p.attack = std::clamp(attack_ms, 1, 100) / 1000.f;
        p.attack_auto = false;
        p.release = std::clamp(release_ms, 5, 500) / 1000.f;
        p.release_auto = false;
        p.knee_multi = 2.f;
        p.max_attack = 0.080f;
        p.max_release = 0.5f;
        p.crest = 0.2f;
        p.adapt = 2.5f;
        p.no_clip = true;
        return p;
    }
    void configure_fet(bool enabled, int threshold, int ratio, int knee,
                       int makeup, int attack, int release) {
        wet.ApplyFetCompressor(native_fet_params(enabled, threshold, ratio, knee,
                                                  makeup, attack, release));
        last_fet_enabled = enabled;
    }
    void apply_fet_from_controls() {
        if (!controls) return;
        const auto serial = controls->fet_request_serial.load(std::memory_order_acquire);
        const bool enabled = controls->fet_enabled.load(std::memory_order_relaxed) != 0;
        const int threshold = std::clamp(controls->fet_threshold_db.load(std::memory_order_relaxed), -48, 0);
        const int ratio = std::clamp(controls->fet_ratio_tenths.load(std::memory_order_relaxed), 10, 100);
        const int knee = std::clamp(controls->fet_knee_db.load(std::memory_order_relaxed), 0, 12);
        const int makeup = std::clamp(controls->fet_makeup_db.load(std::memory_order_relaxed), 0, 24);
        const int attack = std::clamp(controls->fet_attack_ms.load(std::memory_order_relaxed), 1, 100);
        const int release = std::clamp(controls->fet_release_ms.load(std::memory_order_relaxed), 5, 500);
        if (serial != controls->fet_request_serial.load(std::memory_order_acquire)) return;
        if (!enabled && !last_fet_enabled) return;
        std::lock_guard<std::mutex> lock(wet_mutex);
        configure_fet(enabled, threshold, ratio, knee, makeup, attack, release);
    }

    struct MultibandSnapshot {
        bool enabled = false;
        int threshold = -18, ratio = 20, knee = 6, makeup = 0, attack = 10, release = 100;
    };
    void configure_multiband(bool enabled, int band_count,
                             const std::array<int, 4>& crossovers,
                             const std::array<MultibandSnapshot, 5>& bands) {
        viper::MultibandCompressorParams p{};
        p.enable = enabled;
        p.band_count = static_cast<std::uint32_t>(band_count == 5 ? 5 : 3);
        for (int j = 0; j < band_count - 1; ++j)
            p.crossover_frequencies[j] = static_cast<float>(crossovers[j]);
        for (int i = 0; i < band_count; ++i) {
            const auto& s = bands[i];
            const auto native = native_fet_params(s.enabled, s.threshold, s.ratio, s.knee,
                                                   s.makeup, s.attack, s.release);
            auto& b = p.bands[i];
            b.enable = native.enable;
            b.threshold = native.threshold;
            b.ratio = native.ratio;
            b.knee = native.knee;
            b.knee_auto = native.knee_auto;
            b.gain = native.gain;
            b.gain_auto = native.gain_auto;
            b.attack = native.attack;
            b.attack_auto = native.attack_auto;
            b.release = native.release;
            b.release_auto = native.release_auto;
            b.knee_multi = native.knee_multi;
            b.max_attack = native.max_attack;
            b.max_release = native.max_release;
            b.crest = native.crest;
            b.adapt = native.adapt;
            b.no_clip = native.no_clip;
        }
        wet.ApplyMultibandCompressor(p);
        last_mb_enabled = enabled;
    }
    void apply_multiband_from_controls() {
        if (!controls) return;
        const auto serial = controls->mb_request_serial.load(std::memory_order_acquire);
        const bool enabled = controls->mb_enabled.load(std::memory_order_relaxed) != 0;
        const int count = controls->mb_band_count.load(std::memory_order_relaxed) == 5 ? 5 : 3;
        std::array<int, 4> crossover{};
        int last = 0;
        const int nyquist_ceiling = std::max(200, std::min(15000, static_cast<int>(rate / 2 - 100)));
        for (int i = 0; i < count - 1; ++i) {
            // Reserve 20 Hz for each later crossover: even out-of-order
            // GUI edits and low sample rates must not invert filter bands.
            const int floor = last + 20;
            const int ceiling = std::max(floor, nyquist_ceiling - 20 * (count - 2 - i));
            crossover[i] = std::clamp(controls->mb_crossovers_hz[i].load(std::memory_order_relaxed),
                                      floor, ceiling);
            last = crossover[i];
        }
        std::array<MultibandSnapshot, 5> bands{};
        for (int i = 0; i < count; ++i) {
            const auto& src = controls->mb_bands[i];
            auto& b = bands[i];
            b.enabled = src.enabled.load(std::memory_order_relaxed) != 0;
            b.threshold = std::clamp(src.threshold_db.load(std::memory_order_relaxed), -48, 0);
            b.ratio = std::clamp(src.ratio_tenths.load(std::memory_order_relaxed), 10, 100);
            b.knee = std::clamp(src.knee_db.load(std::memory_order_relaxed), 0, 12);
            b.makeup = std::clamp(src.makeup_db.load(std::memory_order_relaxed), 0, 24);
            b.attack = std::clamp(src.attack_ms.load(std::memory_order_relaxed), 1, 100);
            b.release = std::clamp(src.release_ms.load(std::memory_order_relaxed), 5, 500);
        }
        if (serial != controls->mb_request_serial.load(std::memory_order_acquire)) return;
        if (!enabled && !last_mb_enabled) return;
        // Native band/crossover rebuilding only under the worker mutex.
        std::lock_guard<std::mutex> lock(wet_mutex);
        configure_multiband(enabled, count, crossover, bands);
    }

    void configure_imager(bool enabled, int low_width, int mid_width, int high_width,
                          int low_crossover, int high_crossover) {
        viper::StereoImagerParams p{};
        p.enable = enabled;
        p.low_width = std::clamp(low_width, 0, 200) * 0.01f;
        p.mid_width = std::clamp(mid_width, 0, 200) * 0.01f;
        p.high_width = std::clamp(high_width, 0, 200) * 0.01f;
        p.low_crossover = static_cast<float>(std::clamp(low_crossover, 80, 400));
        p.high_crossover = static_cast<float>(std::clamp(high_crossover, 2000, 8000));
        wet.ApplyStereoImager(p);
        last_imager_enabled = enabled;
        last_imager_low = low_width;
        last_imager_mid = mid_width;
        last_imager_high = high_width;
        last_imager_low_xover = low_crossover;
        last_imager_high_xover = high_crossover;
    }
    void apply_imager_from_controls() {
        if (!controls) return;
        const auto serial = controls->imager_request_serial.load(std::memory_order_acquire);
        const bool enabled = controls->imager_enabled.load(std::memory_order_relaxed) != 0;
        const int low = std::clamp(controls->imager_low_width_percent.load(std::memory_order_relaxed), 0, 200);
        const int mid = std::clamp(controls->imager_mid_width_percent.load(std::memory_order_relaxed), 0, 200);
        const int high = std::clamp(controls->imager_high_width_percent.load(std::memory_order_relaxed), 0, 200);
        const int low_xover = std::clamp(controls->imager_low_crossover_hz.load(std::memory_order_relaxed), 80, 400);
        const int high_xover = std::clamp(controls->imager_high_crossover_hz.load(std::memory_order_relaxed), 2000, 8000);
        if (serial != controls->imager_request_serial.load(std::memory_order_acquire)) return;
        if (!enabled && !last_imager_enabled) return; // a disabled slider shouldn't interrupt music
        if (enabled == last_imager_enabled && low == last_imager_low && mid == last_imager_mid &&
            high == last_imager_high && low_xover == last_imager_low_xover &&
            high_xover == last_imager_high_xover) return;
        // Upstream crossover filters are reconstructed by these setters; never call
        // from the audio callback. This mutex is acquired with try_lock there.
        std::lock_guard<std::mutex> lock(wet_mutex);
        configure_imager(enabled, low, mid, high, low_xover, high_xover);
    }

    void configure_dynamic_eq(bool enabled, int band_count,
                              const std::array<viper::DynamicEqBandParams, 10>& bands) {
        viper::DynamicEqParams p{};
        p.enable = enabled;
        p.band_count = static_cast<uint32_t>(std::clamp(band_count, 0, 10));
        p.bands = bands;
        wet.ApplyDynamicEq(p); // Native ViPERDSP; no alternative EQ DSP.
        last_deq_enabled = enabled;
    }
    void apply_dynamic_eq_from_controls() {
        if (!controls) return;
        const auto serial = controls->deq_request_serial.load(std::memory_order_acquire);
        const int count = std::clamp(controls->deq_band_count.load(std::memory_order_relaxed), 1, 10);
        const bool enabled = controls->deq_enabled.load(std::memory_order_relaxed) != 0;
        std::array<viper::DynamicEqBandParams, 10> bands{};
        for (int i = 0; i < count; ++i) {
            const auto& b = controls->deq_bands[i];
            auto& d = bands[i];
            d.frequency = static_cast<float>(std::clamp(b.frequency_hz.load(std::memory_order_relaxed), 20, 20000));
            d.q = std::clamp(b.q_hundredths.load(std::memory_order_relaxed), 50, 800) / 100.f;
            d.gain = std::clamp(b.gain_hundredths.load(std::memory_order_relaxed), -1200, 1200) / 100.f;
            d.threshold = std::clamp(b.threshold_tenths.load(std::memory_order_relaxed), -800, 0) / 10.f;
            d.attack = static_cast<float>(std::clamp(b.attack_ms.load(std::memory_order_relaxed), 1, 100));
            d.release = static_cast<float>(std::clamp(b.release_ms.load(std::memory_order_relaxed), 10, 500));
            d.filter_type = std::clamp(b.filter_type.load(std::memory_order_relaxed), 0, 2);
        }
        if (serial != controls->deq_request_serial.load(std::memory_order_acquire)) return;
        // When already disabled, edits must not interrupt the OTHER active
        // effects. The latest controls remain in shared memory and are applied
        // on the next enable request. Neither reset filters nor fade to dry.
        if (!enabled && !last_deq_enabled) return;
        // The worker owns native Dynamic EQ filter updates. The callback
        // already uses wet_mutex::try_lock and never waits for these setters.
        // Do NOT invoke the global transition-to-dry here: that would bypass
        // *all* other active effects for ~25 ms on every band adjustment,
        // causing an audible warble. This short synchronized update only
        // excludes the callback for the duration of the native setters.
        // Expensive IRS kernel swaps retain their separate dry handoff.
        {
            std::lock_guard<std::mutex> guard(wet_mutex);
            configure_dynamic_eq(enabled, count, bands);
        }
    }

    void run_dry_while_loading(unsigned long frames) {
        auto* in_l = ports[InputLeft]; auto* in_r = ports[InputRight];
        auto* out_l = ports[OutputLeft]; auto* out_r = ports[OutputRight];
        if (!out_l || !out_r) return;
        for (unsigned long offset = 0; offset < frames;) {
            const auto n = static_cast<uint32_t>(std::min<unsigned long>(kChunk, frames-offset));
            for (uint32_t i = 0; i < n; ++i) {
                const auto x = offset+i;
                dry_audio[2*i] = in_l && std::isfinite(in_l[x]) ? in_l[x] : 0.f;
                dry_audio[2*i+1] = in_r && std::isfinite(in_r[x]) ? in_r[x] : 0.f;
            }
            dry.Process(dry_audio, n);
            for (uint32_t i = 0; i < n; ++i) {
                out_l[offset+i] = std::isfinite(dry_audio[2*i]) ? dry_audio[2*i] : 0.f;
                out_r[offset+i] = std::isfinite(dry_audio[2*i+1]) ? dry_audio[2*i+1] : 0.f;
            }
            offset += n;
        }
        mix = 0.f;
    }

    // Only the wet engine is configured with actual effects. The dry engine
    // remains clean and is used for the global 20 ms bypass crossfade.
    void configure_bass(bool enabled, int mode, float strength, float frequency) {
        viper::BassParams bass{};
        bass.enable = enabled;
        bass.mode = std::clamp(mode, 0, 2); // Natural / Pure Bass+ / Subwoofer
        bass.frequency = static_cast<uint32_t>(frequency);
        bass.gain = strength;
        bass.anti_pop = true;
        // When enabling Bass, establish the requested gain while disabled so
        // ViPERDSP resets its gain smoother to that value. Subsequent edits
        // while enabled retain the native smoothing behavior.
        if (enabled && !last_bass_enabled) {
            bass.enable = false;
            wet.ApplyBass(bass);
            bass.enable = true;
        }
        wet.ApplyBass(bass);
        last_bass_enabled = enabled;
        last_bass_mode = bass.mode;
        last_strength = strength;
        last_frequency = frequency;
    }

    void configure_bass_mono(bool enabled, int mode, float strength, int frequency) {
        viper::BassMonoParams p{};
        p.enable = enabled;
        p.mode = std::clamp(mode, 0, 2);
        p.frequency = static_cast<uint32_t>(frequency);
        p.gain = strength;
        p.anti_pop = true;
        // Match the Android effect's initial gain state. ViPERDSP resets the
        // Mono Bass smoother on enable, so seed the selected gain while off.
        // Ordinary parameter edits during playback retain native smoothing.
        if (enabled && !last_mono_enabled) {
            p.enable = false;
            wet.ApplyBassMono(p);
            p.enable = true;
        }
        wet.ApplyBassMono(p);
        last_mono_enabled = enabled;
        last_mono_mode = p.mode;
        last_mono_strength = strength;
        last_mono_frequency = frequency;
    }
    void configure_psycho(bool enabled, int cutoff, float intensity, int order, float original) {
        viper::PsychoacousticBassParams p{};
        p.enable = enabled;
        p.cutoff = static_cast<uint32_t>(cutoff);
        p.intensity = intensity;
        p.harmonic_order = static_cast<uint32_t>(order);
        p.original_level = original;
        wet.ApplyPsychoacousticBass(p);
        last_psycho_enabled = enabled;
        last_psycho_cutoff = cutoff;
        last_psycho_intensity = intensity;
        last_psycho_order = order;
        last_psycho_original = original;
    }
    void configure_dynamic(bool enabled, int xlo, int xhi, int ylo, int yhi,
                           float side_lo, float side_hi, float strength) {
        viper::DynamicSystemParams p{};
        p.enable = enabled;
        p.x_coeff_low = xlo;
        p.x_coeff_high = xhi;
        p.y_coeff_low = ylo;
        p.y_coeff_high = yhi;
        p.side_gain_low = side_lo;
        p.side_gain_high = side_hi;
        p.strength = strength;
        wet.ApplyDynamicSystem(p);
        last_dynamic_enabled = enabled;
        last_dynamic_xlo = xlo;
        last_dynamic_xhi = xhi;
        last_dynamic_ylo = ylo;
        last_dynamic_yhi = yhi;
        last_dynamic_side_lo = side_lo;
        last_dynamic_side_hi = side_hi;
        last_dynamic_strength = strength;
    }
    void configure_clarity(bool enabled, int mode, float strength) {
        viper::ClarityParams clarity{};
        clarity.enable = enabled;
        clarity.mode = mode; // 0 Natural, 1 Ozone, 2 XHiFi
        clarity.gain = strength;
        wet.ApplyClarity(clarity);
        last_clarity_enabled = enabled;
        last_clarity_mode = mode;
        last_clarity_gain = strength;
    }

    void configure_spectrum(bool enabled, int reference_hz, float exciter) {
        viper::SpectrumExtensionParams spectrum{};
        spectrum.enable = enabled;
        // Upstream calls this 'strength', but the value is a reference frequency in Hz.
        spectrum.strength = reference_hz;
        spectrum.exciter = exciter;
        wet.ApplySpectrumExtension(spectrum);
        last_spectrum_enabled = enabled;
        last_spectrum_frequency_hz = reference_hz;
        last_spectrum_exciter = exciter;
    }

    void configure_headphone(bool enabled, int quality) {
        viper::HeadphoneSurroundParams hs{};
        hs.enable = enabled;
        hs.quality = quality; // 0..4, ViPERDSP VHE convolution levels
        wet.ApplyHeadphoneSurround(hs);
        last_headphone_enabled = enabled;
        last_headphone_quality = quality;
    }

    void configure_diff(bool enabled, int delay, bool reverse, float mix, int lp_cutoff) {
        viper::DiffSurroundParams diff{};
        diff.enable = enabled;
        diff.delay = static_cast<float>(delay); // milliseconds, upstream expects 1..20
        diff.reverse = reverse;
        diff.wet_dry_mix = mix;
        diff.lp_cutoff = static_cast<float>(lp_cutoff); // 0 disables low-pass
        wet.ApplyDiffSurround(diff);
        last_diff_enabled = enabled;
        last_diff_delay = delay;
        last_diff_reverse = reverse;
        last_diff_mix = mix;
        last_diff_lp = lp_cutoff;
    }

    void configure_field(bool enabled, float widening, float mid_image, int depth) {
        viper::FieldSurroundParams field{};
        field.enable = enabled;
        field.widening = widening; // upstream native range 0..8
        field.mid_image = mid_image; // upstream native range 1..2
        field.depth = static_cast<short>(depth); // 0 disables depth, else 200..950
        wet.ApplyFieldSurround(field);
        last_field_enabled = enabled;
        last_field_widen = widening;
        last_field_mid = mid_image;
        last_field_depth = depth;
    }

    void configure_reverb(bool enabled, float room, float width, float damp, float wet_level, float dry_level) {
        viper::ReverbParams reverb{};
        reverb.enable = enabled;
        reverb.room_size = room;
        reverb.width = width;
        reverb.damp = damp;
        reverb.wet = wet_level;
        reverb.dry = dry_level;
        wet.ApplyReverb(reverb);
        last_reverb_enabled = enabled;
        last_reverb_room = room;
        last_reverb_width = width;
        last_reverb_damp = damp;
        last_reverb_wet = wet_level;
        last_reverb_dry = dry_level;
    }

    void configure_convolver(bool enabled, float cross_channel) {
        viper::ConvolverParams p{};
        p.enable = enabled;
        p.cross_channel = cross_channel;
        wet.ApplyConvolver(p);
        last_convolver_enabled = enabled;
        last_convolver_cross = cross_channel;
    }

    void configure_limiter(float threshold, float output, float pan) {
        viper::MasterLimiterParams p{};
        p.threshold = threshold;
        p.output_volume = output;
        p.channel_pan = pan;
        // Same output protection on both paths; master bypass never disables it.
        wet.ApplyMasterLimiter(p);
        dry.ApplyMasterLimiter(p);
        last_limiter_threshold = threshold;
        last_limiter_output = output;
        last_limiter_pan = pan;
    }
    void configure_agc(bool enabled, float strength, float max_gain, float threshold) {
        viper::PlaybackGainControlParams p{};
        p.enable = enabled;
        p.strength = strength;
        p.max_gain = max_gain;
        p.output_threshold = threshold;
        wet.ApplyPlaybackGainControl(p);
        last_agc_enabled = enabled;
        last_agc_strength = strength;
        last_agc_max_gain = max_gain;
        last_agc_threshold = threshold;
    }
    void configure_lufs(bool enabled, float target, float max_gain, int speed) {
        viper::LufsParams p{};
        p.enable = enabled;
        p.target = target;
        p.max_gain = max_gain;
        p.speed = speed;
        wet.ApplyLufs(p);
        last_lufs_enabled = enabled;
        last_lufs_target = target;
        last_lufs_max_gain = max_gain;
        last_lufs_speed = speed;
    }

    #include "processor-run.inc"

    float read_control(Port p, float fallback) const {
        const float* ptr = ports[p];
        return ptr && std::isfinite(*ptr) ? *ptr : fallback;
    }

    const uint32_t rate;
    LADSPA_Data* ports[PortCount]{};
    ViPER wet;
    ViPER dry;
    std::mutex wet_mutex; // worker holds while replacing kernel / EQ; RT uses try_lock only
    int eq_worker_bands = 10; // worker/constructor only; never read by RT callback
    std::thread ir_worker;
    std::atomic<bool> ir_worker_stop{false};
    std::atomic<bool> ir_transition{false};
    uint32_t kernel_id = 1;
    viperbass::Controls* controls = nullptr;
    std::vector<float> wet_audio;
    std::vector<float> dry_audio;
    bool last_convolver_enabled = false;
    float last_convolver_cross = -1.0f;
    bool last_bass_enabled = false;
    int last_bass_mode = -1;
    float last_strength = -1.0f;
    float last_frequency = -1.0f;
    bool last_mono_enabled = false;
    int last_mono_mode = -1;
    float last_mono_strength = -1.f;
    int last_mono_frequency = -1;
    bool last_psycho_enabled = false;
    int last_psycho_cutoff = -1;
    float last_psycho_intensity = -1.f;
    int last_psycho_order = -1;
    float last_psycho_original = -1.f;
    bool last_dynamic_enabled = false;
    int last_dynamic_xlo = -1;
    int last_dynamic_xhi = -1;
    int last_dynamic_ylo = -1;
    int last_dynamic_yhi = -1;
    float last_dynamic_side_lo = -1.f;
    float last_dynamic_side_hi = -1.f;
    float last_dynamic_strength = -1.f;
    bool last_clarity_enabled = false;
    int last_clarity_mode = -1;
    float last_clarity_gain = -1.0f;
    bool last_tube_enabled = false;
    int last_tube_model = 0;
    float last_tube_drive = 0.5f;
    float last_tube_mix = 0.3f;
    bool last_analogx_enabled = false;
    bool last_crossfeed_enabled = false;
    int last_crossfeed_preset = 0;
    bool last_fet_enabled = false;
    bool last_mb_enabled = false;
    bool last_imager_enabled = false;
    bool last_speaker_enabled = false;
    int last_imager_low = 100, last_imager_mid = 100, last_imager_high = 100;
    int last_imager_low_xover = 200, last_imager_high_xover = 4000;
    bool last_deq_enabled = false;
    int last_analogx_mode = 0;
    bool last_spectrum_enabled = false;
    int last_spectrum_frequency_hz = -1;
    float last_spectrum_exciter = -1.0f;
    bool last_headphone_enabled = false;
    int last_headphone_quality = -1;
    bool last_diff_enabled = false;
    int last_diff_delay = -1;
    bool last_diff_reverse = false;
    float last_diff_mix = -1.0f;
    int last_diff_lp = -1;
    bool last_field_enabled = false;
    float last_field_widen = -1.0f;
    float last_field_mid = -1.0f;
    int last_field_depth = -1;
    float last_limiter_threshold = -1.0f;
    float last_limiter_output = -1.0f;
    float last_limiter_pan = -2.0f;
    bool last_agc_enabled = false;
    float last_agc_strength = -1.0f;
    float last_agc_max_gain = -1.0f;
    float last_agc_threshold = -1.0f;
    bool last_lufs_enabled = false;
    float last_lufs_target = 0.0f;
    float last_lufs_max_gain = -1.0f;
    int last_lufs_speed = -1;
    bool last_reverb_enabled = false;
    float last_reverb_room = -1.0f;
    float last_reverb_width = -1.0f;
    float last_reverb_damp = -1.0f;
    float last_reverb_wet = -1.0f;
    float last_reverb_dry = -1.0f;
    float mix = 1.0f;
};

