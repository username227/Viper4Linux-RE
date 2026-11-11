// Real ViPERDSP regression guard for the FET floating-point sidechain.
// These 25 Linux functional cases are NOT Android numerical parity captures.
#include "viper/effects/FETCompressor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
constexpr unsigned kRate = 48000;
constexpr std::size_t kFrames = 131072;
constexpr std::size_t kBlock = 512;
constexpr float kDbToLn = 0.11512925464970229f;
constexpr double kPi = 3.14159265358979323846;
struct Case {
    const char* name;
    int threshold_db = -18, ratio_tenths = 20, knee_db = 6;
    int makeup_db = 0, attack_ms = 10, release_ms = 100;
};
constexpr std::array<Case, 25> kCases{{
    {"default"},
    {"threshold_-48", -48}, {"threshold_-36", -36},
    {"threshold_-6", -6}, {"threshold_0", 0},
    {"ratio_1", -18, 10}, {"ratio_1.5", -18, 15},
    {"ratio_4", -18, 40}, {"ratio_10", -18, 100},
    {"knee_0", -18, 20, 0}, {"knee_3", -18, 20, 3},
    {"knee_12", -18, 20, 12},
    {"makeup_6", -18, 20, 6, 6},
    {"makeup_12", -18, 20, 6, 12},
    {"makeup_24", -18, 20, 6, 24},
    {"attack_1", -18, 20, 6, 0, 1},
    {"attack_5", -18, 20, 6, 0, 5},
    {"attack_50", -18, 20, 6, 0, 50},
    {"attack_100", -18, 20, 6, 0, 100},
    {"release_5", -18, 20, 6, 0, 10, 5},
    {"release_20", -18, 20, 6, 0, 10, 20},
    {"release_300", -18, 20, 6, 0, 10, 300},
    {"release_500", -18, 20, 6, 0, 10, 500},
    {"corner_heavy", -48, 100, 12, 12, 1, 500},
    {"corner_light", -6, 10, 0, 0, 100, 5},
}};
std::vector<float> input_samples() {
    std::vector<float> samples(kFrames * 2);
    for (std::size_t i = 0; i < kFrames; ++i) {
        const double time = static_cast<double>(i) / kRate;
        const double envelope = 0.055 + 0.195 *
            (0.5 + 0.5 * std::sin(2 * kPi * 0.73 * time));
        samples[2 * i] = static_cast<float>(
            envelope * std::sin(2 * kPi * 211 * time));
        samples[2 * i + 1] = static_cast<float>(
            0.88 * envelope * std::sin(2 * kPi * 317 * time + 0.31));
    }
    return samples;
}
std::vector<float> process(const std::vector<float>& input, const Case& p) {
    FETCompressor fet;
    fet.SetSamplingRate(kRate);
    fet.SetEnable(true);
    fet.SetThreshold(static_cast<float>(p.threshold_db) * kDbToLn);
    fet.SetRatio(-(1.0f - 10.0f / static_cast<float>(p.ratio_tenths)));
    fet.SetKnee(static_cast<float>(p.knee_db) * kDbToLn);
    fet.SetKneeAuto(false);
    fet.SetGain(static_cast<float>(p.makeup_db) * kDbToLn);
    fet.SetGainAuto(false);
    fet.SetAttack(static_cast<float>(p.attack_ms) / 1000.0f);
    fet.SetAttackAuto(false);
    fet.SetRelease(static_cast<float>(p.release_ms) / 1000.0f);
    fet.SetReleaseAuto(false);
    fet.SetKneeMulti(2.0f);
    fet.SetMaxAttack(0.080f);
    fet.SetMaxRelease(0.5f);
    fet.SetCrest(0.2f);
    fet.SetAdapt(2.5f);
    fet.SetNoClip(true);
    fet.Reset();
    auto result = input;
    for (std::size_t offset = 0; offset < kFrames; offset += kBlock)
        fet.Process(result.data() + 2 * offset, static_cast<unsigned>(kBlock));
    return result;
}
double rms_delta(const std::vector<float>& a, const std::vector<float>& b) {
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double delta = static_cast<double>(a[i]) - b[i];
        sum += delta * delta;
    }
    return std::sqrt(sum / a.size());
}
bool finite_samples(const std::vector<float>& samples) {
    return std::all_of(samples.begin(), samples.end(),
                       [](float value) { return std::isfinite(value); });
}
} // namespace

int main() {
    const auto input = input_samples();
    std::vector<std::vector<float>> outputs;
    outputs.reserve(kCases.size());
    int passed = 0;
    for (std::size_t i = 0; i < kCases.size(); ++i) {
        const auto& p = kCases[i];
        auto actual = process(input, p);
        const double effect = rms_delta(input, actual);
        bool ok = finite_samples(actual);
        // Regression: abs(float) -> abs(int) caused zero gain reduction
        // across every no-makeup scenario in the original Linux build.
        const bool should_compress = p.threshold_db <= -18 &&
            p.ratio_tenths > 10 && p.makeup_db == 0;
        if (should_compress) ok &= effect > 1e-5;
        if (p.ratio_tenths == 10 && p.makeup_db == 0) ok &= effect < 1e-6;
        if (p.threshold_db >= -6 && p.makeup_db == 0) ok &= effect < 1e-6;
        if (i > 0 && p.threshold_db == -18 && p.ratio_tenths > 10 &&
            p.makeup_db == 0) {
            const double vs_default = rms_delta(actual, outputs.front());
            ok &= vs_default > 1e-7;
        }
        std::printf("%s FET %-15s ON/OFF RMS %.10f\n",
                    ok ? "PASS" : "FAIL", p.name, effect);
        if (ok) ++passed;
        outputs.push_back(std::move(actual));
    }
    std::printf("FET native 25-case functional sweep: %d/%zu passed\n",
                passed, kCases.size());
    return passed == static_cast<int>(kCases.size()) ? 0 : 1;
}
