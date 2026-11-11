#pragma once
// Stage the complete selected Multiband configuration before the native
// disabled->enabled reset. Subsequent live edits keep the native smoothers.
// Pinned ViPERDSP headers omit standard includes; mirror viper-ladspa.cpp.
#include "viperdsp_compat.h"
#include "viper/ViPER.h"
#include <cstdint>

namespace viper_multiband {
inline void apply(ViPER& dsp, const viper::MultibandCompressorParams& selected,
                  bool was_enabled, std::uint32_t previous_band_count) {
    // Native ApplyMultibandCompressor() enables before setting thresholds.
    // An initial enable or a 3<->5 topology change therefore otherwise resets
    // FET threshold/gain smoothers with stale values (0 dB on fresh bands).
    // Perform BOTH calls under the existing non-RT worker mutex. Do not do
    // this for ordinary edits: they must retain their running smoother state.
    if (selected.enable && (!was_enabled || selected.band_count != previous_band_count)) {
        auto staged = selected;
        staged.enable = false;
        dsp.ApplyMultibandCompressor(staged);
    }
    dsp.ApplyMultibandCompressor(selected);
}
} // namespace viper_multiband
