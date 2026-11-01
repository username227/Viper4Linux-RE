// ViPERDSP stereo LADSPA adapter for PipeWire's filter-chain.
// Prototype only. The underlying ViPERDSP rights must be reviewed before distribution.
#include <ladspa.h>
#include "viperdsp_compat.h" // Include missing upstream standard headers first.
#include "viper/ViPER.h"
#include "viper-control.h"
#include "viper-ir-wave.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <string>
#include <new>
#include <mutex>
#include <cstdio>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

namespace {
constexpr uint32_t kChunk = 2048;
constexpr unsigned long kPluginId = 2780030;

enum Port : unsigned long {
    InputLeft, InputRight, OutputLeft, OutputRight,
    BassEnabled, BassStrength, BassFrequency,
    PortCount
};

// One shared control page per filter-chain process, even if LADSPA is
// instantiated more than once during audio reconfiguration.
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
            else if (key == "crossfeed_enabled") state.crossfeed_enabled.store(value == "true" || value == "1");
            else if (key == "crossfeed_preset") state.crossfeed_preset.store(std::clamp(std::stoi(value), 0, 2));
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

}

std::mutex control_mutex;
viperbass::Mapping control_mapping;
unsigned control_users = 0;
viperbass::Controls* acquire_controls() {
    std::lock_guard<std::mutex> lock(control_mutex);
    if (control_users == 0) {
        if (!control_mapping.create_server()) {
            std::fprintf(stderr, "ViPER: Cannot create clean control channel; DSP will use fallback defaults.\n");
            // Audio still functions using LADSPA defaults if another instance owns IPC.
            // Do not increment the reference count after a failed setup.
            return nullptr;
        }
        load_saved_settings(*control_mapping.get());
    }
    ++control_users;
    return control_mapping.get();
}
void release_controls() {
    std::lock_guard<std::mutex> lock(control_mutex);
    if (control_users && --control_users == 0) control_mapping.stop_server();
}

struct Processor {
    explicit Processor(unsigned long sample_rate)
        : rate(static_cast<uint32_t>(sample_rate)),
          wet_audio(2 * kChunk, 0.0f), dry_audio(2 * kChunk, 0.0f) {
        wet.SetSamplingRate(rate);
        dry.SetSamplingRate(rate);
        wet.ResetAllEffects();
        dry.ResetAllEffects();
        configure_bass(true, 0, 10.0f, 65.0f);
        configure_bass_mono(false, 0, 2.0f, 65);
        configure_psycho(false, 80, 0.30f, 3, 1.0f);
        configure_dynamic(false, 100, 6000, 100, 150, 0.5f, 0.5f, 0.0f);
        configure_clarity(false, 0, 1.5f);
        configure_spectrum(false, 7600, 1.0f);
        configure_tube(false, 0, 0.5f, 0.3f);
        configure_analogx(false, 0);
        configure_crossfeed(false, 0);
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
        auto observed = controls->convolver_request_serial.load(std::memory_order_acquire);
        auto observed_eq = controls->eq_request_serial.load(std::memory_order_acquire);
        auto observed_tube = controls->tube_request_serial.load(std::memory_order_acquire);
        auto observed_analogx = controls->analogx_request_serial.load(std::memory_order_acquire);
        auto observed_crossfeed = controls->crossfeed_request_serial.load(std::memory_order_acquire);
        auto observed_deq = controls->deq_request_serial.load(std::memory_order_acquire);
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
            const auto deq_request = controls->deq_request_serial.load(std::memory_order_acquire);
            if (deq_request != observed_deq) {
                observed_deq = deq_request;
                apply_dynamic_eq_from_controls();
                controls->deq_applied_serial.store(observed_deq, std::memory_order_release);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(35));
        }
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

    void run(unsigned long frames) {
        if (controls) controls->audio_callbacks.fetch_add(1, std::memory_order_relaxed);
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
        const float target = (!ir_transition.load(std::memory_order_relaxed) && master_enabled && (bass_enabled || mono_enabled || psycho_enabled || dynamic_enabled || clarity_enabled || spectrum_enabled || tube_enabled || analogx_enabled || crossfeed_enabled || (controls && controls->deq_enabled.load(std::memory_order_relaxed) != 0) || headphone_enabled || diff_enabled || field_enabled || reverb_enabled || agc_enabled || lufs_enabled || convolver_enabled || (controls && controls->eq_enabled.load(std::memory_order_relaxed) != 0))) ? 1.0f : 0.0f;
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

LADSPA_Handle create(const LADSPA_Descriptor*, unsigned long rate) {
    try { return new Processor(rate); } catch (...) { return nullptr; }
}
void attach(LADSPA_Handle handle, unsigned long index, LADSPA_Data* data) {
    if (handle && index < PortCount) static_cast<Processor*>(handle)->ports[index] = data;
}
void run(LADSPA_Handle handle, unsigned long frames) {
    if (handle) static_cast<Processor*>(handle)->run(frames);
}
void destroy(LADSPA_Handle handle) { delete static_cast<Processor*>(handle); }

const LADSPA_PortDescriptor descriptors[] = {
    LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO, LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO, LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL, LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL
};
const char* names[] = {
    "Input L", "Input R", "Output L", "Output R",
    "Bass Enabled", "Bass Strength", "Bass Frequency"
};
const LADSPA_PortRangeHint hints[] = {
    {}, {}, {}, {},
    { LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_1, 0.0f, 1.0f },
    { LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_MAXIMUM, 0.5f, 10.0f },
    { LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_MIDDLE, 15.0f, 150.0f },
};

const LADSPA_Descriptor descriptor = {
    kPluginId, "viperbass_stereo", 0,
    "ViPER for Linux Clean — ViPER Bass", "ViPER for Linux prototype",
    "ViPERDSP upstream rights reserved; prototype is not for redistribution",
    PortCount, descriptors, names, hints, nullptr,
    create, attach, nullptr, run, nullptr, nullptr, nullptr, destroy
};
} // namespace

extern "C" const LADSPA_Descriptor* ladspa_descriptor(unsigned long index) {
    return index == 0 ? &descriptor : nullptr;
}
