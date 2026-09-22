#!/usr/bin/env python3
"""Exercise real POSIX PTYs, including PAM's setsid + stdout-pipe topology.
Usage: python3 tests/integration/test-motd.py /path/to/host-runnable/motd
"""
import datetime
import fcntl
import os
from pathlib import Path
import pty
import struct
import subprocess
import sys
import termios
import unicodedata

binary = Path(sys.argv[1]).resolve()
master, slave = pty.openpty()
try:
    for width in (20, 40, 80, 120):
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 24, width, 0, 0))
        for language in ('en_US.UTF-8', 'zh_CN.UTF-8'):
            env = dict(os.environ, LANG=language, LC_ALL='', LC_MESSAGES='',
                       PAM_TTY=os.ttyname(slave), TZ='UTC0', COLUMNS='999')
            before = datetime.datetime.now(datetime.timezone.utc)
            output = subprocess.check_output([binary], env=env, stdin=subprocess.DEVNULL,
                                             start_new_session=True, text=True)
            after = datetime.datetime.now(datetime.timezone.utc)
            assert output.startswith('欢迎使用 ' if language.startswith('zh') else 'Welcome to ')
            for line in output.splitlines():
                cells = sum(0 if unicodedata.combining(c) else
                            2 if unicodedata.east_asian_width(c) in ('W', 'F') else 1
                            for c in line)
                assert cells <= width - 1, (width, line, cells)
            if width >= 80:
                assert os.uname().release in output
                assert os.uname().machine in output
                stamp = output.splitlines()[1].split('：')[-1].removeprefix('System information as of ')
                observed = datetime.datetime.strptime(stamp, '%Y-%m-%d %H:%M:%S UTC').replace(tzinfo=datetime.timezone.utc)
                assert before - datetime.timedelta(seconds=1) <= observed <= after
                assert ('内存:' if language.startswith('zh') else 'Memory:') in output
    env.update(LANG='zh_CN.UTF-8', LC_MESSAGES='en_US.UTF-8')
    assert subprocess.check_output([binary], env=env, text=True).startswith('Welcome to ')
    env['LC_ALL'] = 'zh_CN.UTF-8'
    assert subprocess.check_output([binary], env=env, text=True).startswith('欢迎使用 ')
finally:
    os.close(slave)
    os.close(master)
print('motd PTY: 20/40/80/120 columns, PAM pipe, current uname/time and locale precedence passed')
