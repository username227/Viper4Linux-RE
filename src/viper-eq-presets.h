#pragma once
// Exact built-in graphic EQ curves from likelikeslike/ViPER4Android:
// app/src/main/java/com/llsl/viper4android/effect/BuiltinPresets.kt
// 10/15/25/31 each have their OWN upstream curve; no synthesized interpolation
// for built-in presets. Native ViPERDSP receives values in hundredths of dB.
#include <array>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <utility>

namespace viper_eq {
using Frequencies = std::array<float, 31>;
struct NamedCurve {
    const char* name;
    std::array<int, 10> ten;
    std::array<int, 15> fifteen;
    std::array<int, 25> twentyfive;
    std::array<int, 31> thirtyone;
    const std::vector<int> levels(int bands) const {
        switch (bands) {
        case 15: return {fifteen.begin(), fifteen.end()};
        case 25: return {twentyfive.begin(), twentyfive.end()};
        case 31: return {thirtyone.begin(), thirtyone.end()};
        default: return {ten.begin(), ten.end()};
        }
    }
};
inline int validBands(int n) { return (n == 10 || n == 15 || n == 25 || n == 31) ? n : 10; }
inline const Frequencies& frequencies(int bands) {
    static const Frequencies f10 = {31,62,125,250,500,1000,2000,4000,8000,16000};
    static const Frequencies f15 = {25,40,63,100,160,250,400,630,1000,1600,2500,4000,6300,10000,16000};
    static const Frequencies f25 = {20,31.5f,40,50,80,100,125,160,250,315,400,500,800,1000,1250,1600,2500,3150,4000,5000,8000,10000,12500,16000,20000};
    static const Frequencies f31 = {20,25,31.5f,40,50,63,80,100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000,12500,16000,20000};
    switch (bands) { case 15: return f15; case 25: return f25; case 31: return f31; default: return f10; }
}
inline const std::array<NamedCurve, 12>& builtIns() {
    static const std::array<NamedCurve, 12> presets{{
        {"Acoustic",
          {450, 450, 350, 120, 100, 50, 140, 175, 350, 250},
          {450, 450, 450, 400, 250, 100, 100, 100, 50, 100, 150, 200, 300, 300, 250},
          {450, 450, 450, 450, 400, 400, 350, 250, 100, 100, 100, 100, 50, 50, 100, 100, 150, 150, 200, 250, 350, 300, 300, 250, 250},
          {450, 450, 450, 450, 450, 450, 400, 400, 350, 250, 200, 100, 100, 100, 100, 100, 50, 50, 100, 100, 150, 150, 150, 200, 250, 300, 350, 300, 300, 250, 250},
        },
        {"Bass Booster",
          {600, 400, 200, 0, 0, 0, 0, 0, 0, 0},
          {600, 550, 400, 250, 150, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {600, 600, 550, 450, 350, 250, 200, 150, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {600, 600, 600, 550, 450, 400, 350, 250, 200, 150, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        },
        {"Bass Reducer",
          {-600, -400, -200, 0, 0, 0, 0, 0, 0, 0},
          {-600, -550, -400, -250, -150, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {-600, -600, -550, -450, -350, -250, -200, -150, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {-600, -600, -600, -550, -450, -400, -350, -250, -200, -150, -50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        },
        {"Classical",
          {0, 0, 0, 0, 0, 0, -300, -300, -300, -500},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, -200, -300, -300, -300, -350, -500},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -100, -200, -300, -300, -300, -300, -300, -350, -450, -500, -500},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -100, -200, -300, -300, -300, -300, -300, -300, -300, -350, -450, -500, -500},
        },
        {"Deep",
          {300, 200, 100, 50, 50, 0, -100, -200, -300, -350},
          {300, 250, 200, 150, 100, 50, 50, 50, 0, -50, -150, -200, -250, -300, -350},
          {300, 300, 250, 250, 150, 150, 100, 100, 50, 50, 50, 50, 0, 0, -50, -50, -150, -150, -200, -250, -300, -300, -350, -350, -350},
          {300, 300, 300, 250, 250, 200, 150, 150, 100, 100, 50, 50, 50, 50, 50, 50, 0, 0, -50, -50, -100, -150, -150, -200, -250, -250, -300, -300, -350, -350, -350},
        },
        {"Flat",
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        },
        {"R&B",
          {300, 600, 400, 100, -100, -50, 100, 150, 250, 300},
          {300, 400, 600, 450, 300, 100, -50, -100, -50, 50, 100, 150, 200, 250, 300},
          {300, 300, 400, 500, 550, 450, 400, 300, 100, 50, -50, -100, -50, -50, 0, 50, 100, 150, 150, 200, 250, 250, 300, 300, 300},
          {300, 300, 300, 400, 500, 600, 550, 450, 400, 300, 200, 100, 50, -50, -100, -100, -50, -50, 0, 50, 100, 100, 150, 150, 200, 200, 250, 250, 300, 300, 300},
        },
        {"Rock",
          {400, 300, 100, 0, -50, 0, 150, 250, 350, 400},
          {400, 350, 300, 150, 50, 0, -50, -50, 0, 100, 200, 250, 300, 350, 400},
          {400, 400, 350, 350, 250, 150, 100, 50, 0, 0, -50, -50, 0, 0, 50, 100, 200, 200, 250, 300, 350, 350, 400, 400, 400},
          {400, 400, 400, 350, 350, 300, 250, 150, 100, 50, 50, 0, 0, -50, -50, -50, 0, 0, 50, 100, 150, 200, 200, 250, 300, 300, 350, 350, 400, 400, 400},
        },
        {"Small Speakers",
          {300, 200, 150, 100, 50, -50, -150, -200, -300, -350},
          {300, 250, 200, 150, 150, 100, 50, 0, -50, -100, -150, -200, -250, -300, -350},
          {300, 300, 250, 250, 200, 150, 150, 150, 100, 100, 50, 50, 0, -50, -100, -100, -150, -200, -200, -250, -300, -300, -350, -350, -350},
          {300, 300, 300, 250, 250, 200, 200, 150, 150, 150, 100, 100, 100, 50, 50, 0, 0, -50, -100, -100, -150, -150, -200, -200, -250, -250, -300, -300, -350, -350, -350},
        },
        {"Treble Booster",
          {0, 0, 0, 0, 0, 100, 200, 300, 400, 500},
          {0, 0, 0, 0, 0, 0, 0, 50, 100, 150, 250, 300, 350, 450, 500},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 100, 150, 150, 250, 250, 300, 350, 400, 450, 450, 500, 500},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 50, 100, 150, 150, 200, 250, 250, 300, 350, 350, 400, 450, 450, 500, 500},
        },
        {"Treble Reducer",
          {0, 0, 0, 0, 0, -100, -200, -300, -400, -500},
          {0, 0, 0, 0, 0, 0, 0, -50, -100, -150, -250, -300, -350, -450, -500},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -50, -100, -150, -150, -250, -250, -300, -350, -400, -450, -450, -500, -500},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -50, -50, -100, -150, -150, -200, -250, -250, -300, -350, -350, -400, -450, -450, -500, -500},
        },
        {"Vocal Booster",
          {-100, -50, 0, 150, 300, 300, 200, 100, 0, -100},
          {-100, -100, -50, 0, 50, 150, 250, 300, 300, 250, 150, 100, 50, -50, -100},
          {-100, -100, -100, -50, -50, 0, 0, 50, 150, 200, 250, 300, 300, 300, 250, 250, 150, 150, 100, 50, 0, -50, -50, -100, -100},
          {-100, -100, -100, -100, -50, -50, -50, 0, 0, 50, 100, 150, 200, 250, 300, 300, 300, 300, 250, 250, 200, 150, 150, 100, 50, 50, 0, -50, -50, -100, -100},
        },
    }};
    return presets;
}
// Resample only user-created presets when changing the active band count.
inline std::vector<int> interpolate(const std::vector<float>& srcHz,
                                    const std::vector<int>& srcLevels, int bands) {
    bands = validBands(bands);
    if (srcHz.empty() || srcHz.size() != srcLevels.size())
        return std::vector<int>(bands, 0);
    for (size_t i = 1; i < srcHz.size(); ++i) {
        if (!(srcHz[i] > srcHz[i-1])) return std::vector<int>(bands, 0);
    }
    std::vector<int> out;
    out.reserve(bands);
    const auto& dst = frequencies(bands);
    for (int i = 0; i < bands; ++i) {
        const float f = dst[i];
        if (f <= srcHz.front()) { out.push_back(std::clamp(srcLevels.front(), -1200, 1200)); continue; }
        if (f >= srcHz.back()) { out.push_back(std::clamp(srcLevels.back(), -1200, 1200)); continue; }
        auto upper = std::upper_bound(srcHz.begin(), srcHz.end(), f);
        const size_t j = static_cast<size_t>(upper - srcHz.begin());
        const float t = (std::log(f) - std::log(srcHz[j-1])) /
                        (std::log(srcHz[j]) - std::log(srcHz[j-1]));
        out.push_back(std::clamp(static_cast<int>(std::lround(
            srcLevels[j-1] + t * (srcLevels[j] - srcLevels[j-1]))), -1200, 1200));
    }
    return out;
}
// Read-only, layout-independent plotting math: make max/min of a nonflat
// curve touch the usable top and bottom, while preserving a centered flat line.
inline double normalizedY(int value, int minLevel, int maxLevel) {
    if (maxLevel <= minLevel) return 0.5;
    return static_cast<double>(maxLevel - value) /
           static_cast<double>(maxLevel - minLevel);
}
}  // namespace viper_eq
