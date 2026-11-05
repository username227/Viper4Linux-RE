#!/usr/bin/env python3
"""Diagnostic smoke test of the actual Qt GUI under virtual X11 or native Wayland.

No launcher/backend is started: only the GUI and its single-instance IPC are tested.
X11: sniff Qt's startup-notification REMOVE messages for injected startup IDs.
Wayland: log native xdg_toplevel.set_app_id, compositor protocol support and
         second-instance forwarding. Fake tokens do NOT prove compositor focus.
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

mode = sys.argv[1]
gui = str(Path(sys.argv[2]).resolve())
logs = Path("startup-diagnostics")
logs.mkdir(exist_ok=True)


def alive(proc):
    if proc.poll() is not None:
        raise AssertionError(f"Primary GUI quit early: {proc.returncode}; inspect gui-primary.log")


def poll(predicate, seconds=12):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        if predicate():
            return True
        time.sleep(0.1)
    return predicate()


def start_observer(identifier):
    observer = subprocess.Popen(
        [str(Path("build/ci-x11-startup-observer").resolve()), identifier, "5"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1
    )
    ready = observer.stdout.readline().strip()
    if ready != "READY":
        raise RuntimeError(f"X11 observer not ready: {ready}")
    return observer


def finish_observer(observer):
    try:
        output = observer.communicate(timeout=6)[0]
    except subprocess.TimeoutExpired:
        observer.kill()
        output = observer.communicate()[0] + "\nMONITOR HUNG"
    return "COMPLETED" in output, output


with tempfile.TemporaryDirectory(prefix="viper-startup-ci-") as isolated:
    env = os.environ.copy()
    env["HOME"] = isolated
    env["XDG_CONFIG_HOME"] = str(Path(isolated) / "config")
    env["XDG_STATE_HOME"] = str(Path(isolated) / "state")
    env["QT_QPA_PLATFORM"] = "xcb" if mode == "x11" else "wayland"
    if mode == "wayland":
        env["WAYLAND_DEBUG"] = "client"
    session_runtime = Path(env["XDG_RUNTIME_DIR"])
    socket_path = session_runtime / "viper4linux-re-gui.sock"
    first_id = "viper-ci-initial_TIME12345"
    env["DESKTOP_STARTUP_ID"] = first_id
    env.pop("XDG_ACTIVATION_TOKEN", None)
    observer1 = start_observer(first_id) if mode == "x11" else None
    primary_log = logs / "gui-primary.log"
    primary_output = primary_log.open("w")
    primary = subprocess.Popen([gui], env=env, stdout=primary_output,
                               stderr=subprocess.STDOUT, start_new_session=True)
    try:
        def primary_ready():
            alive(primary)
            if not socket_path.exists():
                return False
            if mode == "x11":
                r = subprocess.run(["xdotool", "search", "--onlyvisible", "--name", "Viper4Linux"],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=2)
                return r.returncode == 0
            try:
                text = primary_log.read_text(errors="replace")
            except OSError:
                return False
            return bool(re.search(r'set_app_id\("([^"]+)"\)', text))

        if not poll(primary_ready):
            raise AssertionError("GUI did not expose a visible X11 window or native Wayland toplevel")
        print(f"PASS: native {mode} Qt GUI launched; primary GUI owns session socket", flush=True)

        if mode == "wayland":
            debug = primary_log.read_text(errors="replace")
            appids = re.findall(r'set_app_id\("([^"]+)"\)', debug)
            print(f"DIAGNOSTIC: Wayland native window application IDs: {appids}", flush=True)
            print("NOTE: compositor support alone does not validate a real user-issued activation token", flush=True)
        else:
            observed, trace = finish_observer(observer1)
            observer1 = None
            (logs / "x11-cold-start.txt").write_text(trace)
            print(f"DIAGNOSTIC: initial X11 startup REMOVE observed: {observed}", flush=True)

        env2 = env.copy()
        env2["DESKTOP_STARTUP_ID"] = "viper-ci-second_TIME23456"
        if mode == "wayland":
            # Intentionally not a compositor-issued token: only tests that the
            # second launch forwards activation and exits without extra GUI.
            env2["XDG_ACTIVATION_TOKEN"] = "viper-ci-not-a-real-token"
        observer2 = start_observer(env2["DESKTOP_STARTUP_ID"]) if mode == "x11" else None
        second_log = logs / "gui-second.log"
        with second_log.open("w") as out:
            result = subprocess.run([gui], env=env2, stdout=out,
                                    stderr=subprocess.STDOUT, timeout=12)
        if result.returncode != 0:
            raise AssertionError(f"Second GUI returned {result.returncode}")
        alive(primary)
        print("PASS: existing-window activation forwarded; no second GUI process remains", flush=True)
        if observer2 is not None:
            observed, trace = finish_observer(observer2)
            (logs / "x11-second-launch.txt").write_text(trace)
            print(f"DIAGNOSTIC: second X11 startup REMOVE observed: {observed}", flush=True)
        print("NOTE: no real desktop launcher, Cinnamon spinner, GNOME Shell, or KWin session was exercised", flush=True)
    finally:
        if observer1 is not None and observer1.poll() is None:
            observer1.kill()
            observer1.communicate(timeout=2)
        primary.terminate()
        try:
            primary.wait(timeout=5)
        except subprocess.TimeoutExpired:
            primary.kill()
            primary.wait(timeout=5)
        primary_output.close()
