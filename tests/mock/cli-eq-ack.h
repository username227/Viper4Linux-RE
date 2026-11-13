// Isolated CLI tests emulate the native EQ worker's acknowledgment.
// This thread never runs in the production CLI and never applies DSP effects.
#pragma once
#include "viper-control.h"
#include <atomic>
#include <chrono>
#include <thread>

class MockCliEqAcknowledger {
    viperbass::Controls& controls_;
    std::atomic<bool> stopping_{false};
    std::thread worker_;
public:
    explicit MockCliEqAcknowledger(viperbass::Controls& controls)
        : controls_(controls), worker_([this] {
            while (!stopping_.load(std::memory_order_acquire)) {
                controls_.eq_applied_serial.store(
                    controls_.eq_request_serial.load(std::memory_order_acquire),
                    std::memory_order_release);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }) {}
    MockCliEqAcknowledger(const MockCliEqAcknowledger&) = delete;
    MockCliEqAcknowledger& operator=(const MockCliEqAcknowledger&) = delete;
    void stop() {
        stopping_.store(true, std::memory_order_release);
        if (worker_.joinable()) worker_.join();
    }
    ~MockCliEqAcknowledger() { stop(); }
};
