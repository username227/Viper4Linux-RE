#pragma once
// Single source of truth for native DSP values and legacy GUI/preset units.
// The different CLI-vs-snapshot Mono Bass projections are intentional.
// This header has no Qt dependency and is never called by the audio callback.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace viper_precision {
enum class Field { Strength, MonoStrength, Clarity, SpectrumFrequency,
                   SpectrumExciter, DiffLowpass, FieldDepth };

struct Binding {
    Field field;
    const char* nativeKey;
    const char* legacyKey;
};
inline constexpr Binding bindings[] = {
    {Field::Strength, "strength_centi", "strength_tenths"},
    {Field::MonoStrength, "bass_mono_strength_centi", "bass_mono_strength_centi"},
    {Field::Clarity, "clarity_gain_milli", "clarity_gain_tenths"},
    {Field::SpectrumFrequency, "spectrum_frequency_hz", "spectrum_frequency_hundreds"},
    {Field::SpectrumExciter, "spectrum_exciter_milli", "spectrum_exciter_tenths"},
    {Field::DiffLowpass, "diff_lp_hz", "diff_lp_hundreds"},
    {Field::FieldDepth, "field_depth", "field_depth_step"},
};
inline constexpr const Binding& binding(Field field) {
    return bindings[static_cast<std::size_t>(field)];
}
inline constexpr const Binding* forNative(std::string_view key) {
    for (const auto& entry : bindings)
        if (key == entry.nativeKey) return &entry;
    return nullptr;
}
// Exact legacy INI projection; do not round native values when saving a preset
// or Audio Output snapshot. Mono Bass is historically quantized by the CLI
// but persisted unquantized when restored from an Audio Output snapshot.
inline constexpr int toLegacy(Field field, int native, bool cli = false) {
    switch (field) {
    case Field::Strength: return native / 10;
    case Field::MonoStrength: return cli ? (native / 10) * 10 : native;
    case Field::Clarity:
    case Field::SpectrumFrequency:
    case Field::SpectrumExciter:
    case Field::DiffLowpass: return native / 100;
    case Field::FieldDepth:
        return native == 0 ? 0 : std::clamp((native - 150 + 25) / 50, 1, 16);
    }
    return native;
}
// Named format-v1 presets contain legacy slider units, not exact precision/
// values. Never use this conversion for a live native value or CLI precision.
inline constexpr int fromLegacy(Field field, int legacy) {
    switch (field) {
    case Field::Strength: return legacy * 10;
    case Field::MonoStrength: return legacy;
    case Field::Clarity:
    case Field::SpectrumFrequency:
    case Field::SpectrumExciter:
    case Field::DiffLowpass: return legacy * 100;
    case Field::FieldDepth: return legacy == 0 ? 0 : legacy * 50 + 150;
    }
    return legacy;
}
// GUI readback historically uses nearest-step rounding for Field depth,
// whereas the saved legacy projection uses integer division above.
inline int toSlider(Field field, int native) {
    if (field == Field::MonoStrength) return native / 10;
    if (field == Field::FieldDepth)
        return native == 0 ? 0 :
            std::clamp(static_cast<int>(std::lround((native - 150) / 50.0)), 1, 16);
    return toLegacy(field, native);
}
inline constexpr int fromSlider(Field field, int slider) {
    return field == Field::MonoStrength ? slider * 10 : fromLegacy(field, slider);
}
} // namespace viper_precision
