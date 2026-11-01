#pragma once
// Conservative, bounded RIFF/WAVE impulse-response decoder, not an audio-thread operation.
// Supports ordinary PCM16/24/32 and IEEE float32 (mono or stereo); .irs is WAV bytes.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <utility>
#include <string>
#include <vector>

namespace viper_ir {
struct Wave { std::vector<float> samples; std::uint32_t rate = 0; std::uint32_t channels = 0; std::string error; };
inline std::uint16_t u16(const std::uint8_t *p) { return std::uint16_t(p[0] | (std::uint16_t(p[1])<<8)); }
inline std::uint32_t u32(const std::uint8_t *p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) | (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);
}
inline Wave read(const std::string &path) {
    Wave w;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) { w.error="Cannot open impulse response"; return w; }
    const auto len = file.tellg();
    constexpr std::streamoff max_bytes = 8 * 1024 * 1024;
    if (len < 44 || len > max_bytes) { w.error="IR must be 44 bytes to 8 MiB"; return w; }
    std::vector<std::uint8_t> bytes(static_cast<size_t>(len));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), len)) { w.error="IR file read failed"; return w; }
    if (std::memcmp(bytes.data(), "RIFF", 4) || std::memcmp(bytes.data()+8, "WAVE", 4)) {
        w.error="IR is not a RIFF/WAV file"; return w;
    }
    std::uint16_t encoding=0, bits=0, block_align=0;
    std::uint32_t frames=0;
    size_t data_pos=0, data_len=0;
    bool fmt=false, data=false;
    for (size_t off=12; off+8 <= bytes.size();) {
        const size_t chunk_len=u32(bytes.data()+off+4);
        const size_t payload=off+8;
        if (chunk_len > bytes.size()-payload) { w.error="Truncated RIFF chunk"; return w; }
        if (!std::memcmp(bytes.data()+off, "fmt ", 4)) {
            if (chunk_len<16) { w.error="Invalid WAV fmt chunk"; return w; }
            encoding=u16(bytes.data()+payload);
            w.channels=u16(bytes.data()+payload+2);
            w.rate=u32(bytes.data()+payload+4);
            block_align=u16(bytes.data()+payload+12);
            bits=u16(bytes.data()+payload+14);
            fmt=true;
        } else if (!std::memcmp(bytes.data()+off, "data", 4) && !data) {
            data_pos=payload; data_len=chunk_len; data=true;
        }
        off=payload+chunk_len+(chunk_len&1u);
    }
    if (!fmt || !data || (w.channels !=1 && w.channels!=2) || !w.rate ||
        !((encoding==1 && (bits==16 || bits==24 || bits==32)) || (encoding==3 && bits==32)) ||
        block_align != w.channels * (bits/8) || data_len % block_align != 0) {
        w.error="Unsupported WAV (use mono/stereo PCM16/24/32 or float32)"; return w;
    }
    frames=static_cast<std::uint32_t>(data_len/block_align);
    if (frames<16 || frames > w.rate*5ull) { w.error="IR duration must be 16 frames to 5 seconds"; return w; }
    const std::uint32_t bytes_per_sample=bits/8;
    w.samples.resize(size_t(frames)*w.channels);
    for (size_t i=0; i<w.samples.size(); ++i) {
        const auto* sample=bytes.data()+data_pos+i*bytes_per_sample;
        float x=0.0f;
        if (encoding==3) {
            const auto raw=u32(sample);
            static_assert(sizeof(float)==4, "32-bit float needed");
            std::memcpy(&x, &raw, 4);
        } else if (bits==16) {
            x=static_cast<std::int16_t>(u16(sample)) / 32768.0f;
        } else if (bits==24) {
            std::int32_t v=static_cast<std::int32_t>(u32(std::array<std::uint8_t,4>{sample[0],sample[1],sample[2],0}.data()));
            if (v & 0x800000) v |= static_cast<std::int32_t>(0xff000000u);
            x=static_cast<float>(v) / 8388608.0f;
        } else {
            const auto v=static_cast<std::int32_t>(u32(sample));
            x=static_cast<float>(v) / 2147483648.0f;
        }
        if (!std::isfinite(x)) { w.samples.clear(); w.error="IR contains NaN or infinity"; return w; }
        w.samples[i]=x;
    }
    return w;
}

// Resample an impulse-response FILTER, not ordinary continuous program audio.
// Use a windowed low-pass sinc, then scale by source/destination sample-rate
// ratio so that an impulse response retains approximately its DC gain after
// converting its discrete filter taps. File loading and this conversion must
// run off the real-time processing callback.
inline bool resample_to(Wave& w, std::uint32_t target_rate) {
    if (!w.error.empty()) return false;
    if (w.rate == target_rate) return true;
    if (w.rate < 8000 || w.rate > 384000 || target_rate < 8000 || target_rate > 384000 ||
        (w.channels != 1 && w.channels != 2) || w.samples.size() % w.channels != 0) {
        w.error = "Invalid impulse response sample rate or channels";
        return false;
    }
    const std::size_t source_frames = w.samples.size()/w.channels;
    // Native Convolver requires at least 16 frames; pad any shorter result
    // with silence rather than rejecting a valid short source kernel.
    const auto target_frames = std::max<std::uint64_t>(16,
        static_cast<std::uint64_t>(std::llround(
            static_cast<double>(source_frames)*target_rate/w.rate)));
    // Upper bound avoids runaway allocations from malformed rates/lengths.
    if (target_frames < 16 || target_frames > static_cast<std::uint64_t>(target_rate)*5 ||
        target_frames*w.channels > 4000000ull) {
        w.error = "Resampled IR exceeds supported duration or memory limit";
        return false;
    }
    std::vector<float> result(static_cast<std::size_t>(target_frames)*w.channels);
    constexpr int radius = 24;
    constexpr double pi = 3.14159265358979323846;
    const double cutoff = std::min(1.0, static_cast<double>(target_rate)/w.rate);
    const double rate_ratio = static_cast<double>(w.rate)/target_rate;
    const auto sinc = [](double x) -> double {
        if (std::abs(x) < 1e-12) return 1.0;
        constexpr double p = 3.14159265358979323846;
        return std::sin(p*x)/(p*x);
    };
    for (std::size_t out=0; out<target_frames; ++out) {
        const double center = static_cast<double>(out)*rate_ratio;
        const auto base = static_cast<std::int64_t>(std::floor(center));
        for (int tap=-radius+1; tap<=radius; ++tap) {
            const auto source_index = base+tap;
            if (source_index < 0 || source_index >= static_cast<std::int64_t>(source_frames))
                continue; // Zero padding is appropriate for a finite FIR.
            const double distance = center-source_index;
            if (std::abs(distance) >= radius) continue;
            const double window = 0.5+0.5*std::cos(pi*distance/radius);
            const double weight = cutoff*sinc(cutoff*distance)*window*rate_ratio;
            for (std::uint32_t ch=0; ch<w.channels; ++ch)
                result[out*w.channels+ch] += static_cast<float>(
                    w.samples[static_cast<std::size_t>(source_index)*w.channels+ch]*weight);
        }
    }
    w.samples = std::move(result);
    w.rate = target_rate;
    return true;
}
} // namespace viper_ir
