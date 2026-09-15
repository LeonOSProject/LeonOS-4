#!/usr/bin/env python3
"""Exercise the packaged upstream OpenRC in a disposable Linux PID namespace.

This is a reference test, not an NTCLKS acceptance result. All service fixtures
and runlevels live only in the temporary root. Never invoke host rc-service.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "build/apk/root"
with tempfile.TemporaryDirectory(prefix="leonos-openrc-") as directory:
    root = Path(directory)
    for name in ("bin", "sbin", "etc", "usr/libexec/rc"):
        shutil.copytree(SOURCE / name, root / name, symlinks=True)
    for name in ("lib", "usr/lib"):
        (root / name).mkdir(parents=True, exist_ok=True)
        for path in (SOURCE / name).iterdir():
            if ".so" in path.name:
                target = root / name / path.name
                if path.is_symlink():
                    target.symlink_to(os.readlink(path))
                elif path.is_file():
                    shutil.copy2(path, target)
    for name in ("usr/bin", "proc", "sys", "dev", "run", "var/log", "var/tmp", "tmp", "root"):
        (root / name).mkdir(parents=True, exist_ok=True)
    (root / "var/run").symlink_to("../run")
    # Install the actual built applet table in the private fixture, including
    # /usr/bin tools that upstream OpenRC's shell scripts use.
    for applet in subprocess.check_output([SOURCE / "bin/busybox", "--list"], text=True).splitlines():
        path = root / "usr/bin" / applet
        if not path.exists():
            path.symlink_to("../../bin/busybox")
    shutil.rmtree(root / "etc/runlevels")
    for level in ("sysinit", "boot", "default", "shutdown"):
        (root / "etc/runlevels" / level).mkdir(parents=True)
    (root / "etc/group").write_text("root:x:0:\nwheel:x:10:\nuucp:x:14:\n")
    (root / "dev/null").touch()
    (root / "etc/inittab").write_text(
        "::sysinit:/sbin/openrc sysinit\n::wait:/sbin/openrc boot\n"
        "::wait:/bin/sh /verify\n::shutdown:/sbin/openrc shutdown\n")
    def script(name, text):
        path = root / name
        path.write_text(text)
        path.chmod(0o755)
    script("etc/init.d/probe", """#!/sbin/openrc-run
command=/bin/sleep
command_args=120
supervisor=supervise-daemon
respawn_delay=1
respawn_max=2
respawn_period=30
retry=TERM/2/KILL/2
""")
    script("etc/init.d/reject", "#!/sbin/openrc-run\nstart() { return 7; }\n")
    script("etc/init.d/dependent", "#!/sbin/openrc-run\ndepend() { need reject; }\nstart() { touch /wrong; }\n")
    script("verify", """#!/bin/sh
export PATH=/usr/sbin:/usr/bin:/sbin:/bin RC_NOCOLOR=YES
fail() { echo "FAIL $*" > /result; reboot; exit 1; }
rc-update add probe default || fail enable
rc-update show default | grep -q probe || fail enabled-state
openrc default || fail default
rc-service probe status || fail running
rc-status default > /status-evidence
first=$(cat /run/supervise-probe.pid) || fail supervisor-pid
child=$(cat /proc/$first/task/$first/children) || fail child-pid
kill -9 $child || fail crash
sleep 3
rc-service probe status || fail recovery
next=$(cat /proc/$first/task/$first/children) || fail replacement-pid
test "$child" != "$next" || fail no-restart
rc-service probe restart || fail restart
rc-service probe stop || fail stop
rc-service probe status && fail still-running
rc-update del probe default || fail disable
test ! -e /etc/runlevels/default/probe || fail still-enabled
rc-service dependent start && fail dependency-accepted
test ! -e /wrong || fail dependent-executed
echo PASS > /result
reboot
""")
    result = subprocess.run(["unshare", "-Urmpf", "--mount-proc=" + str(root / "proc"),
                             "chroot", root, "/sbin/init"], capture_output=True,
                            text=True, timeout=45)
    print(result.stdout, result.stderr)
    assert (root / "result").read_text().strip() == "PASS"
    print((root / "status-evidence").read_text())
    print("PASS upstream packaged OpenRC start/stop/restart/status, runlevels, crash recovery and dependency rejection on Linux")
