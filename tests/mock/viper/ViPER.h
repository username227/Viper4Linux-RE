#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <optional>
namespace viper {
struct MasterLimiterParams { float threshold; float output_volume; float channel_pan; };
struct PlaybackGainControlParams { bool enable; float strength; float max_gain; float output_threshold; };
struct LufsParams { bool enable; float target; float max_gain; int speed; };
struct BassParams { bool enable; int mode; int frequency; float gain; bool anti_pop; };
struct BassMonoParams { bool enable; int mode; unsigned frequency; float gain; bool anti_pop; };
struct PsychoacousticBassParams { bool enable; unsigned cutoff; float intensity; unsigned harmonic_order; float original_level; };
struct DynamicSystemParams { bool enable; int x_coeff_low; int x_coeff_high; int y_coeff_low; int y_coeff_high; float side_gain_low; float side_gain_high; float strength; };
struct ClarityParams { bool enable; int mode; float gain; };
struct SpectrumExtensionParams { bool enable; int strength; float exciter; };
struct TubeSimulatorParams { bool enable=false; int model=0; float drive=0.5f; float mix=0.3f; };
struct AnalogXParams { bool enable=false; int mode=0; };
struct CureParams { bool enable=false; int crossfeed_preset=0; };
struct FetCompressorParams {
    bool enable=false; float threshold=0, ratio=0, knee=0, gain=0;
    bool knee_auto=false, gain_auto=false, attack_auto=false, release_auto=false;
    float attack=0.01f, release=0.1f, knee_multi=2.f, max_attack=0.08f,
          max_release=0.5f, crest=0.2f, adapt=2.5f;
    bool no_clip=true;
};
struct StereoImagerParams { bool enable=false; float low_width=1.f, mid_width=1.f, high_width=1.f;
                            float low_crossover=200.f, high_crossover=4000.f; };
struct DynamicEqBandParams {
    float frequency=0, q=0, gain=0, threshold=0, attack=0, release=0;
    int filter_type=0;
};
struct DynamicEqParams {
    bool enable=false; uint32_t band_count=0;
    std::array<DynamicEqBandParams,10> bands{};
};
struct HeadphoneSurroundParams { bool enable; int quality; };
struct EqualizerParams { bool enable=false; uint32_t band_count=0; std::array<float, 31> band_levels{}; };
struct ConvolverParams { bool enable; float cross_channel; };
struct ReverbParams { bool enable; float room_size; float width; float damp; float wet; float dry; };
struct FieldSurroundParams { bool enable; float widening; float mid_image; short depth; };
struct DiffSurroundParams { bool enable; float delay; bool reverse; float wet_dry_mix; float lp_cutoff; };
}
class ViPER {public:
void SetSamplingRate(uint32_t);
void ResetAllEffects();
void ResetBuffers();
void ApplyMasterLimiter(const viper::MasterLimiterParams&);
void ApplyPlaybackGainControl(const viper::PlaybackGainControlParams&);
void ApplyLufs(const viper::LufsParams&);
void ApplyBass(const viper::BassParams&);
void ApplyBassMono(const viper::BassMonoParams&);
void ApplyPsychoacousticBass(const viper::PsychoacousticBassParams&);
void ApplyDynamicSystem(const viper::DynamicSystemParams&);
void ApplyClarity(const viper::ClarityParams&);
void ApplySpectrumExtension(const viper::SpectrumExtensionParams&);
void ApplyTubeSimulator(const viper::TubeSimulatorParams&);
void ApplyAnalogX(const viper::AnalogXParams&);
void ApplyCure(const viper::CureParams&);
void ApplyStereoImager(const viper::StereoImagerParams&);
void ApplyFetCompressor(const viper::FetCompressorParams&);
void ApplyDynamicEq(const viper::DynamicEqParams&);
void ApplyHeadphoneSurround(const viper::HeadphoneSurroundParams&);
void ApplyDiffSurround(const viper::DiffSurroundParams&);
void ApplyFieldSurround(const viper::FieldSurroundParams&);
void ApplyReverb(const viper::ReverbParams&);
void ApplyConvolver(const viper::ConvolverParams&);
void ApplyEqualizer(const viper::EqualizerParams&);
std::optional<uint32_t> LoadConvolverKernel(const float*, uint32_t, uint32_t, uint32_t);
void UnloadConvolverKernel();
void Process(std::vector<float>&, uint32_t);
};
