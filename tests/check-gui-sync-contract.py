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
    // One notification per modified group; unchanged values are no-ops.
    std::atomic<std::int32_t> gain{100};
    std::atomic<std::uint32_t> serial{7};
    viper_gui_sync::ChangeGroup unchanged;
    unchanged.set(gain, 100);
    unchanged.notify(serial);
    assert(!unchanged.changed && gain.load() == 100 && serial.load() == 7);
    viper_gui_sync::ChangeGroup edited;
    edited.set(gain, 150);
    edited.set(gain, 150);
    assert(edited.changed && gain.load() == 150);
    edited.notify(serial);
    assert(serial.load() == 8);
    viper_gui_sync::ChangeGroup next;
    next.set(gain, 150);
    next.notify(serial);
    assert(!next.changed && serial.load() == 8);
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
print("PASS: GUI control-channel reconnect, precision and effect change tracking")
