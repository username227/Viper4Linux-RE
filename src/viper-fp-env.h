#pragma once
// Audio-thread floating-point environment: flush subnormal (denormal) results
// and inputs to zero. Without this, recursive DSP filters that decay after
// playback reach ~1e-38 and every subsequent operation runs in slow microcode,
// raising idle CPU from ~3% to 35-45%. The change only affects values far below
// any audible level (float subnormals are < 1.2e-38, i.e. about -758 dBFS).
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
namespace viper_fp {
inline void enable_ftz_daz() noexcept {
    // MXCSR bit 15 = FTZ (flush results), bit 6 = DAZ (treat inputs as zero).
    _mm_setcsr(_mm_getcsr() | 0x8040u);
}
} // namespace viper_fp
#elif defined(__aarch64__)
#include <cstdint>
namespace viper_fp {
inline void enable_ftz_daz() noexcept {
    // FPCR bit 24 = FZ (flush-to-zero). AArch64 has no separate DAZ bit.
    std::uint64_t fpcr = 0;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    __asm__ __volatile__("msr fpcr, %0" :: "r"(fpcr | (std::uint64_t(1) << 24)));
}
} // namespace viper_fp
#else
namespace viper_fp {
inline void enable_ftz_daz() noexcept {}
} // namespace viper_fp
#endif
