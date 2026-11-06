#!/usr/bin/env python3
# @author: Ahmad Odeh
# @date: 6th Nov, 2026
#
# test_launcher: the `photon` command behaves like VS Code's `code`.
#
#   photon FILE        returns within a second, prints nothing, and the
#                      window it starts is in its own session (closing the
#                      terminal can't kill it)
#   photon FILE2       goes to the same window (one Photon), paths absolute
#   photon -w FILE     blocks until FILE's tab is closed
#   --version / --help print and exit
#
# Runs against ./Photon in a throwaway runtime/cache/config folder, so it
# never talks to a Photon you have open. Needs a display (Linux CI: xvfb-run).
# Usage: python3 tests/test_launcher.py ./Photon

import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

PHOTON = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./Photon")
failures = 0
checks = 0


def check(ok, what):
    global failures, checks
    checks += 1
    if not ok:
        failures += 1
        print(f"FAIL: {what}", file=sys.stderr)


def launch(args, cwd, env, timeout=10):
    """Run the launcher; return (seconds, exit code, stdout, stderr)."""
    start = time.monotonic()
    done = subprocess.run([PHOTON] + args, cwd=cwd, env=env, capture_output=True, timeout=timeout)
    return time.monotonic() - start, done.returncode, done.stdout.decode(), done.stderr.decode()


def wait_for(condition, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.1)
    return False


def send(sock_path, line):
    s = socket.socket(socket.AF_UNIX)
    s.connect(sock_path)
    s.sendall(line.encode() + b"\n")
    s.close()


def instance_pids(marker):
    """PIDs of Photon windows started from our temp folder (their argv holds its paths)."""
    out = subprocess.run(["pgrep", "-f", "--", f"--detached.*{marker}"], capture_output=True).stdout.decode()
    return [int(p) for p in out.split()]


def main():
    # Short (a socket path must fit in ~100 bytes) and resolved: on macOS
    # /var is a link to /private/var, and Photon's argv holds the real path
    tmp = os.path.realpath(tempfile.mkdtemp(prefix="ph"))
    env = dict(os.environ)
    env["PHOTON_RUNTIME_DIR"] = os.path.join(tmp, "rt")
    env["XDG_CACHE_HOME"] = os.path.join(tmp, "cache")
    env["XDG_CONFIG_HOME"] = os.path.join(tmp, "config")
    os.makedirs(env["PHOTON_RUNTIME_DIR"], mode=0o700)
    sock = os.path.join(env["PHOTON_RUNTIME_DIR"], "photon.sock")
    for name, text in [("a.lunar", "print 1;\n"), ("b.py", "print(2)\n"), ("MSG", "commit message\n")]:
        with open(os.path.join(tmp, name), "w") as f:
            f.write(text)
    a, b, msg = (os.path.realpath(os.path.join(tmp, n)) for n in ("a.lunar", "b.py", "MSG"))

    try:
        # 1. Returns at once, silently, and the window comes up on its own
        seconds, code, out, err = launch(["a.lunar"], tmp, env)          # relative path
        check(seconds < 1.0, f"photon FILE returned in {seconds:.2f}s (want < 1s)")
        check(code == 0, f"photon FILE exit code {code}")
        check(out == "" and err == "", f"photon FILE printed {out!r} {err!r}")
        check(wait_for(lambda: os.path.exists(sock), 20), "the new Photon never opened its socket")

        pids = instance_pids(tmp)
        check(len(pids) == 1, f"expected one Photon window, found {pids}")
        if pids:
            check(os.getsid(pids[0]) != os.getsid(0), "Photon shares the terminal's session (would die with it)")

        # 2. A second file goes to the same Photon
        seconds, code, out, err = launch(["b.py"], tmp, env)
        check(seconds < 1.0, f"second photon FILE returned in {seconds:.2f}s")
        check(code == 0 and out == "" and err == "", f"second launch: {code} {out!r} {err!r}")
        time.sleep(0.5)
        check(len(instance_pids(tmp)) == 1, "a second launch started another Photon")
        _, _, status, _ = launch(["--status"], tmp, env)
        open_files = status.splitlines()
        check(a in open_files and b in open_files, f"--status lists {open_files}, want {a} and {b} (absolute)")

        # 3. -w blocks until its tab closes
        waiter = subprocess.Popen([PHOTON, "-w", "MSG"], cwd=tmp, env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(1.5)
        check(waiter.poll() is None, "photon -w returned before its tab was closed")
        send(sock, f"close {msg}")                                       # what the tab's x does
        try:
            waiter.wait(timeout=5)
            check(waiter.returncode == 0, f"photon -w exit code {waiter.returncode}")
        except subprocess.TimeoutExpired:
            waiter.kill()
            check(False, "photon -w still waiting after its tab was closed")

        # 4. --version / --help print and exit normally
        seconds, code, out, _ = launch(["--version"], tmp, env)
        check(code == 0 and out.startswith("Photon "), f"--version: {code} {out!r}")
        _, code, out, _ = launch(["--help"], tmp, env)
        check(code == 0 and "Usage: photon" in out, f"--help: {code} {out[:40]!r}")
        _, code, _, err = launch(["-w"], tmp, env)
        check(code == 2 and "-w needs a file" in err, f"-w without a file: {code} {err!r}")
    finally:
        for pid in instance_pids(tmp):
            os.kill(pid, 15)
        shutil.rmtree(tmp, ignore_errors=True)

    print(f"test_launcher: {checks} checks, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
