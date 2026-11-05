// Startup preferences are loaded only when the LADSPA instance is created.
// No filesystem access occurs in the real-time audio callback. Match Qt's
// native Unix QSettings INI path for ViPERforLinux / ViPERBass.
std::string config_directory() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    if (xdg && *xdg) return xdg;
    if (home && *home) return std::string(home) + "/.config";
    return {};
}
std::string saved_ir_path() {
    const auto dir = config_directory();
    if (dir.empty()) return {};
    std::ifstream file(dir + "/ViPERforLinux/ViPERClean.conf");
    if (!file) return {};
    bool general = false;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "[General]") { general = true; continue; }
        if (!line.empty() && line.front() == '[') { general = false; continue; }
        if (general && line.rfind("convolver_kernel=", 0) == 0)
            return line.substr(std::strlen("convolver_kernel="));
    }
    return {};
}
std::string saved_ddc_path() {
    const auto dir = config_directory();
    if (dir.empty()) return {};
    std::ifstream file(dir + "/ViPERforLinux/ViPERClean.conf");
    if (!file) return {};
    bool general = false;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "[General]") { general = true; continue; }
        if (!line.empty() && line.front() == '[') { general = false; continue; }
        if (general && line.rfind("ddc_profile=", 0) == 0)
            return line.substr(std::strlen("ddc_profile="));
    }
    return {};
}
void load_saved_settings(viperbass::Controls& state) {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    std::string dir;
    if (xdg && *xdg) dir = xdg;
    else if (home && *home) dir = std::string(home) + "/.config";
    else return;
    std::ifstream file(dir + "/ViPERforLinux/ViPERClean.conf");
    if (!file) return;
    bool general = false;
    bool eq_hundredths = false;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[') {
            general = line == "[General]";
            continue;
        }
        if (!general) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto value = line.substr(eq + 1);
        // All values written by this Qt GUI use plain ASCII integers/bools.
        try {
            if (key == "enabled") state.enabled.store(value == "true" || value == "1");
            else if (key == "master_enabled") state.master_enabled.store(value == "true" || value == "1");
            else if (key == "bass_mode") state.bass_mode.store(
                std::clamp(std::stoi(value), 0, 2));
            else if (key == "strength_tenths") state.strength_centi.store(
                std::clamp(std::stoi(value), 5, 100) * 10);
            else if (key == "frequency") state.frequency_hz.store(
                std::clamp(std::stoi(value), 15, 150));
            else if (key == "eq_precision_hundredths") eq_hundredths = value == "true" || value == "1";
            else if (key == "eq_enabled") state.eq_enabled.store(value == "true" || value == "1");
            else if (key == "eq_band_count") {
                const int n = std::stoi(value);
                if (n == 10 || n == 15 || n == 25 || n == 31) state.eq_band_count.store(n);
            }
            else if (key == "convolver_enabled") state.convolver_enabled.store(value == "true" || value == "1");
            else if (key == "convolver_cross_percent") state.convolver_cross_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "clarity_enabled") state.clarity_enabled.store(value == "true" || value == "1");
            else if (key == "clarity_mode") state.clarity_mode.store(
                std::clamp(std::stoi(value), 0, 2));
            else if (key == "clarity_gain_tenths") state.clarity_gain_milli.store(
                std::clamp(std::stoi(value), 0, 45) * 100);
            else if (key == "spectrum_enabled") state.spectrum_enabled.store(value == "true" || value == "1");
            else if (key == "spectrum_frequency_hundreds") state.spectrum_frequency_hz.store(
                std::clamp(std::stoi(value), 22, 82) * 100);
            else if (key == "spectrum_exciter_tenths") state.spectrum_exciter_milli.store(
                std::clamp(std::stoi(value), 0, 60) * 100);
            else if (key == "tube_enabled") state.tube_enabled.store(value == "true" || value == "1");
            else if (key == "tube_model") state.tube_model.store(std::clamp(std::stoi(value), 0, 2));
            else if (key == "tube_drive_percent") state.tube_drive_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "tube_mix_percent") state.tube_mix_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "analogx_enabled") state.analogx_enabled.store(value == "true" || value == "1");
            else if (key == "analogx_mode") state.analogx_mode.store(std::clamp(std::stoi(value), 0, 2));
            else if (key == "deq_enabled") state.deq_enabled.store(value == "true" || value == "1");
            else if (key == "deq_band_count") state.deq_band_count.store(std::clamp(std::stoi(value), 1, 10));
            else if (key.rfind("deq_b", 0) == 0) {
                const auto underscore = key.find('_', 5);
                if (underscore != std::string::npos) {
                    const int idx = std::stoi(key.substr(5, underscore - 5));
                    if (idx >= 0 && idx < 10) {
                        auto& b = state.deq_bands[idx];
                        const int n = std::stoi(value);
                        const std::string attr = key.substr(underscore + 1);
                        if (attr == "freq") b.frequency_hz.store(std::clamp(n, 20, 20000));
                        else if (attr == "q") b.q_hundredths.store(std::clamp(n, 50, 800));
                        else if (attr == "gain") b.gain_hundredths.store(std::clamp(n, -1200, 1200));
                        else if (attr == "threshold") b.threshold_tenths.store(std::clamp(n, -800, 0));
                        else if (attr == "attack") b.attack_ms.store(std::clamp(n, 1, 100));
                        else if (attr == "release") b.release_ms.store(std::clamp(n, 10, 500));
                        else if (attr == "type") b.filter_type.store(std::clamp(n, 0, 2));
                    }
                }
            }
            else if (key == "mb_enabled") state.mb_enabled.store(value == "true" || value == "1");
            else if (key == "mb_band_count") {
                const int n = std::stoi(value);
                if (n == 3 || n == 5) state.mb_band_count.store(n);
            }
            else if (key.rfind("mb_x", 0) == 0 && key.size() == 8 && key.substr(5) == "_hz") {
                const int i = std::stoi(key.substr(4, 1));
                if (i >= 0 && i < 4) state.mb_crossovers_hz[i].store(std::clamp(std::stoi(value), 80, 15000));
            }
            else if (key.rfind("mb_b", 0) == 0) {
                const auto sep = key.find('_', 4);
                if (sep != std::string::npos) {
                    const int index = std::stoi(key.substr(4, sep - 4));
                    if (index >= 0 && index < 5) {
                        auto& b = state.mb_bands[index];
                        const auto field = key.substr(sep + 1);
                        if (field == "enabled") b.enabled.store(value == "true" || value == "1");
                        else if (field == "threshold_db") b.threshold_db.store(std::clamp(std::stoi(value), -48, 0));
                        else if (field == "ratio_tenths") b.ratio_tenths.store(std::clamp(std::stoi(value), 10, 100));
                        else if (field == "knee_db") b.knee_db.store(std::clamp(std::stoi(value), 0, 12));
                        else if (field == "makeup_db") b.makeup_db.store(std::clamp(std::stoi(value), 0, 24));
                        else if (field == "attack_ms") b.attack_ms.store(std::clamp(std::stoi(value), 1, 100));
                        else if (field == "release_ms") b.release_ms.store(std::clamp(std::stoi(value), 5, 500));
                    }
                }
            }
            else if (key == "fet_enabled") state.fet_enabled.store(value == "true" || value == "1");
            else if (key == "fet_threshold_db") state.fet_threshold_db.store(std::clamp(std::stoi(value), -48, 0));
            else if (key == "fet_ratio_tenths") state.fet_ratio_tenths.store(std::clamp(std::stoi(value), 10, 100));
            else if (key == "fet_knee_db") state.fet_knee_db.store(std::clamp(std::stoi(value), 0, 12));
            else if (key == "fet_makeup_db") state.fet_makeup_db.store(std::clamp(std::stoi(value), 0, 24));
            else if (key == "fet_attack_ms") state.fet_attack_ms.store(std::clamp(std::stoi(value), 1, 100));
            else if (key == "fet_release_ms") state.fet_release_ms.store(std::clamp(std::stoi(value), 5, 500));
            else if (key == "crossfeed_enabled") state.crossfeed_enabled.store(value == "true" || value == "1");
            else if (key == "crossfeed_preset") state.crossfeed_preset.store(std::clamp(std::stoi(value), 0, 2));
            else if (key == "ddc_enabled") state.ddc_enabled.store(value == "true" || value == "1");
            else if (key == "speaker_enabled") state.speaker_enabled.store(value == "true" || value == "1");
            else if (key == "imager_enabled") state.imager_enabled.store(value == "true" || value == "1");
            else if (key == "imager_low_width_percent") state.imager_low_width_percent.store(std::clamp(std::stoi(value), 0, 200));
            else if (key == "imager_mid_width_percent") state.imager_mid_width_percent.store(std::clamp(std::stoi(value), 0, 200));
            else if (key == "imager_high_width_percent") state.imager_high_width_percent.store(std::clamp(std::stoi(value), 0, 200));
            else if (key == "imager_low_crossover_hz") state.imager_low_crossover_hz.store(std::clamp(std::stoi(value), 80, 400));
            else if (key == "imager_high_crossover_hz") state.imager_high_crossover_hz.store(std::clamp(std::stoi(value), 2000, 8000));
            else if (key == "headphone_enabled") state.headphone_enabled.store(value == "true" || value == "1");
            else if (key == "headphone_quality") state.headphone_quality.store(
                std::clamp(std::stoi(value), 0, 4));
            else if (key == "field_enabled") state.field_enabled.store(value == "true" || value == "1");
            else if (key == "field_widen_tenths") state.field_widen_tenths.store(
                std::clamp(std::stoi(value), 0, 80));
            else if (key == "field_mid_hundredths") state.field_mid_hundredths.store(
                std::clamp(std::stoi(value), 100, 200));
            else if (key == "field_depth_step") {
                const int step = std::clamp(std::stoi(value), 0, 16);
                state.field_depth.store(step ? 150 + step * 50 : 0);
            }
            else if (key == "reverb_enabled") state.reverb_enabled.store(value == "true" || value == "1");
            else if (key == "reverb_room_percent") state.reverb_room_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "reverb_width_percent") state.reverb_width_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "reverb_damp_percent") state.reverb_damp_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "reverb_wet_percent") state.reverb_wet_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "reverb_dry_percent") state.reverb_dry_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "bass_mono_enabled") state.bass_mono_enabled.store(value == "true" || value == "1");
            else if (key == "bass_mono_mode") state.bass_mono_mode.store(std::clamp(std::stoi(value), 0, 2));
            else if (key == "bass_mono_strength_centi") state.bass_mono_strength_centi.store(std::clamp(std::stoi(value), 50, 1000));
            else if (key == "bass_mono_frequency_hz") state.bass_mono_frequency_hz.store(std::clamp(std::stoi(value), 15, 150));
            else if (key == "psycho_enabled") state.psycho_enabled.store(value == "true" || value == "1");
            else if (key == "psycho_cutoff_hz") state.psycho_cutoff_hz.store(std::clamp(std::stoi(value), 60, 150));
            else if (key == "psycho_intensity_percent") state.psycho_intensity_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "psycho_order") state.psycho_order.store(std::clamp(std::stoi(value), 2, 5));
            else if (key == "psycho_original_percent") state.psycho_original_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "dynamic_enabled") state.dynamic_enabled.store(value == "true" || value == "1");
            else if (key == "dynamic_x_low") state.dynamic_x_low.store(std::clamp(std::stoi(value), 0, 2400));
            else if (key == "dynamic_x_high") state.dynamic_x_high.store(std::clamp(std::stoi(value), 0, 12000));
            else if (key == "dynamic_y_low") state.dynamic_y_low.store(std::clamp(std::stoi(value), 0, 200));
            else if (key == "dynamic_y_high") state.dynamic_y_high.store(std::clamp(std::stoi(value), 0, 300));
            else if (key == "dynamic_side_low_percent") state.dynamic_side_low_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "dynamic_side_high_percent") state.dynamic_side_high_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "dynamic_strength_percent") state.dynamic_strength_percent.store(std::clamp(std::stoi(value), 0, 100));
            else if (key == "limiter_threshold_permille") state.limiter_threshold_permille.store(std::clamp(std::stoi(value), 300, 1000));
            else if (key == "limiter_output_percent") state.limiter_output_percent.store(std::clamp(std::stoi(value), 1, 200));
            else if (key == "limiter_pan_percent") state.limiter_pan_percent.store(std::clamp(std::stoi(value), -100, 100));
            else if (key == "agc_enabled") state.agc_enabled.store(value == "true" || value == "1");
            else if (key == "agc_strength_centi") state.agc_strength_centi.store(std::clamp(std::stoi(value), 50, 300));
            else if (key == "agc_max_gain_centi") state.agc_max_gain_centi.store(std::clamp(std::stoi(value), 100, 1000));
            else if (key == "agc_threshold_permille") state.agc_threshold_permille.store(std::clamp(std::stoi(value), 300, 1000));
            else if (key == "lufs_enabled") state.lufs_enabled.store(value == "true" || value == "1");
            else if (key == "lufs_target_tenths") state.lufs_target_tenths.store(std::clamp(std::stoi(value), -240, -80));
            else if (key == "lufs_max_gain_tenths") state.lufs_max_gain_tenths.store(std::clamp(std::stoi(value), 0, 120));
            else if (key == "lufs_speed") state.lufs_speed.store(std::clamp(std::stoi(value), 0, 2));
            else if (key == "diff_enabled") state.diff_enabled.store(value == "true" || value == "1");
            else if (key == "diff_delay_ms") state.diff_delay_ms.store(
                std::clamp(std::stoi(value), 1, 20));
            else if (key == "diff_reverse") state.diff_reverse.store(value == "true" || value == "1");
            else if (key == "diff_mix_percent") state.diff_mix_percent.store(
                std::clamp(std::stoi(value), 0, 100));
            else if (key == "diff_lp_hundreds") state.diff_lp_hz.store(
                std::clamp(std::stoi(value), 0, 200) * 100);
        } catch (const std::exception&) { /* Leave that setting at its safe default. */ }
    }
    // Load only the chosen layout's levels. Other layouts remain stored in
    // Qt settings for the GUI, independently of active backend controls.
    file.clear();
    file.seekg(0);
    general = false;
    const int bands = state.eq_band_count.load();
    const std::string eq_key = "eq_levels_" + std::to_string(bands) + "=";
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line.front() == '[') { general = line == "[General]"; continue; }
        if (!general || line.rfind(eq_key, 0) != 0) continue;
        std::string raw = line.substr(eq_key.size());
        // Qt QSettings may quote strings containing commas in INI files.
        if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"')
            raw = raw.substr(1, raw.size()-2);
        std::stringstream stream(raw);
        std::string part;
        for (int i = 0; i < bands && std::getline(stream, part, ','); ++i) {
            try {
                const int saved = std::stoi(part);
                // Existing user settings were in tenths; new settings use 0.01dB.
                state.eq_levels_hundredths[i].store(
                    std::clamp(eq_hundredths ? saved : saved * 10, -1200, 1200));
            }
            catch (...) { state.eq_levels_hundredths[i].store(0); }
        }
        break;
    }


    // QSettings groups precision keys in a separate [precision] INI section.
    // Apply them AFTER the older [General] slider values, independently of
    // the order of sections on disk. Legacy configuration remains supported.
    file.clear();
    file.seekg(0);
    bool precise = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            precise = line == "[precision]";
            continue;
        }
        if (!precise) continue;
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        const auto key = line.substr(0, pos);
        const auto value = line.substr(pos + 1);
        try {
            const int n = std::stoi(value);
            if (key == "strength_centi" && n >= 50 && n <= 1000)
                state.strength_centi.store(n);
            else if (key == "bass_mono_strength_centi" && n >= 50 && n <= 1000)
                state.bass_mono_strength_centi.store(n);
            else if (key == "clarity_gain_milli" && n >= 0 && n <= 4500)
                state.clarity_gain_milli.store(n);
            else if (key == "spectrum_frequency_hz" && n >= 2200 && n <= 8200)
                state.spectrum_frequency_hz.store(n);
            else if (key == "spectrum_exciter_milli" && n >= 0 && n <= 6000)
                state.spectrum_exciter_milli.store(n);
            else if (key == "diff_lp_hz" && n >= 0 && n <= 20000)
                state.diff_lp_hz.store(n);
            else if (key == "field_depth" && (n == 0 || (n >= 200 && n <= 950)))
                state.field_depth.store(n);
        } catch (const std::exception&) { /* Ignore invalid override; keep the legacy value. */ }
    }

}

