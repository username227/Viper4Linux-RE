#include "viper-eq-presets.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
int main() {
    const auto& presets = viper_eq::builtIns();
    assert(presets.size() == 12);
    const std::vector<std::string> expected = {"Acoustic", "Bass Booster", "Bass Reducer",
        "Classical", "Deep", "Flat", "R&B", "Rock", "Small Speakers",
        "Treble Booster", "Treble Reducer", "Vocal Booster"};
    for (size_t i = 0; i < expected.size(); ++i) assert(presets[i].name == expected[i]);
    for (int count : {10, 15, 25, 31}) {
        const auto& freq = viper_eq::frequencies(count);
        for (int i = 1; i < count; ++i) assert(freq[i] > freq[i-1]);
        for (const auto& preset : presets) {
            const auto curve = preset.levels(count);
            assert(static_cast<int>(curve.size()) == count);
            for (auto value : curve) assert(value >= -1200 && value <= 1200);
            if (std::string(preset.name) == "Flat")
                for (auto value : curve) assert(value == 0);
        }
    }
    // 1.75 dB and independent 15/25/31 native Acoustic curves are exact.
    assert(presets[0].ten[7] == 175);
    assert(presets[0].fifteen[7] == 100);
    assert(presets[0].twentyfive[7] == 250);
    assert(presets[0].thirtyone[7] == 400);
    assert(presets[1].ten[0] == 600 && presets[2].ten[0] == -600);
    assert(presets[9].thirtyone[30] == 500);
    assert(presets[10].thirtyone[30] == -500);
    assert(viper_eq::normalizedY(50, -50, 50) == 0.0);
    assert(viper_eq::normalizedY(-50, -50, 50) == 1.0);
    assert(viper_eq::normalizedY(0, 0, 0) == 0.5);
    assert(viper_eq::normalizedY(175, 0, 175) == 0.0);
    assert(viper_eq::normalizedY(0, 0, 175) == 1.0);
    assert(viper_eq::interpolate({}, {}, 31) == std::vector<int>(31, 0));
    assert(viper_eq::interpolate({100.f, 10.f}, {100, -100}, 15) == std::vector<int>(15, 0));
    std::cout << "PASS: 12 exact upstream V4A EQ presets at 10/15/25/31 bands, centi-dB precision, full-height plot mapping\n";
}
