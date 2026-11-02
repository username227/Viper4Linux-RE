// r67 GUI activity classifier: no Qt, PipeWire or live DSP required.
#include "../src/viper-backend-state.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

int main() {
    using viperbass::BackendAudioActivity;
    viperbass::AudioActivityTracker t;
    assert(t.sample(123, 0) == BackendAudioActivity::Checking);
    assert(t.sample(123, 0) == BackendAudioActivity::Idle);
    assert(t.sample(123, 10) == BackendAudioActivity::Active);
    assert(t.sample(123, 10) == BackendAudioActivity::Idle);
    assert(t.sample(123, std::numeric_limits<std::uint32_t>::max()) == BackendAudioActivity::Active);
    assert(t.sample(123, 0) == BackendAudioActivity::Active); // counter rollover
    assert(t.sample(456, 0) == BackendAudioActivity::Checking); // new backend
    assert(t.sample(456, 0) == BackendAudioActivity::Idle);
    assert(t.sample(0, 100) == BackendAudioActivity::Checking); // offline
    assert(t.sample(456, 101) == BackendAudioActivity::Checking); // reconnect
    t.reset();
    assert(t.sample(456, 101) == BackendAudioActivity::Checking);
    std::cout << "PASS: GUI DSP activity classification and reset\n";
}
