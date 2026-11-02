#pragma once
// CLI-only presentation of the existing shared-memory DSP state.
// No DSP writes, ABI changes, background process, or Qt dependencies.
#include "viper-control.h"
#include "viper-eq-presets.h"
#include <initializer_list>
#include <ostream>
#include <string>

namespace viperbass {
namespace cli_report {
struct FullDetails {
    std::string selectedPreset;
    std::string ddcProfile;
    std::string convolverKernel;
};
enum class Effect {
    Master, Limiter, Agc, Lufs,
    Bass, Mono, Psycho, Dynamic,
    Eq, Deq, Clarity, Spectrum, Tube, Analogx,
    Crossfeed, Headphone, Differential, Field, Reverb, Imager,
    Fet, Multiband,
    Ddc, Speaker, Convolver,
    Unknown
};

inline bool starts(const std::string& s, const char* p) {
    return s.rfind(p, 0) == 0;
}
inline Effect fromCommand(const std::string& cmd) {
    if (cmd == "status" || cmd == "on" || cmd == "off" ||
        cmd == "all-on" || cmd == "all-off") return Effect::Master;
    if (starts(cmd, "bass-") || cmd == "toggle") return Effect::Bass;
    if (starts(cmd, "mono-")) return Effect::Mono;
    if (starts(cmd, "psycho-")) return Effect::Psycho;
    if (starts(cmd, "dynamic-")) return Effect::Dynamic;
    if (starts(cmd, "eq-")) return Effect::Eq;
    if (starts(cmd, "deq-")) return Effect::Deq;
    if (starts(cmd, "clarity-")) return Effect::Clarity;
    if (starts(cmd, "spectrum-")) return Effect::Spectrum;
    if (starts(cmd, "tube-")) return Effect::Tube;
    if (starts(cmd, "analogx-")) return Effect::Analogx;
    if (starts(cmd, "crossfeed-")) return Effect::Crossfeed;
    if (starts(cmd, "headphone-")) return Effect::Headphone;
    if (starts(cmd, "diff-")) return Effect::Differential;
    if (starts(cmd, "field-")) return Effect::Field;
    if (starts(cmd, "reverb-")) return Effect::Reverb;
    if (starts(cmd, "imager-")) return Effect::Imager;
    if (starts(cmd, "fet-")) return Effect::Fet;
    if (starts(cmd, "mb-")) return Effect::Multiband;
    if (starts(cmd, "ddc-")) return Effect::Ddc;
    if (starts(cmd, "speaker-")) return Effect::Speaker;
    if (starts(cmd, "convolver-")) return Effect::Convolver;
    if (starts(cmd, "limiter-")) return Effect::Limiter;
    if (starts(cmd, "agc-")) return Effect::Agc;
    if (starts(cmd, "lufs-")) return Effect::Lufs;
    return Effect::Unknown;
}
inline const char* enabled(int x) { return x ? "ON" : "OFF"; }
inline const char* bassMode(int x) {
    switch (x) { case 0: return "Natural"; case 1: return "Pure Bass+";
                 case 2: return "Subwoofer"; default: return "Unknown"; }
}
inline const char* clarityMode(int x) {
    switch (x) { case 0: return "Natural"; case 1: return "Ozone";
                 case 2: return "XHiFi"; default: return "Unknown"; }
}
inline const char* tubeModel(int x) {
    switch (x) { case 0: return "12AX7 (High Gain)"; case 1: return "12AU7 (Low Gain)";
                 case 2: return "6N1P (Medium Gain)"; default: return "Unknown"; }
}
inline const char* strengthMode(int x) {
    switch (x) { case 0: return "Mild"; case 1: return "Medium";
                 case 2: return "Strong"; default: return "Unknown"; }
}
inline const char* lufsSpeed(int x) {
    switch (x) { case 0: return "Slow"; case 1: return "Normal";
                 case 2: return "Fast"; default: return "Unknown"; }
}
inline const char* ddcStatus(int x) {
    switch (x) { case 1: return "READY"; case 2: return "LOADING";
        case -1: return "INVALID"; case -2: return "UNSUPPORTED AUDIO RATE";
        default: return "NONE"; }
}
inline const char* irStatus(int x) {
    switch(x) { case 1: return "READY"; case 2: return "LOADING";
        case -1: return "INVALID"; case -2: return "RESAMPLE ERROR";
        default: return "NONE"; }
}
inline void detail(std::ostream& out, const char* label, double value, const char* unit = "") {
    out << "      " << label << ": " << value << unit << '\n';
}
inline void detail(std::ostream& out, const char* label, const char* value) {
    out << "      " << label << ": " << value << '\n';
}

inline void effect(std::ostream& out, const Controls& c, Effect e, bool full,
                   int eqBand = -1, bool expanded = false, const FullDetails& info = {}) {
    const auto heading = [&](const char* letter, const char* name, const char* state) {
        out << (full ? "  " : "") << letter << ". " << name << " - " << state << '\n';
    };
    switch (e) {
    case Effect::Master:
        out << "Master processing: " << (c.master_enabled.load() ? "ON" : "BYPASSED") << '\n';
        break;
    case Effect::Limiter:
        heading("A", "Master Limiter", "ALWAYS ON");
        detail(out, "Threshold", c.limiter_threshold_permille.load() / 1000.0);
        detail(out, "Output", c.limiter_output_percent.load(), "%");
        detail(out, "Pan", c.limiter_pan_percent.load(), "%");
        break;
    case Effect::Agc:
        heading("B", "Playback Gain Control (AGC)", enabled(c.agc_enabled.load()));
        detail(out, "Strength", c.agc_strength_centi.load() / 100.0);
        detail(out, "Maximum gain", c.agc_max_gain_centi.load() / 100.0);
        detail(out, "Threshold", c.agc_threshold_permille.load() / 1000.0);
        break;
    case Effect::Lufs:
        heading("C", "LUFS Targeting", enabled(c.lufs_enabled.load()));
        detail(out, "Target", c.lufs_target_tenths.load() / 10.0, " LUFS");
        detail(out, "Max adjustment", c.lufs_max_gain_tenths.load() / 10.0, " dB");
        detail(out, "Response speed", lufsSpeed(c.lufs_speed.load()));
        break;
    case Effect::Bass:
        heading("A", "ViPER Bass", enabled(c.enabled.load()));
        detail(out, "Mode", bassMode(c.bass_mode.load()));
        detail(out, "Strength", c.strength_centi.load() / 100.0);
        detail(out, "Frequency", c.frequency_hz.load(), " Hz");
        break;
    case Effect::Mono:
        heading("B", "ViPER Bass Mono", enabled(c.bass_mono_enabled.load()));
        detail(out, "Mode", bassMode(c.bass_mono_mode.load()));
        detail(out, "Strength", c.bass_mono_strength_centi.load() / 100.0);
        detail(out, "Frequency", c.bass_mono_frequency_hz.load(), " Hz");
        break;
    case Effect::Psycho:
        heading("C", "Psychoacoustic Bass", enabled(c.psycho_enabled.load()));
        detail(out, "Cutoff", c.psycho_cutoff_hz.load(), " Hz");
        detail(out, "Intensity", c.psycho_intensity_percent.load(), "%");
        detail(out, "Filter order", c.psycho_order.load());
        detail(out, "Original signal", c.psycho_original_percent.load(), "%");
        break;
    case Effect::Dynamic:
        heading("D", "Dynamic System", enabled(c.dynamic_enabled.load()));
        detail(out, "X low", c.dynamic_x_low.load());
        detail(out, "X high", c.dynamic_x_high.load());
        detail(out, "Y low", c.dynamic_y_low.load());
        detail(out, "Y high", c.dynamic_y_high.load());
        detail(out, "Side low", c.dynamic_side_low_percent.load(), "%");
        detail(out, "Side high", c.dynamic_side_high_percent.load(), "%");
        detail(out, "Strength", c.dynamic_strength_percent.load(), "%");
        break;
    case Effect::Eq:
        heading("A", "Graphic Equalizer", enabled(c.eq_enabled.load()));
        detail(out, "Bands", c.eq_band_count.load());
        if (eqBand >= 1 && eqBand <= c.eq_band_count.load() && eqBand <= 31)
            detail(out, "Selected band gain", c.eq_levels_hundredths[eqBand - 1].load() / 100.0, " dB");
        detail(out, "Applied/requested", (std::to_string(c.eq_applied_serial.load()) + "/" +
                std::to_string(c.eq_request_serial.load())).c_str());
        if (expanded) {
            detail(out, "Selected preset", info.selectedPreset.empty() ? "custom" : info.selectedPreset.c_str());
            const int count = viper_eq::validBands(c.eq_band_count.load());
            const auto& hz = viper_eq::frequencies(count);
            for (int i = 0; i < count; ++i)
                out << "      Band " << i+1 << " (" << hz[i] << " Hz): "
                    << c.eq_levels_hundredths[i].load()/100.0 << " dB\n";
        }
        break;
    case Effect::Deq:
        heading("B", "Dynamic EQ", enabled(c.deq_enabled.load()));
        detail(out, "Active bands", c.deq_band_count.load());
        if (expanded) {
            for (int i = 0; i < std::clamp(c.deq_band_count.load(), 0, 10); ++i) {
                const auto& b = c.deq_bands[i];
                out << "      Band " << i+1 << ": " << b.frequency_hz.load() << " Hz"
                    << ", Q " << b.q_hundredths.load()/100.0
                    << ", gain " << b.gain_hundredths.load()/100.0 << " dB"
                    << ", threshold " << b.threshold_tenths.load()/10.0 << " dB"
                    << ", attack " << b.attack_ms.load() << " ms"
                    << ", release " << b.release_ms.load() << " ms"
                    << ", filter type " << b.filter_type.load() << '\n';
            }
            detail(out, "Applied/requested", (std::to_string(c.deq_applied_serial.load()) + "/" +
                    std::to_string(c.deq_request_serial.load())).c_str());
        }
        break;
    case Effect::Clarity:
        heading("C", "ViPER Clarity", enabled(c.clarity_enabled.load()));
        detail(out, "Mode", clarityMode(c.clarity_mode.load()));
        detail(out, "Strength", c.clarity_gain_milli.load() / 1000.0);
        break;
    case Effect::Spectrum:
        heading("D", "Spectrum Extension", enabled(c.spectrum_enabled.load()));
        detail(out, "Reference frequency", c.spectrum_frequency_hz.load(), " Hz");
        detail(out, "Harmonic strength", c.spectrum_exciter_milli.load() / 1000.0);
        break;
    case Effect::Tube:
        heading("E", "Tube Simulator", enabled(c.tube_enabled.load()));
        detail(out, "Tube model", tubeModel(c.tube_model.load()));
        detail(out, "Drive", c.tube_drive_percent.load(), "%");
        detail(out, "Mix", c.tube_mix_percent.load(), "%");
        break;
    case Effect::Analogx:
        heading("F", "AnalogX", enabled(c.analogx_enabled.load()));
        detail(out, "Mode", strengthMode(c.analogx_mode.load()));
        break;
    case Effect::Crossfeed:
        heading("A", "Crossfeed (ViPER Cure)", enabled(c.crossfeed_enabled.load()));
        detail(out, "Strength", strengthMode(c.crossfeed_preset.load()));
        break;
    case Effect::Headphone:
        heading("B", "Headphone Surround+", enabled(c.headphone_enabled.load()));
        detail(out, "Surround level", c.headphone_quality.load() + 1);
        break;
    case Effect::Differential:
        heading("C", "Differential Surround", enabled(c.diff_enabled.load()));
        detail(out, "Delay", c.diff_delay_ms.load(), " ms");
        detail(out, "Reverse", enabled(c.diff_reverse.load()));
        detail(out, "Mix", c.diff_mix_percent.load(), "%");
        detail(out, "Low-pass", c.diff_lp_hz.load(), " Hz");
        break;
    case Effect::Field:
        heading("D", "Field Surround", enabled(c.field_enabled.load()));
        detail(out, "Widening", c.field_widen_tenths.load() / 10.0);
        detail(out, "Mid image", c.field_mid_hundredths.load() / 100.0);
        detail(out, "Depth", c.field_depth.load());
        break;
    case Effect::Reverb:
        heading("E", "Reverberation", enabled(c.reverb_enabled.load()));
        detail(out, "Room", c.reverb_room_percent.load(), "%");
        detail(out, "Width", c.reverb_width_percent.load(), "%");
        detail(out, "Damping", c.reverb_damp_percent.load(), "%");
        detail(out, "Wet", c.reverb_wet_percent.load(), "%");
        detail(out, "Dry", c.reverb_dry_percent.load(), "%");
        break;
    case Effect::Imager:
        heading("F", "Stereo Imager", enabled(c.imager_enabled.load()));
        detail(out, "Low width", c.imager_low_width_percent.load(), "%");
        detail(out, "Mid width", c.imager_mid_width_percent.load(), "%");
        detail(out, "High width", c.imager_high_width_percent.load(), "%");
        detail(out, "Low crossover", c.imager_low_crossover_hz.load(), " Hz");
        detail(out, "High crossover", c.imager_high_crossover_hz.load(), " Hz");
        break;
    case Effect::Fet:
        heading("A", "FET Compressor", enabled(c.fet_enabled.load()));
        detail(out, "Threshold", c.fet_threshold_db.load(), " dB");
        detail(out, "Ratio", c.fet_ratio_tenths.load() / 10.0);
        detail(out, "Knee", c.fet_knee_db.load(), " dB");
        detail(out, "Makeup", c.fet_makeup_db.load(), " dB");
        detail(out, "Attack", c.fet_attack_ms.load(), " ms");
        detail(out, "Release", c.fet_release_ms.load(), " ms");
        break;
    case Effect::Multiband:
        heading("B", "Multiband Compressor", enabled(c.mb_enabled.load()));
        detail(out, "Bands", c.mb_band_count.load());
        if (expanded) {
            const int count = c.mb_band_count.load() == 5 ? 5 : 3;
            for (int i = 0; i < count-1; ++i)
                out << "      Crossover " << i+1 << ": " << c.mb_crossovers_hz[i].load() << " Hz\n";
            for (int i = 0; i < count; ++i) {
                const auto& b = c.mb_bands[i];
                out << "      Band " << i+1 << ": " << enabled(b.enabled.load())
                    << ", threshold " << b.threshold_db.load() << " dB"
                    << ", ratio " << b.ratio_tenths.load()/10.0 << ":1"
                    << ", knee " << b.knee_db.load() << " dB"
                    << ", makeup " << b.makeup_db.load() << " dB"
                    << ", attack " << b.attack_ms.load() << " ms"
                    << ", release " << b.release_ms.load() << " ms\n";
            }
            detail(out, "Applied/requested", (std::to_string(c.mb_applied_serial.load()) + "/" +
                    std::to_string(c.mb_request_serial.load())).c_str());
        }
        break;
    case Effect::Ddc:
        heading("A", "ViPER-DDC", enabled(c.ddc_enabled.load()));
        detail(out, "Profile status", ddcStatus(c.ddc_status.load()));
        if (expanded) detail(out, "Selected profile", info.ddcProfile.empty() ? "(none)" : info.ddcProfile.c_str());
        break;
    case Effect::Speaker:
        heading("B", "Speaker Correction", enabled(c.speaker_enabled.load()));
        break;
    case Effect::Convolver:
        heading("C", "Convolver", enabled(c.convolver_enabled.load()));
        detail(out, "IR status", irStatus(c.convolver_status.load()));
        detail(out, "Cross-channel mix", c.convolver_cross_percent.load(), "%");
        if (expanded) detail(out, "Selected kernel", info.convolverKernel.empty() ? "(none)" : info.convolverKernel.c_str());
        break;
    case Effect::Unknown:
        out << "Unknown CLI effect group\n";
        break;
    }
}

inline void section(std::ostream& out, const Controls& c, const char* name,
                    std::initializer_list<Effect> effects, bool expanded = false,
                    const FullDetails& info = {}) {
    out << '\n' << name << '\n' << std::string(58, '-') << '\n';
    for (Effect e : effects) effect(out, c, e, true, -1, expanded, info);
}
} // namespace cli_report

inline void printCliReport(std::ostream& out, const Controls& c,
                           const std::string& cmd, int eqBand = -1,
                           bool expanded = false, const cli_report::FullDetails& info = {}) {
    using namespace cli_report;
    // Only an explicit status query prints the complete six-tab report.
    // The master switch and its legacy aliases confirm the new state briefly.
    if (cmd == "on" || cmd == "off" || cmd == "all-on" || cmd == "all-off") {
        effect(out, c, Effect::Master, false);
        return;
    }
    if (cmd != "status") {
        effect(out, c, fromCommand(cmd), false, eqBand);
        return;
    }
    out << "ViPER4Linux-RE - DSP status\n";
    out << "============================================\n";
    effect(out, c, Effect::Master, true);
    section(out, c, "1. Output", {Effect::Limiter, Effect::Agc, Effect::Lufs}, expanded, info);
    section(out, c, "2. Bass", {Effect::Bass, Effect::Mono, Effect::Psycho, Effect::Dynamic}, expanded, info);
    section(out, c, "3. EQ & Tone", {Effect::Eq, Effect::Deq, Effect::Clarity,
                                     Effect::Spectrum, Effect::Tube, Effect::Analogx}, expanded, info);
    section(out, c, "4. Spatial", {Effect::Crossfeed, Effect::Headphone,
                                   Effect::Differential, Effect::Field, Effect::Reverb,
                                   Effect::Imager}, expanded, info);
    section(out, c, "5. Dynamics", {Effect::Fet, Effect::Multiband}, expanded, info);
    section(out, c, "6. Sound Correction", {Effect::Ddc, Effect::Speaker,
                                            Effect::Convolver}, expanded, info);
    out << "\nBackend\n" << std::string(58, '-') << '\n';
    out << "  Processing rate: " << c.sample_rate_hz.load() << " Hz\n"
        << "  Backend PID:     " << c.owner_pid.load() << '\n'
        << "  Audio blocks:    " << c.audio_callbacks.load() << '\n';
}
} // namespace viperbass
