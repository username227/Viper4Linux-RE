#pragma once
// Explicit, opt-in GUI exit operation. Never stop system PipeWire or an
// unmarked filter-chain client. Linux pidfds bind the signal to the verified
// process even when PIDs are reused between discovery and termination.
#include "viper-backend-state.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace viper_gui_exit {

enum class StopResult { Signalled, AlreadyStopped, Refused, Failed };

inline bool isMarkedBackend(pid_t pid, const std::string& procRoot = "/proc") {
    if (pid <= 0) return false;
    const std::string path = procRoot + "/" + std::to_string(pid);
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0 || st.st_uid != ::getuid()) return false;
    if (!viperbass::detail::pipewireCommand(
            viperbass::detail::procContents(path + "/cmdline"))) return false;
    return viperbass::detail::nulField(
        viperbass::detail::procContents(path + "/environ"),
        "VIPER_CLEAN_INSTANCE=viperlinux-unified-v1");
}

inline StopResult stopMarkedBackend(pid_t pid, std::string& error,
                                    const std::string& procRoot = "/proc") {
    error.clear();
    if (pid <= 0) return StopResult::AlreadyStopped;
#if !defined(SYS_pidfd_open) || !defined(SYS_pidfd_send_signal)
    error = "Linux pidfd signaling is unavailable; refusing an unsafe PID-based stop.";
    return StopResult::Failed;
#else
    const int fd = static_cast<int>(::syscall(SYS_pidfd_open, pid, 0));
    if (fd < 0) {
        if (errno == ESRCH) return StopResult::AlreadyStopped;
        error = std::string("Cannot open backend process: ") + std::strerror(errno);
        return StopResult::Failed;
    }
    // Revalidate after binding the pidfd. A stale PID or an unrelated
    // PipeWire process must NEVER receive a stop request.
    if (!isMarkedBackend(pid, procRoot)) {
        ::close(fd);
        error = "The target process could not be verified as the ViPER backend; refusing to stop it.";
        return StopResult::Refused;
    }
    const int sent = static_cast<int>(::syscall(SYS_pidfd_send_signal, fd, SIGTERM,
                                                nullptr, 0));
    const int savedErrno = errno;
    ::close(fd);
    if (sent == 0) return StopResult::Signalled;
    if (savedErrno == ESRCH) return StopResult::AlreadyStopped;
    error = std::string("Could not stop ViPER backend: ") + std::strerror(savedErrno);
    return StopResult::Failed;
#endif
}

} // namespace viper_gui_exit
