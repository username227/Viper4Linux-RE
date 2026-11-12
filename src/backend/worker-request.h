// Non-realtime worker policy: coalesce changed serials while acknowledging
// the last requested version only after native effect reconfiguration finishes.
#pragma once
#include <atomic>
#include <cstdint>

namespace viper_worker {
template <typename Apply>
void applyPending(std::atomic<std::uint32_t>& requested,
                  std::atomic<std::uint32_t>& acknowledged,
                  std::uint32_t& observed, Apply&& action) {
    const auto current = requested.load(std::memory_order_acquire);
    if (current == observed) return;
    observed = current;
    action();
    acknowledged.store(observed, std::memory_order_release);
}
} // namespace viper_worker
