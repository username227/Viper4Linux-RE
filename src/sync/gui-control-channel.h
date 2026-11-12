// Qt-independent GUI control-channel policies; GUI event loop only.
#pragma once
#include <atomic>
#include <cstdint>
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

// Track one effect group's parameter edits without notifying the worker for
// unchanged values. The GUI publishes the group serial only after all writes.
struct ChangeGroup {
    bool changed = false;
    void set(std::atomic<std::int32_t>& destination, int value) noexcept {
        if (destination.load(std::memory_order_relaxed) != value) {
            destination.store(value, std::memory_order_relaxed);
            changed = true;
        }
    }
    void notify(std::atomic<std::uint32_t>& requestSerial) const noexcept {
        if (changed) requestSerial.fetch_add(1, std::memory_order_release);
    }
};
} // namespace viper_gui_sync
