// GUI activity classifier and signal detection: no Qt, PipeWire or live DSP.
#include "../src/viper-backend-state.h"
#include "../src/viper-input-signal.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

int main() {
    using viperbass::BackendAudioActivity;
    viperbass::AudioActivityTracker t;
    assert(t.sample(123, 0, 0) == BackendAudioActivity::Checking);
    assert(t.sample(123, 1, 0) == BackendAudioActivity::NoSignal); // always-process silence
    assert(t.sample(123, 2, 1) == BackendAudioActivity::Active);
    assert(t.sample(123, 3, 1) == BackendAudioActivity::Active);   // short quiet passage
    assert(t.sample(123, 4, 1) == BackendAudioActivity::Active);
    assert(t.sample(123, 5, 1) == BackendAudioActivity::NoSignal); // paused for ~1.5 seconds
    assert(t.sample(123, 5, 1) == BackendAudioActivity::Idle);     // callbacks stopped too
    assert(t.sample(123, 6, 2) == BackendAudioActivity::Active);   // resumed
    assert(t.sample(123, 7, 3) == BackendAudioActivity::Active);
    assert(t.sample(456, 0, 0) == BackendAudioActivity::Checking); // new backend
    assert(t.sample(456, 1, 0) == BackendAudioActivity::NoSignal);
    assert(t.sample(0, 100, 100) == BackendAudioActivity::Checking); // offline
    assert(t.sample(456, 101, 100) == BackendAudioActivity::Checking); // reconnect
    t.reset();
    assert(t.sample(456, 101, 100) == BackendAudioActivity::Checking);

    viperbass::AudioActivityTracker wrap;
    constexpr auto max = std::numeric_limits<std::uint32_t>::max();
    assert(wrap.sample(999, max - 1, max - 1) == BackendAudioActivity::Checking);
    assert(wrap.sample(999, max, max) == BackendAudioActivity::Active);
    assert(wrap.sample(999, 0, 0) == BackendAudioActivity::Active); // both counters wrap

    float l[] = {0.0f, 0.00009f, std::numeric_limits<float>::quiet_NaN(), 0.0f};
    float r[] = {0.0f, 0.0f, std::numeric_limits<float>::infinity(), 0.0f};
    assert(!viperbass::hasInputSignal(l, r, 4)); // ignore dither and invalid samples
    assert(!viperbass::hasInputSignal(nullptr, nullptr, 4));
    assert(!viperbass::hasInputSignal(l, r, 0));
    l[1] = -0.001f;
    assert(viperbass::hasInputSignal(l, r, 4)); // left negative amplitude
    l[1] = 0.0f;
    r[3] = 0.0002f;
    assert(viperbass::hasInputSignal(l, r, 4)); // right-channel-only signal
    std::cout << "PASS: GUI input-signal classification, silence, hysteresis and reset\n";
}
