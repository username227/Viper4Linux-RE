#!/usr/bin/env python3
"""Build isolated GUI IPC/precision policies against a fake mapping."""
from pathlib import Path
from tempfile import TemporaryDirectory
import os
import subprocess

root = Path(__file__).resolve().parents[1]
source = r'''
#include "sync/gui-control-channel.h"
#include <cassert>
struct MockMapping {
    bool alive = false;
    bool can_attach = false;
    int closes = 0;
    int attaches = 0;
    bool current() const { return alive; }
    void close() { ++closes; alive = false; }
    bool attach() { ++attaches; alive = can_attach; return alive; }
};
int main() {
    MockMapping live{true, true};
    assert(viper_gui_sync::ensureCurrent(live));
    assert(live.closes == 0 && live.attaches == 0);
    MockMapping stale{false, true};
    assert(viper_gui_sync::ensureCurrent(stale));
    assert(stale.closes == 1 && stale.attaches == 1 && stale.alive);
    MockMapping offline{false, false};
    assert(!viper_gui_sync::ensureCurrent(offline));
    assert(offline.closes == 1 && offline.attaches == 1 && !offline.alive);
    assert(viper_gui_sync::nativePrecision(false, false, 8024, 8000) == 8024);
    assert(viper_gui_sync::nativePrecision(false, true, 8024, 8000) == 8000);
    assert(viper_gui_sync::nativePrecision(true, false, 537, 550) == 550);
    assert(viper_gui_sync::nativePrecision(false, false, -234, -230) == -234);
}
'''
with TemporaryDirectory(prefix="viper-gui-sync-") as dirname:
    directory = Path(dirname)
    cpp = directory / "sync.cpp"
    executable = directory / "sync-test"
    cpp.write_text(source)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall",
                    "-Wextra", "-Werror", "-I", str(root / "src"),
                    str(cpp), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print("PASS: GUI control-channel reconnect and native precision policies")
