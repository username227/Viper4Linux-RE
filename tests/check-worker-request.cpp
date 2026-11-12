// Exercise worker serial coalescing/acknowledgment without ViPERDSP or PipeWire.
#include "backend/worker-request.h"
#include <cassert>
#include <cstdint>

int main() {
    std::atomic<std::uint32_t> requested{0};
    std::atomic<std::uint32_t> acknowledged{0};
    std::uint32_t observed = acknowledged.load(std::memory_order_acquire);
    int rebuilds = 0;
    const auto process = [&] {
        viper_worker::applyPending(requested, acknowledged, observed,
                                   [&] { ++rebuilds; });
    };
    process();
    assert(rebuilds == 0 && acknowledged.load() == 0);
    requested.store(3, std::memory_order_release);
    process();
    assert(rebuilds == 1 && observed == 3 && acknowledged.load() == 3);
    process();
    assert(rebuilds == 1 && acknowledged.load() == 3);
    // Multiple edits before polling build filters once at the newest serial.
    requested.store(4, std::memory_order_release);
    requested.store(8, std::memory_order_release);
    process();
    assert(rebuilds == 2 && observed == 8 && acknowledged.load() == 8);
    // An edit arriving while the filter is rebuilt must remain pending.
    requested.store(9, std::memory_order_release);
    viper_worker::applyPending(requested, acknowledged, observed, [&] {
        ++rebuilds;
        requested.store(10, std::memory_order_release);
        assert(acknowledged.load() == 8);
    });
    assert(rebuilds == 3 && observed == 9 && acknowledged.load() == 9);
    process();
    assert(rebuilds == 4 && observed == 10 && acknowledged.load() == 10);
    // Wrapped serials are still compared, not ordered.
    requested.store(0, std::memory_order_release);
    process();
    assert(rebuilds == 5 && observed == 0 && acknowledged.load() == 0);
}
