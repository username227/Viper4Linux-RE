#!/usr/bin/env python3
"""Source contracts and isolated pidfd signaling using a harmless dummy child."""
from pathlib import Path
from tempfile import TemporaryDirectory
import os
import subprocess

root = Path(__file__).resolve().parents[1]
gui = (root / "src/viper-gui.cpp").read_text()
shell = (root / "src/window/window-shell.inc").read_text()
assert '#include "gui-backend-exit.h"' in gui
assert 'viper_gui_exit::stopMarkedBackend(backendPid, error)' in shell
assert 'viperbass::running_backend_pid()' in shell
assert 'Remember this choice' in shell and 'ui/backendOnExit' in shell
assert 'if (!prepareBackendForGuiExit()) return;' in shell
assert 'if (backendPid <= 0) return true;' in shell
decide = shell[shell.index('bool prepareBackendForGuiExit()'):shell.index('void quitGui()')]
assert 'else return false;' in decide  # Cancel leaves GUI open.
assert 'setDefaultButton(keepButton)' in decide
assert 'setEscapeButton(cancelButton)' in decide
assert 'exitGroup->checkedId()' in shell
close = gui[gui.index('void closeEvent(QCloseEvent* event) override {'):
            gui.index('void changeEvent(QEvent* event) override {')]
assert 'quitGui();' in close and 'event->ignore();' in close
assert 'if (choice == "tray")' in close and 'if (choice == "minimized")' in close
assert close.index('if (choice == "minimized")') < close.index('quitGui();')

source = r'''
#include "gui-backend-exit.h"
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

static void writeBinary(const std::filesystem::path& path, const std::string& s) {
    std::ofstream out(path, std::ios::binary);
    assert(out);
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
    assert(out);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string fakeProc = argv[1];
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        for (;;) pause();
    }
    const auto directory = std::filesystem::path(fakeProc) / std::to_string(child);
    std::filesystem::create_directories(directory);
    const char argvData[] = "pipewire\0-c\0filter-chain.conf\0";
    writeBinary(directory / "cmdline", std::string(argvData, sizeof(argvData) - 1));
    writeBinary(directory / "environ", "UNRELATED_PROCESS=1");
    std::string error;
    assert(!viper_gui_exit::isMarkedBackend(child, fakeProc));
    assert(viper_gui_exit::stopMarkedBackend(child, error, fakeProc)
        == viper_gui_exit::StopResult::Refused);
    assert(kill(child, 0) == 0); // An unmarked process was not touched.
    const char marker[] = "VIPER_CLEAN_INSTANCE=viperlinux-unified-v1\0";
    writeBinary(directory / "environ", std::string(marker, sizeof(marker) - 1));
    assert(viper_gui_exit::isMarkedBackend(child, fakeProc));
    assert(viper_gui_exit::stopMarkedBackend(child, error, fakeProc)
        == viper_gui_exit::StopResult::Signalled);
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM);
    assert(viper_gui_exit::stopMarkedBackend(child, error, fakeProc)
        == viper_gui_exit::StopResult::AlreadyStopped);
    return 0;
}
'''
with TemporaryDirectory(prefix="viper-gui-exit-") as dirname:
    directory = Path(dirname)
    cpp = directory / "check.cpp"
    executable = directory / "check"
    fixture = directory / "fake-proc"
    fixture.mkdir()
    cpp.write_text(source)
    subprocess.run(
        [os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra",
         "-Werror", "-I", str(root / "src"), str(cpp), "-o", str(executable)],
        check=True, timeout=40)
    subprocess.run([str(executable), str(fixture)], check=True, timeout=10)
print("PASS: GUI backend exit choices, preferences and isolated pidfd stop guard")
