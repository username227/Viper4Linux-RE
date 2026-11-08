    void run(unsigned long frames) {
        if (controls) {
            controls->audio_callbacks.fetch_add(1, std::memory_order_relaxed);
            // Observe sanitized-input activity even during a temporary wet-engine
            // lock miss. This never changes samples or the always-process policy.
            if (viperbass::hasInputSignal(ports[InputLeft], ports[InputRight], frames))
                controls->input_signal_blocks.fetch_add(1, std::memory_order_relaxed);
        }
        std::unique_lock<std::mutex> wet_guard(wet_mutex, std::try_to_lock);
        if (!wet_guard.owns_lock()) {
            // Never block PipeWire while the background loader constructs FFT kernels.
            run_dry_while_loading(frames);
            return;
        }
        // Local shared-memory reads are lock-free and do not perform system calls.
        const float strength = controls
            ? controls->strength_centi.load(std::memory_order_relaxed) * 0.01f
            : read_control(BassStrength, 10.0f);
        const float frequency = controls
            ? static_cast<float>(controls->frequency_hz.load(std::memory_order_relaxed))
            : read_control(BassFrequency, 65.0f);
        const bool bass_enabled = controls
            ? controls->enabled.load(std::memory_order_relaxed) != 0
            : read_control(BassEnabled, 1.0f) >= 0.5f;
        const int bass_mode = controls
            ? std::clamp(controls->bass_mode.load(std::memory_order_relaxed), 0, 2) : 0;
        const bool mono_enabled = controls && controls->bass_mono_enabled.load(std::memory_order_relaxed) != 0;
        const int mono_mode = controls ? std::clamp(controls->bass_mono_mode.load(std::memory_order_relaxed), 0, 2) : 0;
        const float mono_strength = controls ? std::clamp(controls->bass_mono_strength_centi.load(std::memory_order_relaxed), 50, 1000) * 0.01f : 2.0f;
        const int mono_frequency = controls ? std::clamp(controls->bass_mono_frequency_hz.load(std::memory_order_relaxed), 15, 150) : 65;
        const bool psycho_enabled = controls && controls->psycho_enabled.load(std::memory_order_relaxed) != 0;
        const int psycho_cutoff = controls ? std::clamp(controls->psycho_cutoff_hz.load(std::memory_order_relaxed), 60, 150) : 80;
        const float psycho_intensity = controls ? std::clamp(controls->psycho_intensity_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.30f;
        const int psycho_order = controls ? std::clamp(controls->psycho_order.load(std::memory_order_relaxed), 2, 5) : 3;
        const float psycho_original = controls ? std::clamp(controls->psycho_original_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 1.f;
        const bool dynamic_enabled = controls && controls->dynamic_enabled.load(std::memory_order_relaxed) != 0;
        const int dyn_xlo = controls ? std::clamp(controls->dynamic_x_low.load(std::memory_order_relaxed), 0, 2400) : 100;
        const int dyn_xhi = controls ? std::clamp(controls->dynamic_x_high.load(std::memory_order_relaxed), 0, 12000) : 6000;
        const int dyn_ylo = controls ? std::clamp(controls->dynamic_y_low.load(std::memory_order_relaxed), 0, 200) : 100;
        const int dyn_yhi = controls ? std::clamp(controls->dynamic_y_high.load(std::memory_order_relaxed), 0, 300) : 150;
        const float dyn_side_lo = controls ? std::clamp(controls->dynamic_side_low_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.5f;
        const float dyn_side_hi = controls ? std::clamp(controls->dynamic_side_high_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.5f;
        const float dyn_strength = controls ? std::clamp(controls->dynamic_strength_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.f;
        const bool clarity_enabled = controls &&
            controls->clarity_enabled.load(std::memory_order_relaxed) != 0;
        const int clarity_mode = controls
            ? std::clamp(controls->clarity_mode.load(std::memory_order_relaxed), 0, 2) : 0;
        const float clarity_gain = controls
            ? std::clamp(controls->clarity_gain_milli.load(std::memory_order_relaxed) * 0.001f, 0.0f, 4.5f)
            : 1.5f;
        const bool spectrum_enabled = controls &&
            controls->spectrum_enabled.load(std::memory_order_relaxed) != 0;
        const int spectrum_frequency = controls
            ? std::clamp(controls->spectrum_frequency_hz.load(std::memory_order_relaxed), 2200, 8200)
            : 7600;
        const float spectrum_exciter = controls
            ? std::clamp(controls->spectrum_exciter_milli.load(std::memory_order_relaxed) * 0.001f, 0.0f, 6.0f)
            : 1.0f;
        const bool tube_enabled = controls &&
            controls->tube_enabled.load(std::memory_order_relaxed) != 0;
        const bool analogx_enabled = controls &&
            controls->analogx_enabled.load(std::memory_order_relaxed) != 0;
        const bool mb_enabled = controls && controls->mb_enabled.load(std::memory_order_relaxed) != 0;
        const bool fet_enabled = controls && controls->fet_enabled.load(std::memory_order_relaxed) != 0;
        const bool ddc_enabled = controls && controls->ddc_enabled.load(std::memory_order_relaxed) != 0 && controls->ddc_ready.load(std::memory_order_relaxed) != 0;
        const bool speaker_enabled = controls && controls->speaker_enabled.load(std::memory_order_relaxed) != 0;
        const bool imager_enabled = controls &&
            controls->imager_enabled.load(std::memory_order_relaxed) != 0;
        const bool crossfeed_enabled = controls &&
            controls->crossfeed_enabled.load(std::memory_order_relaxed) != 0;
        const bool requested_headphone = controls &&
            controls->headphone_enabled.load(std::memory_order_relaxed) != 0;
        const int headphone_quality = controls
            ? std::clamp(controls->headphone_quality.load(std::memory_order_relaxed), 0, 4) : 0;
        // The upstream VHE convolution kernels support 44.1 and 48 kHz only.
        // At other PipeWire processing rates leave the toggle saved but don't
        // activate an unsupported kernel. The GUI reports the actual rate.
        const bool headphone_enabled = requested_headphone && (rate == 44100 || rate == 48000);
        const bool diff_enabled = controls &&
            controls->diff_enabled.load(std::memory_order_relaxed) != 0;
        const int diff_delay = controls
            ? std::clamp(controls->diff_delay_ms.load(std::memory_order_relaxed), 1, 20) : 8;
        const bool diff_reverse = controls &&
            controls->diff_reverse.load(std::memory_order_relaxed) != 0;
        const float diff_mix = controls
            ? std::clamp(controls->diff_mix_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.5f;
        // Keep low-pass frequency below Nyquist even on uncommon PipeWire rates.
        const int diff_max_lp = rate > 400 ? std::min(20000, static_cast<int>(rate / 2 - 100)) : 0;
        const int diff_lp = controls
            ? std::clamp(controls->diff_lp_hz.load(std::memory_order_relaxed), 0, diff_max_lp) : 0;
        const bool field_enabled = controls &&
            controls->field_enabled.load(std::memory_order_relaxed) != 0;
        const float field_widen = controls
            ? std::clamp(controls->field_widen_tenths.load(std::memory_order_relaxed), 0, 80) * 0.1f : 1.0f;
        const float field_mid = controls
            ? std::clamp(controls->field_mid_hundredths.load(std::memory_order_relaxed), 100, 200) * 0.01f : 1.0f;
        const int field_depth_raw = controls ? controls->field_depth.load(std::memory_order_relaxed) : 300;
        const int field_depth = field_depth_raw == 0 ? 0 : std::clamp(field_depth_raw, 200, 950);
        const bool reverb_enabled = controls &&
            controls->reverb_enabled.load(std::memory_order_relaxed) != 0;
        const float reverb_room = controls
            ? std::clamp(controls->reverb_room_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.5f;
        const float reverb_width = controls
            ? std::clamp(controls->reverb_width_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.5f;
        const float reverb_damp = controls
            ? std::clamp(controls->reverb_damp_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.5f;
        const float reverb_wet = controls
            ? std::clamp(controls->reverb_wet_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.2f;
        const float reverb_dry = controls
            ? std::clamp(controls->reverb_dry_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.5f;
        const float limiter_threshold = controls ?
            std::clamp(controls->limiter_threshold_permille.load(std::memory_order_relaxed), 300, 1000) * 0.001f : 0.5f;
        const float limiter_output = controls ?
            std::clamp(controls->limiter_output_percent.load(std::memory_order_relaxed), 1, 200) * 0.01f : 1.0f;
        const float limiter_pan = controls ?
            std::clamp(controls->limiter_pan_percent.load(std::memory_order_relaxed), -100, 100) * 0.01f : 0.0f;
        const bool agc_enabled = controls && controls->agc_enabled.load(std::memory_order_relaxed) != 0;
        const float agc_strength = controls ?
            std::clamp(controls->agc_strength_centi.load(std::memory_order_relaxed), 50, 300) * 0.01f : 1.0f;
        const float agc_max_gain = controls ?
            std::clamp(controls->agc_max_gain_centi.load(std::memory_order_relaxed), 100, 1000) * 0.01f : 3.0f;
        const float agc_threshold = controls ?
            std::clamp(controls->agc_threshold_permille.load(std::memory_order_relaxed), 300, 1000) * 0.001f : 0.9f;
        const bool lufs_enabled = controls && controls->lufs_enabled.load(std::memory_order_relaxed) != 0;
        const float lufs_target = controls ?
            std::clamp(controls->lufs_target_tenths.load(std::memory_order_relaxed), -240, -80) * 0.1f : -14.0f;
        const float lufs_max_gain = controls ?
            std::clamp(controls->lufs_max_gain_tenths.load(std::memory_order_relaxed), 0, 120) * 0.1f : 6.0f;
        const int lufs_speed = controls ?
            std::clamp(controls->lufs_speed.load(std::memory_order_relaxed), 0, 2) : 1;
        const bool master_enabled = !controls ||
            controls->master_enabled.load(std::memory_order_relaxed) != 0;
        const bool convolver_enabled = controls &&
            controls->convolver_ready.load(std::memory_order_relaxed) != 0 &&
            controls->convolver_enabled.load(std::memory_order_relaxed) != 0;
        const float convolver_cross = controls ?
            std::clamp(controls->convolver_cross_percent.load(std::memory_order_relaxed), 0, 100) * 0.01f : 0.0f;
        const float target = (!ir_transition.load(std::memory_order_relaxed) && master_enabled && (bass_enabled || mono_enabled || psycho_enabled || dynamic_enabled || clarity_enabled || spectrum_enabled || tube_enabled || analogx_enabled || crossfeed_enabled || imager_enabled || speaker_enabled || ddc_enabled || fet_enabled || mb_enabled || (controls && controls->deq_enabled.load(std::memory_order_relaxed) != 0) || headphone_enabled || diff_enabled || field_enabled || reverb_enabled || agc_enabled || lufs_enabled || convolver_enabled || (controls && controls->eq_enabled.load(std::memory_order_relaxed) != 0))) ? 1.0f : 0.0f;
        const float safe_strength = std::clamp(strength, 0.5f, 10.0f);
        const float safe_frequency = std::clamp(frequency, 15.0f, 150.0f);
        // Apply only changed parameters. Avoid resetting an effect on every
        // PipeWire audio block; parameter updates stay on the same RT thread.
        if (bass_enabled != last_bass_enabled || bass_mode != last_bass_mode ||
            safe_strength != last_strength || safe_frequency != last_frequency)
            configure_bass(bass_enabled, bass_mode, safe_strength, safe_frequency);
        if (mono_enabled != last_mono_enabled || mono_mode != last_mono_mode ||
            mono_strength != last_mono_strength || mono_frequency != last_mono_frequency)
            configure_bass_mono(mono_enabled, mono_mode, mono_strength, mono_frequency);
        if (psycho_enabled != last_psycho_enabled || psycho_cutoff != last_psycho_cutoff ||
            psycho_intensity != last_psycho_intensity || psycho_order != last_psycho_order ||
            psycho_original != last_psycho_original)
            configure_psycho(psycho_enabled, psycho_cutoff, psycho_intensity, psycho_order, psycho_original);
        if (dynamic_enabled != last_dynamic_enabled || dyn_xlo != last_dynamic_xlo ||
            dyn_xhi != last_dynamic_xhi || dyn_ylo != last_dynamic_ylo || dyn_yhi != last_dynamic_yhi ||
            dyn_side_lo != last_dynamic_side_lo || dyn_side_hi != last_dynamic_side_hi ||
            dyn_strength != last_dynamic_strength)
            configure_dynamic(dynamic_enabled, dyn_xlo, dyn_xhi, dyn_ylo, dyn_yhi, dyn_side_lo, dyn_side_hi, dyn_strength);
        if (clarity_enabled != last_clarity_enabled || clarity_mode != last_clarity_mode ||
            clarity_gain != last_clarity_gain)
            configure_clarity(clarity_enabled, clarity_mode, clarity_gain);
        if (spectrum_enabled != last_spectrum_enabled || spectrum_frequency != last_spectrum_frequency_hz ||
            spectrum_exciter != last_spectrum_exciter)
            configure_spectrum(spectrum_enabled, spectrum_frequency, spectrum_exciter);
        if (headphone_enabled != last_headphone_enabled || headphone_quality != last_headphone_quality)
            configure_headphone(headphone_enabled, headphone_quality);
        if (diff_enabled != last_diff_enabled || diff_delay != last_diff_delay ||
            diff_reverse != last_diff_reverse || diff_mix != last_diff_mix || diff_lp != last_diff_lp)
            configure_diff(diff_enabled, diff_delay, diff_reverse, diff_mix, diff_lp);
        if (field_enabled != last_field_enabled || field_widen != last_field_widen ||
            field_mid != last_field_mid || field_depth != last_field_depth)
            configure_field(field_enabled, field_widen, field_mid, field_depth);
        if (reverb_enabled != last_reverb_enabled || reverb_room != last_reverb_room ||
            reverb_width != last_reverb_width || reverb_damp != last_reverb_damp ||
            reverb_wet != last_reverb_wet || reverb_dry != last_reverb_dry)
            configure_reverb(reverb_enabled, reverb_room, reverb_width, reverb_damp, reverb_wet, reverb_dry);

        if (convolver_enabled != last_convolver_enabled || convolver_cross != last_convolver_cross)
            configure_convolver(convolver_enabled, convolver_cross);

        if (limiter_threshold != last_limiter_threshold || limiter_output != last_limiter_output ||
            limiter_pan != last_limiter_pan)
            configure_limiter(limiter_threshold, limiter_output, limiter_pan);
        if (agc_enabled != last_agc_enabled || agc_strength != last_agc_strength ||
            agc_max_gain != last_agc_max_gain || agc_threshold != last_agc_threshold)
            configure_agc(agc_enabled, agc_strength, agc_max_gain, agc_threshold);
        if (lufs_enabled != last_lufs_enabled || lufs_target != last_lufs_target ||
            lufs_max_gain != last_lufs_max_gain || lufs_speed != last_lufs_speed)
            configure_lufs(lufs_enabled, lufs_target, lufs_max_gain, lufs_speed);

        auto* in_l = ports[InputLeft];
        auto* in_r = ports[InputRight];
        auto* out_l = ports[OutputLeft];
        auto* out_r = ports[OutputRight];
        if (!out_l || !out_r) return;
        const float step = 1.0f / std::max(1.0f, rate * 0.020f);
        unsigned long offset = 0;
        while (offset < frames) {
            const auto n = static_cast<uint32_t>(std::min<unsigned long>(kChunk, frames - offset));
            for (uint32_t i = 0; i < n; ++i) {
                const auto ix = offset + i;
                // Unity input: ViPERDSP receives the unattenuated stream.
                // Preserve NaN/Inf sanitization; no hidden gain or headroom stage.
                const float l = in_l && std::isfinite(in_l[ix]) ? in_l[ix] : 0.0f;
                const float r = in_r && std::isfinite(in_r[ix]) ? in_r[ix] : 0.0f;
                wet_audio[2 * i] = dry_audio[2 * i] = l;
                wet_audio[2 * i + 1] = dry_audio[2 * i + 1] = r;
            }
            wet.Process(wet_audio, n);
            dry.Process(dry_audio, n);
            for (uint32_t i = 0; i < n; ++i) {
                if (mix < target) mix = std::min(target, mix + step);
                else if (mix > target) mix = std::max(target, mix - step);
                const float l = dry_audio[2 * i] + mix * (wet_audio[2 * i] - dry_audio[2 * i]);
                const float r = dry_audio[2 * i + 1] + mix * (wet_audio[2 * i + 1] - dry_audio[2 * i + 1]);
                // Native ViPERDSP lookahead limiter handles the configured ceiling
                // in wet AND dry paths; don't add a distortion-producing hard clip.
                out_l[offset + i] = std::isfinite(l) ? l : 0.0f;
                out_r[offset + i] = std::isfinite(r) ? r : 0.0f;
            }
            offset += n;
        }
    }

