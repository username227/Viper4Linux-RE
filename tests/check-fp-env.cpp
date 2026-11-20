// Verifies the audio-thread FTZ/DAZ helper without starting PipeWire or DSP.
#include "viper-fp-env.h"

#include <cassert>
#include <cstdint>
#include <iostream>

#if defined(__x86_64__) || defined(__i386__)
static unsigned read_mode() { return _mm_getcsr(); }
static void write_mode(unsigned value) { _mm_setcsr(value); }
#elif defined(__aarch64__)
static std::uint64_t read_mode() {
    std::uint64_t value = 0;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(value));
    return value;
}
static void write_mode(std::uint64_t value) {
    __asm__ __volatile__("msr fpcr, %0" :: "r"(value));
}
#endif

int main() {
#if defined(__x86_64__) || defined(__i386__)
    const unsigned saved = read_mode();
    write_mode(saved & ~0x8040u);
    viper_fp::enable_ftz_daz();
    assert((read_mode() & 0x8040u) == 0x8040u);
    viper_fp::enable_ftz_daz();
    assert((read_mode() & 0x8040u) == 0x8040u);
    write_mode(saved);
#elif defined(__aarch64__)
    const std::uint64_t saved = read_mode();
    write_mode(saved & ~(std::uint64_t(1) << 24));
    viper_fp::enable_ftz_daz();
    assert((read_mode() & (std::uint64_t(1) << 24)) != 0);
    write_mode(saved);
#else
    viper_fp::enable_ftz_daz(); // portable no-op
#endif
    std::cout << "PASS: audio-thread FTZ/DAZ mode is enabled and idempotent\n";
}
