#pragma once
// Linux-only, per-user shared-memory control channel for ViPER for Linux.
// The filter-chain process owns the page; GUI and CLI clients only attach to it.
// All shared mutable fields are lock-free 32-bit atomics (x86_64 Linux).
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace viperbass {
constexpr std::uint32_t kMagic = 0x56434C31; // VBPC
constexpr std::uint32_t kVersion = 26;
struct alignas(64) Controls {
    std::uint32_t magic = kMagic;
    std::uint32_t version = kVersion;
    std::atomic<std::int32_t> owner_pid{0};
    std::atomic<std::int32_t> active{0};
    std::atomic<std::uint32_t> audio_callbacks{0};
    std::atomic<std::int32_t> enabled{1};
    std::atomic<std::int32_t> master_enabled{1}; // bypass all DSP effects, preserve parameters
    std::atomic<std::int32_t> bass_mode{0}; // 0 Natural, 1 Pure Bass+, 2 Subwoofer
    // New bass processors default OFF. Values use scaled integers for lock-free IPC.
    std::atomic<std::int32_t> bass_mono_enabled{0};
    std::atomic<std::int32_t> bass_mono_mode{0};
    std::atomic<std::int32_t> bass_mono_strength_centi{200}; // 50..1000 = 0.5..10
    std::atomic<std::int32_t> bass_mono_frequency_hz{65}; // 15..150
    std::atomic<std::int32_t> psycho_enabled{0};
    std::atomic<std::int32_t> psycho_cutoff_hz{80}; // 60..150
    std::atomic<std::int32_t> psycho_intensity_percent{30}; // 0..100
    std::atomic<std::int32_t> psycho_order{3}; // 2..5
    std::atomic<std::int32_t> psycho_original_percent{100}; // 0..100
    std::atomic<std::int32_t> dynamic_enabled{0};
    std::atomic<std::int32_t> dynamic_x_low{100}; // 0..2400
    std::atomic<std::int32_t> dynamic_x_high{6000}; // 0..12000
    std::atomic<std::int32_t> dynamic_y_low{100}; // 0..200
    std::atomic<std::int32_t> dynamic_y_high{150}; // 0..300
    std::atomic<std::int32_t> dynamic_side_low_percent{50};
    std::atomic<std::int32_t> dynamic_side_high_percent{50};
    std::atomic<std::int32_t> dynamic_strength_percent{0}; // start neutral
    std::atomic<std::int32_t> strength_centi{1000}; // 0.50 to 10.00
    std::atomic<std::int32_t> frequency_hz{65};
    std::atomic<std::int32_t> clarity_enabled{0}; // independent from Bass
    std::atomic<std::int32_t> clarity_mode{0}; // 0 Natural, 1 Ozone, 2 XHiFi
    std::atomic<std::int32_t> clarity_gain_milli{1500}; // 0..4.5
    std::atomic<std::int32_t> spectrum_enabled{0}; // independent from Bass and Clarity
    std::atomic<std::int32_t> spectrum_frequency_hz{7600}; // 2200..8200
    std::atomic<std::int32_t> spectrum_exciter_milli{1000}; // 0..6
    // Native TubeSimulator: GUI commits parameters and worker applies them
    // under the wet-engine lock. Upstream resets filters for model/drive edits.
    std::atomic<std::int32_t> tube_enabled{0};
    std::atomic<std::int32_t> tube_model{0}; // 0=12AX7, 1=12AU7, 2=6N1P
    std::atomic<std::int32_t> tube_drive_percent{50}; // 0..100, upstream float 0..1
    std::atomic<std::int32_t> tube_mix_percent{30}; // 0..100, upstream float 0..1
    std::atomic<std::uint32_t> tube_request_serial{0};
    std::atomic<std::uint32_t> tube_applied_serial{0};
    // Native AnalogX: 0=Mild, 1=Medium, 2=Strong. Model changes reset
    // the upstream filters, so commit on the non-real-time worker.
    std::atomic<std::int32_t> analogx_enabled{0};
    std::atomic<std::int32_t> analogx_mode{0};
    std::atomic<std::uint32_t> analogx_request_serial{0};
    std::atomic<std::uint32_t> analogx_applied_serial{0};
    // Native ViPERDSP Cure crossfeed. Off by default; model changes on the worker.
    std::atomic<std::int32_t> crossfeed_enabled{0};
    std::atomic<std::int32_t> crossfeed_preset{0}; // 0=Mild, 1=Medium, 2=Strong
    std::atomic<std::uint32_t> crossfeed_request_serial{0};
    std::atomic<std::uint32_t> crossfeed_applied_serial{0};
    // Native FET Compressor, default disabled. Ratio is stored as R*10.
    // dB and milliseconds are converted to native logarithmic/second units by worker.
    std::atomic<std::int32_t> fet_enabled{0};
    std::atomic<std::int32_t> fet_threshold_db{-18}; // -48..0 dB
    std::atomic<std::int32_t> fet_ratio_tenths{20}; // 10..100 = 1:1 .. 10:1
    std::atomic<std::int32_t> fet_knee_db{6}; // 0..12 dB
    std::atomic<std::int32_t> fet_makeup_db{0}; // 0..24 dB
    std::atomic<std::int32_t> fet_attack_ms{10}; // 1..100 ms
    std::atomic<std::int32_t> fet_release_ms{100}; // 5..500 ms
    std::atomic<std::uint32_t> fet_request_serial{0};
    std::atomic<std::uint32_t> fet_applied_serial{0};
    // Native ViPERDSP Multiband Compressor: 3 or 5 bands, off by default.
    // Each band is an independent native FET compressor instance.
    struct MultibandControls {
        std::atomic<std::int32_t> enabled{1};
        std::atomic<std::int32_t> threshold_db{-18};
        std::atomic<std::int32_t> ratio_tenths{20};
        std::atomic<std::int32_t> knee_db{6};
        std::atomic<std::int32_t> makeup_db{0};
        std::atomic<std::int32_t> attack_ms{10};
        std::atomic<std::int32_t> release_ms{100};
    };
    std::atomic<std::int32_t> mb_enabled{0};
    std::atomic<std::int32_t> mb_band_count{3};
    std::atomic<std::int32_t> mb_crossovers_hz[4]{{200}, {4000}, {5000}, {8000}};
    MultibandControls mb_bands[5]{};
    std::atomic<std::uint32_t> mb_request_serial{0};
    std::atomic<std::uint32_t> mb_applied_serial{0};
    // ViPER-DDC: path stored in QSettings; loader worker validates and applies.
    std::atomic<std::int32_t> ddc_enabled{0};
    std::atomic<std::int32_t> ddc_ready{0};
    std::atomic<std::int32_t> ddc_status{0}; // 0 none, 1 ready, 2 loading, -1 invalid, -2 unsupported rate
    std::atomic<std::uint32_t> ddc_request_serial{0};
    std::atomic<std::uint32_t> ddc_applied_serial{0};
    // Speaker Correction is a native, fixed-filter processor intended for speakers.
    // Its enable/reset is performed on the non-real-time worker.
    std::atomic<std::int32_t> speaker_enabled{0};
    std::atomic<std::uint32_t> speaker_request_serial{0};
    std::atomic<std::uint32_t> speaker_applied_serial{0};
    // Native three-band Stereo Imager. 100% = unity side gain in each band.
    // Crossover coefficient updates belong to the non-realtime worker.
    std::atomic<std::int32_t> imager_enabled{0};
    std::atomic<std::int32_t> imager_low_width_percent{100}; // 0..200
    std::atomic<std::int32_t> imager_mid_width_percent{100}; // 0..200
    std::atomic<std::int32_t> imager_high_width_percent{100}; // 0..200
    std::atomic<std::int32_t> imager_low_crossover_hz{200}; // 80..400
    std::atomic<std::int32_t> imager_high_crossover_hz{4000}; // 2000..8000
    std::atomic<std::uint32_t> imager_request_serial{0};
    std::atomic<std::uint32_t> imager_applied_serial{0};
    std::atomic<std::int32_t> headphone_enabled{0}; // independent Headphone Surround+
    std::atomic<std::int32_t> headphone_quality{0}; // INTERNAL 0..4; GUI/CLI display Levels 1..5
    std::atomic<std::int32_t> diff_enabled{0}; // independent Differential Surround
    std::atomic<std::int32_t> diff_delay_ms{8}; // 1..20 milliseconds
    std::atomic<std::int32_t> diff_reverse{0}; // delay left rather than right
    std::atomic<std::int32_t> diff_mix_percent{50}; // 0..100% delayed channel blend
    std::atomic<std::int32_t> diff_lp_hz{0}; // 0 disables low-pass, else 100..20000 Hz
    std::atomic<std::int32_t> field_enabled{0}; // independent Field Surround; off by default
    std::atomic<std::int32_t> field_widen_tenths{10}; // 0..80 = 0..8.0 widening
    std::atomic<std::int32_t> field_mid_hundredths{100}; // 100..200 = 1.0..2.0 mid image
    std::atomic<std::int32_t> field_depth{300}; // 0(off), or 200..950
    // ViPERDSP Reverberation native parameters 0..1, stored in integer %.
    std::atomic<std::int32_t> reverb_enabled{0}; // off unless explicitly enabled
    std::atomic<std::int32_t> reverb_room_percent{50};
    std::atomic<std::int32_t> reverb_width_percent{50};
    std::atomic<std::int32_t> reverb_damp_percent{50};
    std::atomic<std::int32_t> reverb_wet_percent{20};
    std::atomic<std::int32_t> reverb_dry_percent{50};
    // Output processors: integer IPC values avoid torn floating point controls.
    // Limiter is permanently present in the DSP engine; no enable flag.
    std::atomic<std::int32_t> limiter_threshold_permille{500}; // 300..1000 (linear)
    std::atomic<std::int32_t> limiter_output_percent{100}; // 1..200 (percent)
    std::atomic<std::int32_t> limiter_pan_percent{0}; // -100..100
    std::atomic<std::int32_t> agc_enabled{0};
    std::atomic<std::int32_t> agc_strength_centi{100}; // 50..300, 0.5..3
    std::atomic<std::int32_t> agc_max_gain_centi{300}; // 100..1000, 1..10
    std::atomic<std::int32_t> agc_threshold_permille{900}; // 300..1000
    std::atomic<std::int32_t> lufs_enabled{0};
    std::atomic<std::int32_t> lufs_target_tenths{-140}; // -240..-80, -24..-8 LUFS
    std::atomic<std::int32_t> lufs_max_gain_tenths{60}; // 0..120, dB
    std::atomic<std::int32_t> lufs_speed{1}; // 0 slow, 1 normal, 2 fast
    // Native ViPERDSP IIR equalizer, 10/15/25/31 bands. Each level is 0.01 dB.
    // The worker reconstructs coefficients; audio never allocates for EQ edits.
    std::atomic<std::int32_t> eq_enabled{0};
    std::atomic<std::int32_t> eq_band_count{10};
    std::atomic<std::int32_t> eq_levels_hundredths[31]{};
    std::atomic<std::uint32_t> eq_request_serial{0};
    std::atomic<std::uint32_t> eq_applied_serial{0};
    // Native DynamicEQ: maximum ten independently configurable bands. All
    // setters that rebuild biquad filters execute on the non-RT worker.
    struct DynamicEqBandControl {
        std::atomic<std::int32_t> frequency_hz{1000};
        std::atomic<std::int32_t> q_hundredths{100};
        std::atomic<std::int32_t> gain_hundredths{0};
        std::atomic<std::int32_t> threshold_tenths{-240};
        std::atomic<std::int32_t> attack_ms{10};
        std::atomic<std::int32_t> release_ms{100};
        std::atomic<std::int32_t> filter_type{0}; // Peak, low shelf, high shelf
    };
    std::atomic<std::int32_t> deq_enabled{0};
    std::atomic<std::int32_t> deq_band_count{3};
    DynamicEqBandControl deq_bands[10]{};
    std::atomic<std::uint32_t> deq_request_serial{0};
    std::atomic<std::uint32_t> deq_applied_serial{0};
    // Live Convolver reload: file decoding and kernel construction happen off the RT thread.
    std::atomic<std::int32_t> convolver_enabled{0};
    std::atomic<std::int32_t> convolver_cross_percent{0};
    std::atomic<std::uint32_t> convolver_request_serial{0}; // GUI increments after saving requested filename
    std::atomic<std::uint32_t> convolver_applied_serial{0}; // backend acknowledges completed request
    std::atomic<std::int32_t> convolver_ready{0}; // native kernel successfully loaded
    std::atomic<std::int32_t> convolver_status{0}; // 0 none, 1 ready, -1 invalid, -2 rate mismatch
    std::atomic<std::int32_t> sample_rate_hz{0}; // actual LADSPA sample rate for GUI diagnostics
};
static_assert(std::atomic<std::int32_t>::is_always_lock_free,
              "The IPC prototype needs lock-free 32-bit atomics");

// Regression tests run the actual LADSPA adapter with a mock ViPERDSP engine.
// They must NEVER publish over, attach to, or unlink a running user's live
// backend. The mock DSO and its check executable are loaded into the same
// process, so a PID-scoped test channel is unique even for concurrent tests.
// This override is compiled ONLY with -DVIPER_MOCK_TEST_IPC; production
// binaries retain the existing, unchanged shared-memory channel name.
inline std::string name() {
#ifdef VIPER_MOCK_TEST_IPC
    return "/viperlinux-unified-v5-mock-" + std::to_string(getuid())
         + "-" + std::to_string(getpid());
#else
    return "/viperlinux-unified-v10-control-" + std::to_string(getuid());
#endif
}
inline bool process_alive(std::int32_t pid) {
    if (pid <= 0) return false;
    return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
}
inline bool valid(const Controls* p) {
    return p && p->magic == kMagic && p->version == kVersion;
}
inline bool running(const Controls* p) {
    return valid(p) && p->active.load(std::memory_order_acquire) == 1 &&
           process_alive(p->owner_pid.load(std::memory_order_acquire));
}

class Mapping {
public:
    Mapping() = default;
    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;
    ~Mapping() { close(); }

    Controls* get() const { return p_; }
    void close() {
        if (p_) { munmap(p_, sizeof(Controls)); p_ = nullptr; }
        device_ = 0;
        inode_ = 0;
    }

    // The old GUI checked only the owner PID and an 'active' flag. A POSIX
    // shared-memory object may be unlinked and replaced while the old mapping
    // remains accessible and still reports a live PID. Verify that our mapping
    // refers to the object currently published under the control-channel name.
    // This is for GUI/CLI threads, never the real-time audio callback.
    bool current() const {
        if (!running(p_) || inode_ == 0) return false;
        const int fd = shm_open(name().c_str(), O_RDONLY | O_CLOEXEC, 0);
        if (fd < 0) return false;
        struct stat st{};
        const bool matches = fstat(fd, &st) == 0 &&
            st.st_size >= static_cast<off_t>(sizeof(Controls)) &&
            st.st_dev == device_ && st.st_ino == inode_;
        ::close(fd);
        return matches;
    }
    bool attach() {
        close();
        int fd = shm_open(name().c_str(), O_RDWR | O_CLOEXEC, 0);
        if (fd < 0) return false;
        struct stat st{};
        if (fstat(fd, &st) != 0 || st.st_size < static_cast<off_t>(sizeof(Controls))) {
            ::close(fd);
            return false;
        }
        void* mem = mmap(nullptr, sizeof(Controls), PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, 0);
        ::close(fd);
        if (mem == MAP_FAILED) return false;
        p_ = static_cast<Controls*>(mem);
        device_ = st.st_dev;
        inode_ = st.st_ino;
        if (!current()) { close(); return false; }
        return true;
    }

    // Called only by the LADSPA plugin at instantiation, not from the RT callback.
    bool create_server() {
        close();
        const std::string path = name();
        int fd = shm_open(path.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0 && errno == EEXIST) {
            // Reclaim only an abandoned page; never unlink a live processor's page.
            int previous = shm_open(path.c_str(), O_RDONLY | O_CLOEXEC, 0);
            bool busy = true;
            if (previous >= 0) {
                struct stat st{};
                if (fstat(previous, &st) == 0 && st.st_size >= static_cast<off_t>(sizeof(Controls))) {
                    void* mem = mmap(nullptr, sizeof(Controls), PROT_READ, MAP_SHARED, previous, 0);
                    if (mem != MAP_FAILED) {
                        const auto* old = static_cast<const Controls*>(mem);
                        busy = running(old);
                        munmap(mem, sizeof(Controls));
                    }
                }
                ::close(previous);
            }
            if (busy) return false;
            shm_unlink(path.c_str());
            fd = shm_open(path.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        }
        if (fd < 0) return false;
        if (ftruncate(fd, sizeof(Controls)) != 0) {
            ::close(fd); shm_unlink(path.c_str()); return false;
        }
        struct stat st{};
        if (fstat(fd, &st) != 0) {
            ::close(fd); shm_unlink(path.c_str()); return false;
        }
        void* mem = mmap(nullptr, sizeof(Controls), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        ::close(fd);
        if (mem == MAP_FAILED) { shm_unlink(path.c_str()); return false; }
        device_ = st.st_dev;
        inode_ = st.st_ino;
        p_ = new (mem) Controls();
        p_->owner_pid.store(static_cast<std::int32_t>(getpid()), std::memory_order_release);
        p_->active.store(1, std::memory_order_release);
        return true;
    }
    void stop_server() {
        if (!p_) return;
        p_->active.store(0, std::memory_order_release);
        shm_unlink(name().c_str());
        close();
    }
private:
    Controls* p_ = nullptr;
    dev_t device_ = 0;
    ino_t inode_ = 0;
};
} // namespace viperbass
