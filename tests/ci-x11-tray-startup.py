#!/usr/bin/env python3
"""Exercise the cold-start-to-tray path with an actual X11 tray selection.

A fake XEmbed tray host is used only inside Xvfb. DSP/backend are never
started; the real viper-gui is launched with isolated QSettings.
"""
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time

root = Path.cwd()
gui = str((root / "build/viper-gui").resolve())
observer_exe = str((root / "build/ci-x11-startup-observer").resolve())
tray_exe = str((root / "build/ci-x11-tray-host").resolve())
logs = root / "startup-diagnostics"
logs.mkdir(exist_ok=True)

def until(check, seconds=10):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if check():
            return True
        time.sleep(0.05)
    return check()

with tempfile.TemporaryDirectory(prefix="viper-ci-tray-") as home:
    home = Path(home)
    config = home / ".config"
    prefs = config / "ViPERforLinux" / "ViPERClean.conf"
    prefs.parent.mkdir(parents=True)
    prefs.write_text("[ui]\nstartupBehavior=tray\n")
    env = os.environ.copy()
    env.update(
        HOME=str(home), XDG_CONFIG_HOME=str(config), QT_QPA_PLATFORM="xcb",
        DESKTOP_STARTUP_ID="viper-ci-tray-start_TIME34567"
    )
    marker = home / "icon-docked"
    tray = subprocess.Popen([tray_exe, str(marker)], env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    observer = None
    gui_proc = None
    primary_out = None
    try:
        assert tray.stdout is not None
        ready, _, _ = select.select([tray.stdout], [], [], 6)
        if not ready or tray.stdout.readline().strip() != "READY":
            raise AssertionError("X11 tray owner failed to acquire the selection")
        observer = subprocess.Popen([observer_exe, env["DESKTOP_STARTUP_ID"], "7"],
                                    env=env, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
        assert observer.stdout is not None
        ready, _, _ = select.select([observer.stdout], [], [], 6)
        if not ready or observer.stdout.readline().strip() != "READY":
            raise AssertionError("X11 startup-notification observer not ready")
        primary_out = (logs / "gui-tray.log").open("w")
        gui_proc = subprocess.Popen([gui], env=env, stdout=primary_out,
                                    stderr=subprocess.STDOUT, start_new_session=True)

        def gui_and_tray_ready():
            if gui_proc.poll() is not None:
                raise AssertionError(f"GUI quit before tray docking: {gui_proc.returncode}")
            return marker.exists()

        if not until(gui_and_tray_ready, 12):
            raise AssertionError("GUI failed to dock in fake X11 tray; was tray available?")
        # The Qt GUI must remain running rather than spawning a visible window
        # as a fallback. The actual tray docking above confirms hidden mode.
        gui_proc.poll()
        assert gui_proc.returncode is None, "Tray GUI ended unexpectedly"
        print("PASS: Qt GUI started hidden using a live X11 tray selection", flush=True)

        output, _ = observer.communicate(timeout=10)
        (logs / "x11-tray-start.txt").write_text(output)
        if "COMPLETED" not in output:
            raise AssertionError(f"Tray startup lacked X11 REMOVE notification: {output}")
        print("PASS: X11 start-in-tray completed desktop startup notification", flush=True)
    finally:
        if gui_proc is not None and gui_proc.poll() is None:
            gui_proc.terminate()
            try:
                gui_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                gui_proc.kill()
                gui_proc.wait(timeout=5)
        if primary_out is not None:
            primary_out.close()
        if observer is not None and observer.poll() is None:
            observer.kill()
            observer.wait(timeout=3)
        if tray.poll() is None:
            tray.terminate()
            try:
                tray.wait(timeout=3)
            except subprocess.TimeoutExpired:
                tray.kill()
                tray.wait(timeout=3)
        if tray.stdout:
            tray.stdout.close()
