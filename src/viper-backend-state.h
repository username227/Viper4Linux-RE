#pragma once
// Read-only discovery of the marked ViPER PipeWire process. Unlike the
// shared-memory DSP control page, this exists even before audio activates the
// LADSPA processor. Mirror viper_launcher's strict instance identification;
// do NOT mistake another user's or an ordinary PipeWire process for ViPER.
// Called from the GUI event loop/CLI only, never from an audio callback.
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace viperbass {
namespace detail {
inline std::string procContents(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
inline bool nulField(const std::string& contents, const std::string& value) {
    size_t begin = 0;
    while (begin < contents.size()) {
        const auto end = contents.find('\0', begin);
        const auto len = (end == std::string::npos ? contents.size() : end) - begin;
        if (len == value.size() && contents.compare(begin, len, value) == 0) return true;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return false;
}
inline bool pipewireCommand(const std::string& cmdline) {
    if (!nulField(cmdline, "filter-chain.conf")) return false;
    size_t begin = 0;
    while (begin < cmdline.size()) {
        const auto end = cmdline.find('\0', begin);
        const auto len = (end == std::string::npos ? cmdline.size() : end) - begin;
        if (len == 0) {
            if (end == std::string::npos) break;
            begin = end + 1;
            continue;
        }
        const auto slash = cmdline.rfind('/', begin + len - 1);
        const auto basename = (slash != std::string::npos && slash >= begin)
            ? slash + 1 : begin;
        if (len >= basename - begin &&
            cmdline.compare(basename, begin + len - basename, "pipewire") == 0)
            return true;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return false;
}
} // namespace detail

inline pid_t running_backend_pid(const std::string& proc_root = "/proc") {
    DIR* dir = opendir(proc_root.c_str());
    if (!dir) return 0; // Without /proc access, do not assume the backend is running.
    pid_t found = 0;
    while (const auto* ent = readdir(dir)) {
        const std::string pid_text(ent->d_name);
        if (pid_text.empty() ||
            !std::all_of(pid_text.begin(), pid_text.end(),
                         [](unsigned char c) { return std::isdigit(c) != 0; }))
            continue;
        errno = 0;
        char* rest = nullptr;
        const long parsed = strtol(pid_text.c_str(), &rest, 10);
        if (errno || !rest || *rest || parsed <= 0 ||
            parsed > std::numeric_limits<pid_t>::max()) continue;
        const auto path = proc_root + "/" + pid_text;
        struct stat st{};
        if (stat(path.c_str(), &st) != 0 || st.st_uid != getuid()) continue;
        if (!detail::pipewireCommand(detail::procContents(path + "/cmdline"))) continue;
        if (!detail::nulField(detail::procContents(path + "/environ"),
                              "VIPER_CLEAN_INSTANCE=viperlinux-unified-v1")) continue;
        found = static_cast<pid_t>(parsed);
        break;
    }
    closedir(dir);
    return found;
}

// A process can exist without a DSP channel (e.g. before playback begins).
// Expose this test hook only in the isolated mock-IPC CLI regression build.
inline pid_t detected_backend_pid() {
#ifdef VIPER_MOCK_TEST_IPC
    if (const auto* root = std::getenv("VIPER_TEST_PROC_ROOT"))
        return running_backend_pid(root);
#endif
    return running_backend_pid();
}
// A GUI-local sampler of the already-published DSP callback counter.
// No added shared-memory fields or audio-thread work is required.
// "Active" means callbacks are advancing; a callback may process silence.
enum class BackendAudioActivity { Checking, Active, Idle };

class AudioActivityTracker {
public:
    void reset() noexcept {
        have_sample_ = false;
        backend_pid_ = 0;
        last_callbacks_ = 0;
    }

    BackendAudioActivity sample(pid_t pid, std::uint32_t callbacks) noexcept {
        if (pid <= 0) {
            reset();
            return BackendAudioActivity::Checking;
        }
        if (!have_sample_ || pid != backend_pid_) {
            have_sample_ = true;
            backend_pid_ = pid;
            last_callbacks_ = callbacks;
            return BackendAudioActivity::Checking;
        }
        const auto result = callbacks != last_callbacks_
            ? BackendAudioActivity::Active : BackendAudioActivity::Idle;
        last_callbacks_ = callbacks;
        return result;
    }

private:
    pid_t backend_pid_ = 0;
    std::uint32_t last_callbacks_ = 0;
    bool have_sample_ = false;
};
} // namespace viperbass
