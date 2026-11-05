#!/usr/bin/env python3
"""Real, headless two-process test of the same Qt guard used by the GUI."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

root = Path(__file__).resolve().parents[1]
gui = (root / "src/viper-gui.cpp").read_text()
cmake = (root / "CMakeLists.txt").read_text()
assert '#include "gui-single-instance.h"' in gui
assert 'guiInstance.claim(waylandActivationToken);' in gui
assert 'instance == ViperGuiInstance::StartResult::Forwarded) {' in gui
assert 'viper_startup::completeForwardedX11(x11StartupId);' in gui
assert 'app.setDesktopFileName("viper4linux");' in gui
assert 'window.activateFromSecondLaunch();' in gui
assert 'void activateFromSecondLaunch() { restoreFromTray(); }' in gui
assert "Qt6::Widgets Qt6::Network" in cmake

with tempfile.TemporaryDirectory(prefix="viper-single-gui-") as temp:
    tmp = Path(temp)
    runtime = tmp / "runtime"
    runtime.mkdir(mode=0o700)
    markers = tmp / "markers"
    markers.mkdir()
    output = tmp / "instance-test"
    flags = subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "Qt6Core", "Qt6Network"], text=True
    ).split()
    subprocess.run(
        [os.environ.get("CXX", "c++"), "-std=c++17", "-fPIC",
         "-Wall", "-Wextra", "-Werror", "-I" + str(root / "src"),
         str(root / "tests/gui-single-instance-harness.cpp"),
         "-o", str(output), *flags], check=True, timeout=40
    )
    env = os.environ.copy()
    env["XDG_RUNTIME_DIR"] = str(runtime)

    def start():
        return subprocess.Popen(
            [str(output), str(markers)], env=env, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

    def wait_for(path, proc=None):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if path.exists():
                return
            if proc is not None and proc.poll() is not None:
                raise AssertionError(f"primary exited prematurely ({proc.returncode})")
            time.sleep(0.04)
        raise AssertionError(f"timed out waiting for {path.name}")

    def second():
        # The real single-instance socket must preserve a new Wayland
        # compositor activation token when the second process exits.
        child_env = env.copy()
        child_env["XDG_ACTIVATION_TOKEN"] = "viper-ci-test-token"
        result = subprocess.run(
            [str(output), str(markers)], env=child_env, capture_output=True,
            text=True, timeout=5
        )
        assert result.returncode == 0, (result.returncode, result.stderr)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            marker = markers / "activated"
            if marker.exists() and b"token=viper-ci-test-token" in marker.read_bytes():
                break
            time.sleep(0.04)
        else:
            raise AssertionError("Second-instance activation token was not forwarded")

    primary = start()
    try:
        wait_for(markers / "ready", primary)
        second()
        assert primary.poll() is None, "second launch terminated the first GUI"
        # SIGKILL leaves both the lock and socket behind; a fresh primary
        # must recover them and still refuse later duplicate windows.
        primary.kill()
        primary.wait(timeout=4)
        (markers / "ready").unlink()
        (markers / "activated").unlink()
        primary = start()
        wait_for(markers / "ready", primary)
        second()
        assert primary.poll() is None
    finally:
        if primary.poll() is None:
            primary.terminate()
            try:
                primary.wait(timeout=3)
            except subprocess.TimeoutExpired:
                primary.kill()
                primary.wait(timeout=3)
        if primary.stdout:
            primary.stdout.close()
        if primary.stderr:
            primary.stderr.close()

print("PASS: one Qt GUI owner, second launch activates it, crash/stale-socket recovery")
