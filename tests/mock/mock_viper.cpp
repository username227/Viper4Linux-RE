#include "viper/ViPER.h"
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>
namespace {
    std::unordered_map<const ViPER*, viper::MasterLimiterParams> limiter;
    std::unordered_map<const ViPER*, viper::PlaybackGainControlParams> agc;
    std::unordered_map<const ViPER*, viper::LufsParams> lufs;
    std::unordered_map<const ViPER*, viper::BassParams> bass;
    std::unordered_map<const ViPER*, viper::BassMonoParams> mono;
    std::unordered_map<const ViPER*, viper::PsychoacousticBassParams> psycho;
    std::unordered_map<const ViPER*, viper::DynamicSystemParams> dynamic;
    std::unordered_map<const ViPER*, viper::ClarityParams> clarity;
    std::unordered_map<const ViPER*, viper::SpectrumExtensionParams> spectrum;
    std::unordered_map<const ViPER*, viper::TubeSimulatorParams> tube;
    std::unordered_map<const ViPER*, viper::AnalogXParams> analogx;
    std::unordered_map<const ViPER*, viper::StereoImagerParams> imager;
    std::unordered_map<const ViPER*, viper::FetCompressorParams> fet;
    std::unordered_map<const ViPER*, viper::MultibandCompressorParams> multiband;
    std::unordered_map<const ViPER*, viper::DynamicEqParams> deq;
    std::unordered_map<const ViPER*, viper::HeadphoneSurroundParams> headphone;
    std::unordered_map<const ViPER*, viper::DiffSurroundParams> diff;
    std::unordered_map<const ViPER*, viper::FieldSurroundParams> field;
    std::unordered_map<const ViPER*, viper::ReverbParams> reverb;
    std::unordered_map<const ViPER*, viper::ConvolverParams> convolver;
    std::unordered_map<const ViPER*, viper::EqualizerParams> eq;
}
void ViPER::SetSamplingRate(uint32_t) {}
void ViPER::ResetAllEffects() {}
void ViPER::ResetBuffers() {}
void ViPER::ApplyMasterLimiter(const viper::MasterLimiterParams& p) { limiter[this] = p; }
void ViPER::ApplyPlaybackGainControl(const viper::PlaybackGainControlParams& p) { agc[this] = p; }
void ViPER::ApplyLufs(const viper::LufsParams& p) { lufs[this] = p; }
void ViPER::ApplyBass(const viper::BassParams& p) { bass[this] = p; }
void ViPER::ApplyBassMono(const viper::BassMonoParams& p) { mono[this] = p; }
void ViPER::ApplyPsychoacousticBass(const viper::PsychoacousticBassParams& p) { psycho[this] = p; }
void ViPER::ApplyDynamicSystem(const viper::DynamicSystemParams& p) { dynamic[this] = p; }
void ViPER::ApplyClarity(const viper::ClarityParams& p) { clarity[this] = p; }
void ViPER::ApplySpectrumExtension(const viper::SpectrumExtensionParams& p) { spectrum[this] = p; }
void ViPER::ApplyTubeSimulator(const viper::TubeSimulatorParams& p) { tube[this] = p; }
void ViPER::ApplyAnalogX(const viper::AnalogXParams& p) { analogx[this] = p; }
static std::unordered_map<const ViPER*, viper::CureParams> crossfeed;
void ViPER::ApplyCure(const viper::CureParams& p) { crossfeed[this] = p; }
void ViPER::ApplyStereoImager(const viper::StereoImagerParams& p) { imager[this] = p; }
void ViPER::ApplyFetCompressor(const viper::FetCompressorParams& p) { fet[this] = p; }
void ViPER::ApplyMultibandCompressor(const viper::MultibandCompressorParams& p) { multiband[this] = p; }
void ViPER::ApplyDynamicEq(const viper::DynamicEqParams& p) { deq[this] = p; }
void ViPER::ApplyHeadphoneSurround(const viper::HeadphoneSurroundParams& p) { headphone[this] = p; }
void ViPER::ApplyDiffSurround(const viper::DiffSurroundParams& p) { diff[this] = p; }
void ViPER::ApplyFieldSurround(const viper::FieldSurroundParams& p) { field[this] = p; }
void ViPER::ApplyReverb(const viper::ReverbParams& p) { reverb[this] = p; }
void ViPER::ApplyConvolver(const viper::ConvolverParams& p) { convolver[this] = p; }
void ViPER::ApplySpeakerCorrection(const viper::SpeakerCorrectionParams&) {}
void ViPER::ApplyDdc(const viper::DdcParams&) {}
void ViPER::LoadDdcCoefficients(const viper::BiquadSection*, const viper::BiquadSection*, unsigned int) {}
void ViPER::ApplyEqualizer(const viper::EqualizerParams& p) { eq[this] = p; }
std::optional<uint32_t> ViPER::LoadConvolverKernel(const float* samples, uint32_t frames, uint32_t channels, uint32_t id) {
    if (id >= 2) std::this_thread::sleep_for(std::chrono::milliseconds(95)); // Simulate expensive FFT kernel rebuild.
    return samples && frames >= 16 && (channels == 1 || channels == 2) ? std::optional<uint32_t>{id} : std::nullopt;
}
void ViPER::UnloadConvolverKernel() { convolver[this].enable = false; }
void ViPER::Process(std::vector<float>& values, uint32_t frames) {
    // Deterministic transformations isolate control correctness; NOT real compressor code.
    const auto e = eq[this];
    if (e.enable) {
        // Deliberately fake response to make IPC changes observable by tests.
        // Real frequency-domain EQ behavior comes from upstream ViPERDSP.
        float factor = 1.0f;
        for (unsigned i = 0; i < e.band_count && i < 31; ++i) factor += e.band_levels[i] / 100.f;
        for (uint32_t i = 0; i < 2*frames; ++i) values[i] *= factor;
    }
    const auto cv = convolver[this];
    if (cv.enable) {
        const float factor = 1.0f + 0.2f + cv.cross_channel * 0.1f;
        for (uint32_t i=0; i<2*frames; ++i) values[i] *= factor;
    }
    const auto g = agc[this];
    if (g.enable) for (uint32_t i=0; i<2*frames; ++i) values[i] *= 1.1f;
    const auto u = lufs[this];
    if (u.enable) for (uint32_t i=0; i<2*frames; ++i) values[i] *= 1.05f;
    const auto rv = reverb[this];
    if (rv.enable) {
        // Synthetic deterministic factor, tests parameter forwarding only;
        // not a model of real reverberation or its tails.
        const float factor = 1.0f + 0.01f * rv.room_size + 0.02f * rv.width +
            0.03f * rv.damp + 0.04f * rv.wet + 0.05f * rv.dry;
        for (uint32_t i = 0; i < 2 * frames; ++i) values[i] *= factor;
    }
    const auto f = field[this];
    if (f.enable) {
        // Deliberately synthetic factor. Tests IPC forwarding, not upstream's sound.
        const float factor = 1.0f + 0.01f * f.widening +
            0.04f * (f.mid_image - 1.0f) + 0.02f * f.depth / 950.0f;
        for (uint32_t i = 0; i < 2 * frames; ++i) values[i] *= factor;
    }
    const auto d = diff[this];
    if (d.enable) {
        // Deliberately fake, deterministic response for testing parameter forwarding.
        const float factor = 1.0f + 0.01f * d.delay + 0.2f * d.wet_dry_mix +
                             (d.reverse ? 0.05f : 0.0f) + 0.05f * d.lp_cutoff / 20000.0f;
        for (uint32_t i = 0; i < 2 * frames; ++i) values[i] *= factor;
    }
    const auto s = spectrum[this];
    if (s.enable) {
        const float factor = 1.0f + s.exciter * (0.03f + 0.015f * (s.strength - 2200) / 6000.0f);
        for (uint32_t i = 0; i < 2 * frames; ++i) values[i] *= factor;
    }
    const auto tb = tube[this];
    if (tb.enable) {
        // Synthetic output multiplier for control-path validation only.
        const float factor = 1.f + 0.12f * tb.drive * tb.mix * (1.f + 0.2f * tb.model);
        for (uint32_t i = 0; i < 2*frames; ++i) values[i] *= factor;
    }
    const auto cf = crossfeed[this];
    if (cf.enable) {
        const float factor = 1.03f + cf.crossfeed_preset * .01f; // mock only, not ViPERDSP
        for (uint32_t i=0;i<2*frames;++i) values[i] *= factor;
    }
    const auto mb = multiband[this];
    if (mb.enable) {
        // Synthetic control-path factor, not a compressor algorithm.
        float factor = 1.f + .000001f * mb.crossover_frequencies[0];
        for (std::uint32_t i=0; i<mb.band_count && i<5; ++i) {
            const auto& b = mb.bands[i];
            if (b.enable) factor += .01f*b.ratio + .001f*b.threshold + .001f*b.attack;
        }
        for (uint32_t i=0; i<2*frames; ++i) values[i] *= factor;
    }
    const auto fc = fet[this];
    if (fc.enable) {
        // Synthetic ONLY: verifies native dispatch and parameter forwarding.
        const float factor = 1.f + .03f*fc.ratio + .005f*fc.threshold +
            .002f*fc.knee + .004f*fc.gain + .01f*fc.attack + .01f*fc.release;
        for (uint32_t i=0; i<2*frames; ++i) values[i] *= factor;
    }
    const auto si = imager[this];
    if (si.enable) {
        // Mock ONLY: param-dependent factor to test worker/IPC delivery.
        const float factor = 1.f + .01f*(si.low_width - 1.f) +
            .02f*(si.mid_width - 1.f) + .03f*(si.high_width - 1.f) +
            .00001f*(si.low_crossover - 200.f) +
            .000001f*(si.high_crossover - 4000.f);
        for (uint32_t i=0; i<2*frames; ++i) values[i] *= factor;
    }
    const auto dq = deq[this];
    if (dq.enable) {
        // Synthetic deterministic control-path check; actual DynamicEQ
        // DSP and time constants remain strictly upstream ViPERDSP.
        float factor=1.f;
        for(uint32_t j=0; j<dq.band_count && j<10; ++j) {
            const auto& b = dq.bands[j];
            factor += .003f * b.gain + .0001f * b.q
                + .000001f * b.frequency + .0001f * b.threshold;
        }
        for (uint32_t i=0; i<2*frames; ++i) values[i] *= factor;
    }
    const auto ax = analogx[this];
    if (ax.enable) {
        // Synthetic multiplier only: verifies native method dispatch & mode.
        const float factor = 1.f + .03f * (ax.mode + 1);
        for (uint32_t i=0; i<2*frames; ++i) values[i] *= factor;
    }
    const auto h = headphone[this];
    if (h.enable) {
        const float factor = 1.0f + 0.08f * (h.quality + 1);
        for (uint32_t i = 0; i < 2 * frames; ++i) values[i] *= factor;
    }
    // Mock factors deliberately synthetic; test forwarding, not real DSP quality.
    const auto bm = mono[this];
    if (bm.enable) {
        const float factor = 1.f + 0.02f*bm.mode + 0.005f*bm.gain + 0.001f*bm.frequency;
        for (uint32_t i=0; i<2*frames; ++i) values[i] *= factor;
    }
    const auto ps = psycho[this];
    if (ps.enable) {
        const float factor = 1.f + 0.01f*ps.intensity + 0.002f*ps.harmonic_order + 0.001f*ps.cutoff/100.f + 0.003f*ps.original_level;
        for (uint32_t i=0; i<2*frames; ++i) values[i] *= factor;
    }
    const auto dy = dynamic[this];
    if (dy.enable) {
        const float factor = 1.f + 0.002f*dy.x_coeff_low/100.f + 0.001f*dy.x_coeff_high/1000.f
                          + 0.001f*dy.y_coeff_low/100.f + 0.001f*dy.y_coeff_high/100.f
                          + 0.002f*dy.side_gain_low + 0.002f*dy.side_gain_high + 0.08f*dy.strength;
        for (uint32_t i=0; i<2*frames; ++i) values[i] *= factor;
    }
    const auto b = bass[this];
    if (b.enable) {
        // Test-double output differs for each supported mode; this is NOT a model
        // of real ViPERDSP sound, only a way to verify mode forwarding.
        const float factor = 1.4f + 0.1f * b.mode;
        for (uint32_t i = 0; i < 2 * frames; ++i) values[i] *= factor;
    }
    const auto c = clarity[this];
    if (c.enable) {
        const float factor = 1.0f + 0.05f * c.gain + 0.025f * c.mode;
        for (uint32_t i = 0; i < 2 * frames; ++i) values[i] *= factor;
    }
    // Test-double output stage: simple bounded approximation, not a model
    // of upstream's lookahead limiter. We test forwarding and positioning.
    const auto m = limiter[this];
    if (m.output_volume > 0.f) {
        const float left = (m.channel_pan > 0.f ? 1.f-m.channel_pan : 1.f);
        const float right = (m.channel_pan < 0.f ? 1.f+m.channel_pan : 1.f);
        for (uint32_t i=0; i<frames; ++i) {
            values[2*i] = std::clamp(values[2*i]*m.output_volume*left, -m.threshold, m.threshold);
            values[2*i+1] = std::clamp(values[2*i+1]*m.output_volume*right, -m.threshold, m.threshold);
        }
    }
}
