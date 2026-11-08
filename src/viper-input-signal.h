#pragma once
// A cheap input-signal predicate for the PipeWire/LADSPA real-time callback.
// It only reads the input ports; no allocation, locking or audio modification.
#include <cmath>

namespace viperbass {
constexpr float kInputSignalFloor = 0.0001f; // about -80 dBFS; ignore near-silent dither

inline bool hasInputSignal(const float* left, const float* right,
                           unsigned long frames) noexcept {
    if (!left && !right) return false;
    for (unsigned long i = 0; i < frames; ++i) {
        if (left) {
            const float sample = left[i];
            if ((sample > kInputSignalFloor || sample < -kInputSignalFloor) &&
                std::isfinite(sample)) return true;
        }
        if (right) {
            const float sample = right[i];
            if ((sample > kInputSignalFloor || sample < -kInputSignalFloor) &&
                std::isfinite(sample)) return true;
        }
    }
    return false;
}
} // namespace viperbass
