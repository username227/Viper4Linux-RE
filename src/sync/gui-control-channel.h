// Qt-independent GUI control-channel policies; GUI event loop only.
#pragma once
namespace viper_gui_sync {
// Reopen the named shared-memory object after unlink/replacement.
template<typename Mapping>
bool ensureCurrent(Mapping& mapping) {
    if (mapping.current()) return true;
    mapping.close();
    return mapping.attach();
}
// Preserve the live DSP's exact value unless the slider was deliberately
// edited or a complete preset/reset explicitly replaces the value.
constexpr int nativePrecision(bool replace, bool editedSlider,
                              int liveValue, int sliderValue) noexcept {
    return replace || editedSlider ? sliderValue : liveValue;
}
} // namespace viper_gui_sync
