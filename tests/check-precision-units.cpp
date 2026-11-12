// Regression for seven exact native-to-legacy conversions shared across
// CLI, Audio Output snapshots, named presets and GUI sliders. No live IPC.
#include "viper-precision-units.h"
#include <cassert>
#include <string_view>

int main() {
    using namespace viper_precision;
    assert(forNative("strength_centi") == &binding(Field::Strength));
    assert(forNative("bass_mono_strength_centi") == &binding(Field::MonoStrength));
    assert(forNative("field_depth") == &binding(Field::FieldDepth));
    assert(forNative("unrelated") == nullptr);
    assert(std::string_view(binding(Field::Clarity).legacyKey) == "clarity_gain_tenths");

    assert(toLegacy(Field::Strength, 238) == 23);
    assert(fromLegacy(Field::Strength, 23) == 230);
    assert(fromSlider(Field::Strength, 23) == 230);
    assert(toLegacy(Field::Clarity, 2675) == 26);
    assert(fromLegacy(Field::Clarity, 26) == 2600);
    assert(toLegacy(Field::SpectrumFrequency, 8024) == 80);
    assert(toSlider(Field::SpectrumFrequency, 8024) == 80);
    assert(fromLegacy(Field::SpectrumFrequency, 80) == 8000);
    assert(toLegacy(Field::SpectrumExciter, 2345) == 23);
    assert(toLegacy(Field::DiffLowpass, 4267) == 42);
    assert(fromSlider(Field::DiffLowpass, 42) == 4200);

    // The CLI's Mono Bass coarse INI projection is NOT the same as a
    // snapshot's exact INI projection. Both retain precision/ = 247.
    assert(toLegacy(Field::MonoStrength, 247, true) == 240);
    assert(toLegacy(Field::MonoStrength, 247, false) == 247);
    assert(toSlider(Field::MonoStrength, 247) == 24);
    assert(fromLegacy(Field::MonoStrength, 247) == 247);
    assert(fromSlider(Field::MonoStrength, 24) == 240);

    assert(toLegacy(Field::FieldDepth, 0) == 0);
    assert(fromLegacy(Field::FieldDepth, 0) == 0);
    assert(toLegacy(Field::FieldDepth, 537) == 8);
    assert(toSlider(Field::FieldDepth, 537) == 8);
    assert(toLegacy(Field::FieldDepth, 550) == 8);
    assert(fromLegacy(Field::FieldDepth, 8) == 550);
    assert(fromSlider(Field::FieldDepth, 8) == 550);
    assert(toSlider(Field::FieldDepth, 200) == 1);
    assert(toSlider(Field::FieldDepth, 950) == 16);
    for (int depth = 0; depth <= 950; ++depth) {
        const int expected = depth == 0 ? 0 :
            std::clamp((depth - 150 + 25) / 50, 1, 16);
        assert(toLegacy(Field::FieldDepth, depth) == expected);
        const int readback = depth == 0 ? 0 :
            std::clamp(static_cast<int>(std::lround((depth - 150) / 50.0)), 1, 16);
        assert(toSlider(Field::FieldDepth, depth) == readback);
    }
}
