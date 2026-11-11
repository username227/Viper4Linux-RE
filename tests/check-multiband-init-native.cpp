// Native ViPERDSP regression: selected thresholds must be primed before the
// Multiband reset; live edits must not introduce a new disable/enable reset.
// This exercises the same production helper as the LADSPA control worker.
#include "viper-multiband-init.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
constexpr std::uint32_t kRate = 48000;
constexpr std::uint32_t kBlock = 512;
constexpr float kLn10Over20 = 0.11512925464970229f;
constexpr double kPi = 3.14159265358979323846;

void require(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}

viper::MultibandCompressorParams preset(std::uint32_t count) {
    viper::MultibandCompressorParams p{};
    p.enable = true;
    p.band_count = count;
    if (count == 3) {
        p.crossover_frequencies[0] = 200;
        p.crossover_frequencies[1] = 4000;
    } else {
        p.crossover_frequencies[0] = 120;
        p.crossover_frequencies[1] = 500;
        p.crossover_frequencies[2] = 4000;
        p.crossover_frequencies[3] = 8000;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        auto& b = p.bands[i];
        b.enable = true;
        b.threshold = (i == 4 ? -42.f : (i == 3 ? -30.f : -36.f)) * kLn10Over20;
        b.ratio = -0.75f;
        b.knee = 6.f * kLn10Over20;
        b.knee_auto = false;
        b.gain = 0.f;
        b.gain_auto = false;
        b.attack = 0.01f;
        b.attack_auto = false;
        b.release = 0.1f;
        b.release_auto = false;
        b.knee_multi = 2.f;
        b.max_attack = 0.080f;
        b.max_release = 0.5f;
        b.crest = 0.2f;
        b.adapt = 2.5f;
        b.no_clip = true;
    }
    return p;
}

void init(ViPER& v) {
    v.SetSamplingRate(kRate);
    v.ResetAllEffects();
    viper::MasterLimiterParams lim{};
    lim.threshold = 0.5f;
    lim.output_volume = 1.f;
    lim.channel_pan = 0.f;
    v.ApplyMasterLimiter(lim);
}

std::vector<float> input(unsigned block) {
    std::vector<float> data(2 * kBlock);
    for (std::uint32_t i = 0; i < kBlock; ++i) {
        const double t = (static_cast<double>(block) * kBlock + i) / kRate;
        const double low = std::sin(2 * kPi * 97 * t);
        const double mid = std::sin(2 * kPi * 880 * t);
        const double high = std::sin(2 * kPi * 9113 * t);
        data[2 * i] = static_cast<float>(0.15 * low + 0.13 * mid + 0.12 * high);
        data[2 * i + 1] = static_cast<float>(0.14 * low - 0.12 * mid + 0.11 * high);
    }
    return data;
}

double pcm_error(ViPER& a, ViPER& b, unsigned begin, unsigned blocks) {
    double diff = 0.0;
    for (unsigned n = begin; n < begin + blocks; ++n) {
        auto x = input(n), y = x;
        a.Process(x, kBlock);
        b.Process(y, kBlock);
        for (std::size_t j = 0; j < x.size(); ++j) {
            require(std::isfinite(x[j]) && std::isfinite(y[j]), "nonfinite native audio");
            diff = std::max(diff, static_cast<double>(std::abs(x[j] - y[j])));
        }
    }
    return diff;
}

void check(std::uint32_t count) {
    auto p = preset(count);
    ViPER actual, reference;
    init(actual); init(reference);
    // Production code under test: previous disabled/three-band defaults.
    viper_multiband::apply(actual, p, false, 3);
    // Independent intended native setup (complete parameters, then enable).
    auto disabled = p;
    disabled.enable = false;
    reference.ApplyMultibandCompressor(disabled);
    reference.ApplyMultibandCompressor(p);
    require(pcm_error(actual, reference, 0, 22) < 1e-7, "fresh-start initialization mismatch");
    // Unlike the matched-state check, these should produce a measurable
    // startup difference on the frequency-rich signal.
    ViPER corrected, old;
    init(corrected); init(old);
    viper_multiband::apply(corrected, p, false, 3);
    old.ApplyMultibandCompressor(p);
    require(pcm_error(corrected, old, 0, 12) > 1e-6, "startup stimulus did not exercise threshold state");

    // While enabled, a threshold edit must be equivalent to ONE ordinary
    // native update, with no new reset.
    auto edited = p;
    edited.bands[count - 1].threshold = -24.f * kLn10Over20;
    viper_multiband::apply(actual, edited, true, count);
    reference.ApplyMultibandCompressor(edited);
    require(pcm_error(actual, reference, 22, 16) < 1e-7, "live edit changed compressor state");

    // Switching 3<->5 under playback must pre-stage the new band's controls.
    const auto other_count = count == 3 ? 5u : 3u;
    auto other = preset(other_count);
    viper_multiband::apply(actual, other, true, count);
    auto staged = other;
    staged.enable = false;
    reference.ApplyMultibandCompressor(staged);
    reference.ApplyMultibandCompressor(other);
    require(pcm_error(actual, reference, 38, 16) < 1e-7, "topology switch mismatch");

    // A disabled->enabled transition must reseed selected current values.
    staged.enable = false;
    viper_multiband::apply(actual, staged, true, other_count);
    reference.ApplyMultibandCompressor(staged);
    viper_multiband::apply(actual, other, false, other_count);
    reference.ApplyMultibandCompressor(staged);
    reference.ApplyMultibandCompressor(other);
    require(pcm_error(actual, reference, 54, 16) < 1e-7, "re-enable mismatch");

    // No wall-clock timing promise from CI; report the native apply cost so
    // the maintainer can compare it on the actual hardware before installation.
    const auto begin = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < 100; ++i) {
        viper_multiband::apply(actual, other, false, other_count);
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    std::printf("PASS %u-band initialization, live edit, 3/5 switch, re-enable; "
                "mean fresh-enable apply %.4f ms (100 calls)\n", count, elapsed / 100.0);
}
} // namespace

int main() {
    check(3);
    check(5);
    return 0;
}
