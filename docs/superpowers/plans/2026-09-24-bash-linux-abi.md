# Bash Linux ABI / POSIX 兼容实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 subagent-driven-development（推荐）或 executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 让未修改的 Alpine `bash-5.3.9-r1` 在 LeonOS x86_64/musl 环境中稳定运行非交互脚本，并让 `/bin/bash -i` 在真实 PTY 中具备可验证的 readline、信号和 job-control 行为。

**架构：** 以 Linux v6.12 的进程组、会话、TTY、等待状态和信号契约为行为基线，补齐 NTCLKS 现有 syscall 后端的 POSIX 语义；不修改 Bash、musl 或 Alpine 包。通过原始 syscall C 探针、Bash 黑盒脚本和 PTY 集成测试驱动每一层，最后用 QEMU 来宾验收未修改 Bash。

**技术栈：** C11 freestanding kernel、现有 NTCLKS scheduler/PTY/signal/procfs、x86_64 `syscall` ABI、Alpine musl Bash 5.3.9、Python QEMU 测试驱动、Linux v6.12 源码行为参考。

**规格：** 本计划实现用户需求“补一些 Linux ABI / POSIX，目标是能跑 bash”。论证依据来自 `docs/APK_PREPARATION.md:188-196` 的现有 Bash 验收、`docs/SYSCALLS.md`、`docs/LINUX_ABI_PROGRESS_2026-09-08.md`、`docs/ABI_MIGRATION.md`，以及本计划“参考边界”列出的 Linux v6.12 和 Context7 文档。执行者必须同时阅读这些材料和本计划。

## 全局约束

- 目标 ABI 是 Linux v6.12 x86_64 原生 `syscall`，寄存器为 `rax/rdi/rsi/rdx/r10/r8/r9`，错误返回负 errno。
- Bash、musl、readline、ncurses 和 Alpine 包均保持上游二进制/源码不变；本计划只修改 LeonOS 内核、UAPI、测试和文档。
- Linux v6.12 源码和 Bash 源码只用于观察错误码、状态转换、锁/生命周期和可见结果；不得将 GPL/Linux/Bash 代码复制到 Apache-2.0 仓库。实现必须是基于行为契约的原创代码。
- 不能用“编译通过”替代来宾验收。每个 ABI 任务至少有一个主机测试和一个 LeonOS/QEMU 测试；TTY/job-control 任务必须在 PTY 中验证。
- 复用 `kernel/ntclks` 现有 `task`、`task_file`、`pty_session`、`sched_wait_reap`、`kernel_signal_queue` 和 `procfs` 数据结构；不在本计划中做无关重构。
- 所有新增内核函数遵循 `AGENT.md` 的 Doxygen 注释要求；用户指针、长度、锁、信号和进程生命周期必须写入注释。
- QEMU/KVM 通过不等于 VMware 通过。本计划只声明 QEMU 证据，VMware 验收另行记录。
- 测试输出必须区分 `PASS`、`FAIL`、`SKIP`；不得为了通过而放宽 Bash 结果断言。

## 范围

### 本计划必须交付

1. Bash 非交互兼容：变量、函数、数组、算术、条件、循环、参数展开、命令替换、进程替换、管道、重定向、trap、`read`、`wait`、`ulimit`、`times`。
2. Bash 交互兼容：PTY 中的 prompt、readline 输入、行编辑、SIGINT/SIGTSTP/SIGCONT、`jobs`、`fg`、`bg`、`wait`。
3. 支撑 Bash 的 Linux/POSIX 语义：进程组/会话、控制终端、前台进程组、wait 状态、SIGCHLD、信号 mask/sigsuspend、termios、`/dev/fd`/`/proc/self/fd`。
4. 原始 syscall 回归，而不是只测 libc 包装器。

### 明确不在本计划内

- IA32 `int 0x80`、x32、glibc loader、完整任意 Linux 二进制兼容。
- Bash 自带完整 testsuite 的所有历史边界、国际化、所有 readline 终端类型。
- Linux namespace、seccomp、Landlock、io_uring、BPF、swap、kexec。
- ALSA/DRM、IPv6/UDP 完整网络栈；Bash 只要求已有文件/进程/PTY 路径工作。
- 任何 Bash/musl/Alpine 源码 patch。

## 参考边界

### Linux v6.12 行为参考

源码归档是 `build/linux-6.12.tar.xz`。执行者可以只读解压到临时目录，但不得把参考实现复制进仓库。重点观察：

- `kernel/sys.c`：`setpgid`、`setsid`、`getpgid`、`getsid` 的 leader/parent/child race、EPERM/ESRCH 规则。
- `kernel/exit.c`：`wait_task_stopped`、`wait_task_continued`、`do_notify_parent` 的 `WUNTRACED`、`WCONTINUED`、`WNOWAIT` 和 siginfo/status 编码。
- `kernel/signal.c`：`do_sigaction`、`do_sigtimedwait`、`do_sigpending`、`sigsuspend`、`sigaltstack`、`prepare_signal`、`complete_signal` 的阻塞、排队、默认处置和原子 mask 语义。
- `drivers/tty/tty_jobctrl.c`：`__tty_check_change`、`proc_set_tty`、`tiocspgrp`、`tiocgpgrp`、`tiocsctty`、`tiocnotty`、`disassociate_ctty` 的 session/foreground/orphan 约束。
- `drivers/tty/tty_ioctl.c`、`drivers/tty/tty_io.c`：termios、`TIOCGWINSZ`/`TIOCSWINSZ`、hangup、`SIGTTOU`/`SIGTTIN`。
- `fs/fcntl.c`：`F_DUPFD`、`F_DUPFD_CLOEXEC`、`F_GETFD`/`F_SETFD`、`F_GETFL`/`F_SETFL` 的 descriptor-local 与 open-description 状态边界。
- `fs/select.c`、`fs/read_write.c`：`poll`/`select` 的 EINTR、超时、部分 I/O 和 signal-mask 关系。
- `fs/proc/base.c`、`fs/proc/fd.c`：`/proc/self/fd`、`/proc/PID/fd` 的 magic-link、权限和生命周期。

### Context7 文档参考

- GNU Bash 手册：`/websites/gnu_software_bash_manual_html_node`
  - Job Control Basics、Signals、Bash Startup Files、The Set Builtin。
  - 重点契约：job-control 下 shell 与前台命令不在同一 process group；`set -m` 创建独立 process group；`SIGINT`/`SIGTSTP`/`SIGCONT` 和 `wait` 状态必须一致。
- Bash 源码索引：`/bminor/bash`
  - 只用来确认 Bash 的 job table、`pgrp`、`SIGCHLD`、`wait` 和启动/信号路径调用面，不复制其中代码。
- Context7 查询记录应作为实现者的行为来源之一，不作为代码许可证来源。

### LeonOS 现有实现入口

- `kernel/ntclks/syscall_process.c:129-230`：signal syscall 入口。
- `kernel/ntclks/syscall_process.c:937-951`：`getpgrp/getpgid/getsid/setpgid/setsid` 当前入口。
- `kernel/ntclks/sched/sched.c:826-930`：clone、共享资源和 task 拷贝。
- `kernel/ntclks/sched/sched.c:3074-3150`：wait/reap 和 stopped/continued 事件。
- `kernel/ntclks/pty.c:973-1075`：前台 pgrp、控制终端和 session exit。
- `kernel/ntclks/syscall.c:6521-6576`：`wait4`/`waitid`。
- `kernel/ntclks/syscall.c:6840-6910`：PTY `TIOCGPGRP/TIOCSPGRP/TIOCGWINSZ/termios`。
- `kernel/ntclks/procfs.c:440-730`：当前 `/proc` 与 `/proc/*/fd` 子集。
- `tools/tests/apk_guest_probe.c:129-149`：当前只验证 Bash 数组表达式，必须扩展为黑盒 Bash 矩阵。
- `tools/tests/terminal_session_test.c`：已有 PTY shell、输入和输出测试，可扩展交互 job-control 场景。

## 文件结构

### 新建

- `tools/tests/bash_abi_test.c`
  - 原始 syscall/POSIX 测试；覆盖 process group、wait status、signal mask、sigaltstack、fcntl、poll、proc fd 和 termios。主机和来宾都可编译。
- `tools/tests/bash_guest_test.c`
  - 来宾黑盒 Bash 测试入口；创建临时脚本、启动 `/bin/bash`、断言 stdout/stderr/status 和 Bash 退出码。
- `tools/tests/bash_job_control_test.c`
  - PTY 集成测试；启动交互 Bash、注入按键、验证 prompt、Ctrl-C、Ctrl-Z、`jobs/fg/bg`。
- `tools/test_bash_qemu.py`
  - 构建/启动 QEMU、运行 Bash 测试、保存串口和 QMP 证据；不得复制现有 APK 测试的网络/包管理断言。

### 修改

- `kernel/ntclks/include/ntclks/sched.h`
  - 扩展 process/session、stopped/continued child event 和 controlling-pty 生命周期所需的公开内核接口。
- `kernel/ntclks/sched/sched.c`
  - 实现/修正 `setpgid`、`setsid`、group membership、orphaned pgrp、child stop/continue 状态和 wait event 转换。
- `kernel/ntclks/syscall_process.c`
  - 让 `getpgrp/getpgid/getsid/setpgid/setsid`、signal mask、`rt_sigsuspend`、`sigaltstack` 返回 Linux 可见语义。
- `kernel/ntclks/signal.c`
  - 补齐 SIGCHLD stop/continue 通知、siginfo 和 handler/frame 的 Bash 所需路径。
- `kernel/ntclks/signal_queue.c`
  - 修正 pending/blocked、SIGCHLD、`SA_NOCLDSTOP`、queue delivery 和 signal coalescing 边界。
- `kernel/ntclks/include/ntclks/pty.h`、`kernel/ntclks/pty.c`
  - 补齐 controlling tty、foreground pgrp、`SIGTTIN/SIGTTOU`、hangup、master/slave termios 和 `VMIN/VTIME` 语义。
- `kernel/ntclks/syscall.c`
  - 修正 `wait4/waitid`、PTY ioctl 分发、signal-interrupt 交互和 descriptor fd 行为。
- `kernel/ntclks/procfs.c`
  - 补齐 Bash process substitution 所需的 `/dev/fd` 或 `/proc/self/fd` magic-link 行为、权限和 unlink/open 生命周期。
- `include/uapi/linux/tty.h`、`include/uapi/linux/termios.h`、`include/uapi/linux/resource.h`
  - 只补 Bash 实际使用的常量、结构和大小；不得重新定义 Linux ABI。
- `tools/tests/apk_guest_probe.c`
  - 调用 `bash_guest_test()` 和 `bash_job_control_test()`，保留原有 APK/HyFetch 断言。
- `tools/test_apk_qemu.py`
  - 将新 Bash 测试加入已有 guest probe 生命周期，保留包管理和 `MAP_STACK` 证据。
- `docs/SYSCALLS.md`
  - 增加 Bash 所需 syscall/TTY/wait/signal 契约和“已实现/部分实现/未认证”边界。
- `docs/LINUX_ABI_PROGRESS_2026-09-08.md`
  - 追加 Bash 验收检查点、证据路径和剩余边界。
- `docs/APK_PREPARATION.md`
  - 将 Bash 验收从“数组表达式”扩展为非交互、脚本、交互和 job-control 矩阵。

## 任务 1：建立 Bash 黑盒验收 fixture

**文件：**
- 创建：`tools/tests/bash_guest_test.c`
- 修改：`tools/tests/apk_guest_probe.c:129-149`
- 测试：`tools/test_apk_qemu.py`

- [ ] **步骤 1：编写失败的 Bash 场景测试**

```c
/* tools/tests/bash_guest_test.c */
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int bash_guest_test(void)
{
    pid_t child = fork();
    if (child == 0) {
        char *const argv[] = {
            "/bin/bash", "--noprofile", "--norc", "-c",
            "set -e; a=(17 25); test $((a[0]+a[1])) -eq 42; "
            "f(){ printf 'fn:%s' \"$1\"; }; f ok; "
            "x=$(printf sub); test \"$x\" = sub; "
            "printf 'pipe:'; printf abc | tr a-z A-Z",
            NULL
        };
        execve(argv[0], argv, NULL);
        _exit(127);
    }
    int status = 0;
    if (child < 0 || waitpid(child, &status, 0) != child) return 1;
    return !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
```

- [ ] **步骤 2：运行测试验证失败**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：当前 `apk_guest_probe.c` 尚未调用 `bash_guest_test()`，测试报告缺少 `Bash guest matrix`，因此失败。

- [ ] **步骤 3：把 Bash 测试接入现有 guest probe**

```c
/* tools/tests/apk_guest_probe.c:139-140 */
command("Bash execution", 1, "/bin/bash", "-c",
        "a=(17 25); test $((a[0]+a[1])) -eq 42", NULL);
failures += bash_guest_test();
check(failures == 0, "Bash guest matrix");
```

在 `tools/test_apk_qemu.py:24-27` 的 Clang 输入列表增加 `tools/tests/bash_guest_test.c`，不改变 Bash 包本身。

- [ ] **步骤 4：运行测试验证通过**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：串口出现 `[apk-probe] DONE failures=0`，且 Bash 场景的变量、函数、数组、命令替换和管道全部成功。

- [ ] **步骤 5：Commit**

```bash
git add tools/tests/bash_guest_test.c tools/tests/apk_guest_probe.c tools/test_apk_qemu.py
git commit -m "test: add Bash guest compatibility fixture"
```

## 任务 2：实现 Linux 进程组和会话语义

**文件：**
- 修改：`kernel/ntclks/include/ntclks/sched.h`
- 修改：`kernel/ntclks/sched/sched.c:2860-2955`
- 修改：`kernel/ntclks/syscall_process.c:937-951`
- 测试：`tools/tests/process_group_runtime_probe.c`、`tools/tests/bash_abi_test.c`

- [ ] **步骤 1：编写失败的 process-group/session 测试**

```c
static void bash_process_group_abi(void)
{
    pid_t parent = getpid();
    pid_t child = fork();
    if (child == 0) {
        if (setsid() != getpid()) _exit(41);
        if (getpgrp() != getpid()) _exit(42);
        if (setpgid(0, getpid()) != 0) _exit(43);
        if (getsid(0) != getpid()) _exit(44);
        _exit(0);
    }
    int status = 0;
    assert(child > 0 && waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    (void)parent;
}
```

同时增加父子 race：父进程在 child `execve` 前设置 child pgid，子进程启动后读取 `getpgid(0)`；断言两者都能观察同一个 pgid，且 child 只能加入同一 session 的 group。

- [ ] **步骤 2：运行测试验证失败**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：现有 `setsid/setpgid` 路径不能同时满足 session leader、group membership 和父子 race 断言，出现 `bash_process_group_abi` 失败。

- [ ] **步骤 3：实现 Linux 行为的最小状态转换**

在 `sched.h` 增加：

```c
int64_t sched_set_process_group(uint32_t caller_pid, uint32_t target_pid,
                                uint32_t process_group);
int64_t sched_create_process_session(uint32_t caller_pid);
int sched_process_group_is_orphaned(uint32_t process_group);
int sched_process_group_has_session(uint32_t process_group,
                                    uint32_t session_id);
```

在 `sched.c` 中把 group/session 更新集中到一个受 `scheduler_lock` 保护的 helper；只允许 caller 修改自己，或 parent 修改尚未 exec 的 child；拒绝跨 session、leader reassignment、group 0 和不存在的 target。`setsid` 成功时创建新 session、新 pgrp、清除 controlling PTY；失败返回 `EPERM`。`getpgid/getsid` 保持 PID 查询和权限错误顺序与 Linux 一致。

- [ ] **步骤 4：运行测试验证通过**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：`bash_process_group_abi`、父子 race、非法跨 session 操作全部 `PASS`，原有 `process_group_runtime_probe` 不回归。

- [ ] **步骤 5：Commit**

```bash
git add kernel/ntclks/include/ntclks/sched.h kernel/ntclks/sched/sched.c kernel/ntclks/syscall_process.c tools/tests/process_group_runtime_probe.c tools/tests/bash_abi_test.c
git commit -m "feat(abi): implement POSIX process group and session rules"
```

## 任务 3：实现 Bash 所需 wait stop/continue 契约

**文件：**
- 修改：`kernel/ntclks/sched/sched.c:3040-3160`
- 修改：`kernel/ntclks/syscall.c:6521-6576`
- 修改：`kernel/ntclks/signal_queue.c`
- 测试：`tools/tests/wait_runtime_probe.c`、`tools/tests/bash_abi_test.c`

- [ ] **步骤 1：编写失败的 wait 状态测试**

```c
static void bash_wait_status_abi(void)
{
    pid_t child = fork();
    if (child == 0) {
        raise(SIGSTOP);
        _exit(7);
    }
    int status = 0;
    assert(waitpid(child, &status, WUNTRACED) == child);
    assert(WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP);
    assert(kill(child, SIGCONT) == 0);
    assert(waitpid(child, &status, WCONTINUED) == child);
    assert(WIFCONTINUED(status));
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 7);
}
```

增加 `waitpid(-pgid, ..., WUNTRACED|WCONTINUED|WNOHANG)`、`waitid(P_PGID, ...)` 和 `WNOWAIT` 的重复观察测试；验证 stop/continue 事件不会被错误当成 exit，`SIGCHLD` 与 `SA_NOCLDSTOP` 不丢失。

- [ ] **步骤 2：运行测试验证失败**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：`waitpid` 的 stopped/continued 状态、重复事件或 `waitid` siginfo 不满足 Linux 状态编码，测试失败。

- [ ] **步骤 3：实现状态机和事件通知**

把 `TASK_CHILD_EVENT_STOPPED`、`TASK_CHILD_EVENT_CONTINUED` 与 `sched_wait_reap` 的 option 位显式命名，禁止用 magic number；`WNOHANG` 保持不消费事件，`WNOWAIT` 保留事件，其他 wait 调用消费一次事件。`sched_wait_info` 生成 Linux `CLD_STOPPED`/`CLD_CONTINUED`/exit siginfo，`signal_queue.c` 在 `SA_NOCLDSTOP` 未设置时投递 `SIGCHLD`。`wait4` 保留原始 status int 布局。

- [ ] **步骤 4：运行测试验证通过**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：`bash_wait_status_abi` 的 stop、continue、exit、group wait、WNOWAIT 全部 `PASS`；原有 `wait_runtime_probe` 通过。

- [ ] **步骤 5：Commit**

```bash
git add kernel/ntclks/sched/sched.c kernel/ntclks/syscall.c kernel/ntclks/signal_queue.c tools/tests/wait_runtime_probe.c tools/tests/bash_abi_test.c
git commit -m "feat(abi): align child wait states with Linux job control"
```

## 任务 4：实现控制 TTY 和前台进程组

**文件：**
- 修改：`kernel/ntclks/include/ntclks/pty.h`
- 修改：`kernel/ntclks/pty.c:973-1075`
- 修改：`kernel/ntclks/syscall.c:6825-6910`
- 测试：`tools/tests/pty_metadata_runtime_probe.c`、`tools/tests/bash_job_control_test.c`

- [ ] **步骤 1：编写失败的 TTY job-control 测试**

```c
static void bash_tty_job_control_abi(void)
{
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
    char *name = ptsname(master);
    pid_t child = fork();
    if (child == 0) {
        setsid();
        int slave = open(name, O_RDWR);
        assert(slave >= 0);
        assert(ioctl(slave, TIOCSCTTY, 0) == 0);
        pid_t pgrp = getpgrp();
        assert(ioctl(slave, TIOCSPGRP, &pgrp) == 0);
        pid_t observed = 0;
        assert(ioctl(slave, TIOCGPGRP, &observed) == 0);
        assert(observed == pgrp);
        _exit(0);
    }
    int status = 0;
    assert(child > 0 && waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
```

增加非 leader `TIOCSCTTY`、跨 session `TIOCSPGRP`、`TIOCNOTTY`、`TIOCGWINSZ`/`TIOCSWINSZ`、canonical EOF、`VMIN=0/VTIME>0`、`ICANON/ECHO/ISIG` 和 hangup 的错误/信号断言。

- [ ] **步骤 2：运行测试验证失败**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：现有 PTY 实现不能覆盖 Linux 的 session leader、foreground pgrp、`SIGTTIN/SIGTTOU` 和 termios/hangup 组合，测试失败。

- [ ] **步骤 3：实现 TTY session/foreground 状态**

在 `pty_session` 中明确区分 `process_session`、`foreground_pgid`、`tty_old_pgrp` 和 hung-up 状态；复用 `pty_acquire_controlling`、`pty_set_foreground_pgid`、`pty_check_change`，不要另建第二套 job-control 状态。实现 Linux 的：非 session leader `TIOCSCTTY=EPERM`、跨 session `TIOCSPGRP=EPERM`、非前台读写触发 `SIGTTIN/SIGTTOU`、挂断向 session/foreground group 发送 `SIGHUP/SIGCONT`。termios 保存 master/slave 差异，`TIOCGPGRP/TIOCSPGRP` 用 `pid_t` 用户指针。

- [ ] **步骤 4：运行测试验证通过**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：`bash_tty_job_control_abi`、PTY metadata probe 和 terminal session shell 测试全部 `PASS`。

- [ ] **步骤 5：Commit**

```bash
git add kernel/ntclks/include/ntclks/pty.h kernel/ntclks/pty.c kernel/ntclks/syscall.c tools/tests/pty_metadata_runtime_probe.c tools/tests/bash_job_control_test.c
git commit -m "feat(pty): implement Linux controlling-tty job-control ABI"
```

## 任务 5：补齐 Bash 信号、mask 和 sigaltstack 语义

**文件：**
- 修改：`kernel/ntclks/syscall_process.c:129-230`
- 修改：`kernel/ntclks/signal.c:210-350`
- 修改：`kernel/ntclks/signal_queue.c`
- 测试：`tools/tests/signal_queue_abi_test.c`、`tools/tests/signal_info_queue_test.c`、`tools/tests/bash_abi_test.c`

- [ ] **步骤 1：编写失败的 signal-race 测试**

```c
static void bash_signal_mask_abi(void)
{
    sigset_t block, old, pending;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    assert(sigprocmask(SIG_BLOCK, &block, &old) == 0);
    assert(kill(getpid(), SIGUSR1) == 0);
    assert(sigpending(&pending) == 0 && sigismember(&pending, SIGUSR1) == 1);
    sigset_t waitset;
    sigemptyset(&waitset);
    assert(sigsuspend(&waitset) == -1 && errno == EINTR);
    assert(sigprocmask(SIG_SETMASK, &old, NULL) == 0);

    stack_t ss = {.ss_sp = malloc(SIGSTKSZ), .ss_size = SIGSTKSZ};
    stack_t old_ss;
    assert(sigaltstack(&ss, &old_ss) == 0);
    assert(sigaltstack(NULL, &old_ss) == 0 && old_ss.ss_size == SIGSTKSZ);
}
```

增加 SIGCHLD stop/continue、`SA_NOCLDSTOP`、nested handler、`SA_RESTART` 对 `read/poll/wait` 的 EINTR 行为和 signal frame 恢复测试。

- [ ] **步骤 2：运行测试验证失败**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：pending/blocked、`sigsuspend` 原子替换、`sigaltstack` 查询或 SIGCHLD stop/continue 信号与 Linux 不一致，测试失败。

- [ ] **步骤 3：实现信号契约**

把 signal mask 的读改写和临时替换放在同一个任务状态临界区；`rt_sigsuspend` 在睡眠期间原子替换 mask，唤醒后恢复旧 mask，只返回 `EINTR`。`sigaltstack` 遵循 `SS_DISABLE`、最小 `MINSIGSTKSZ`、当前 handler 在 altstack 时的 `EPERM`。信号队列必须保留 siginfo 来源/status，`SA_NOCLDSTOP` 只抑制 stopped/continued 的 SIGCHLD，不抑制 exit 通知。不要复制 Linux 函数体，只实现这些可观测规则。

- [ ] **步骤 4：运行测试验证通过**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：`bash_signal_mask_abi`、signal queue/info、altstack 和 signal-address-space 回归全部 `PASS`。

- [ ] **步骤 5：Commit**

```bash
git add kernel/ntclks/syscall_process.c kernel/ntclks/signal.c kernel/ntclks/signal_queue.c tools/tests/signal_queue_abi_test.c tools/tests/signal_info_queue_test.c tools/tests/bash_abi_test.c
git commit -m "feat(signal): implement Bash signal mask and child notification ABI"
```

## 任务 6：补齐 Bash 使用的文件描述符、proc fd 和资源接口

**文件：**
- 修改：`kernel/ntclks/procfs.c:440-730`
- 修改：`kernel/ntclks/syscall_fs.c`
- 修改：`kernel/ntclks/syscall_process.c:50-117`
- 测试：`tools/tests/procfs_directories_test.c`、`tools/tests/fd_exec_runtime_probe.c`、`tools/tests/bash_abi_test.c`

- [ ] **步骤 1：编写失败的 process-substitution/ulimit 测试**

```c
static void bash_fd_and_limits_abi(void)
{
    int fd = open("/proc/self/fd/1", O_WRONLY);
    assert(fd >= 0);
    char target[256];
    ssize_t n = readlink("/proc/self/fd/1", target, sizeof(target) - 1);
    assert(n > 0);
    close(fd);

    struct rlimit old, requested;
    assert(getrlimit(RLIMIT_NOFILE, &old) == 0);
    requested = old;
    requested.rlim_cur = 64;
    assert(setrlimit(RLIMIT_NOFILE, &requested) == 0);
    assert(getrlimit(RLIMIT_NOFILE, &requested) == 0 && requested.rlim_cur == 64);
    assert(setrlimit(RLIMIT_NOFILE, &old) == 0);
}
```

Bash 黑盒测试增加 `cat <(printf process-substitution)`、`printf x | { read y; printf \"$y\"; }`、`ulimit -n`、`ulimit -c`、`times`，并验证 `/dev/fd` 和 `/proc/self/fd` 在 exec/fork/unlink 后指向正确 open description。

- [ ] **步骤 2：运行测试验证失败**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：Bash process substitution 或 `ulimit`/fd magic-link 路径失败，现有 procfd 只支持只读 link 子集。

- [ ] **步骤 3：实现 fd/proc/resource 契约**

`procfs` 对 `/proc/self/fd/N` 和 `/proc/PID/fd/N` 返回可跟随 magic-link，权限按目标 task 检查，目标 fd 生命周期与 `task_file`/`task_pty_fd` 一致；`readlink` 返回 Linux 风格目标，`open` 能得到同一 open description，不把符号链接内容复制成普通文件。补齐 Bash 使用的 `RLIMIT_NOFILE/AS/STACK/CORE/FSIZE/NPROC/SIGPENDING` 查询；只对已有后端可执行的限制实施 set，其他资源查询必须返回 Linux-shaped 值或明确 errno，不允许静默成功。

- [ ] **步骤 4：运行测试验证通过**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：Bash process substitution、pipe redirection、`ulimit -n/-c`、`times` 和 `/proc/self/fd` 场景全部 `PASS`，fd exec/fork probe 无回归。

- [ ] **步骤 5：Commit**

```bash
git add kernel/ntclks/procfs.c kernel/ntclks/syscall_fs.c kernel/ntclks/syscall_process.c tools/tests/procfs_directories_test.c tools/tests/fd_exec_runtime_probe.c tools/tests/bash_abi_test.c
git commit -m "feat(procfs): expose fd magic-links and Bash resource limits"
```

## 任务 7：验证 Bash 非交互脚本兼容性

**文件：**
- 修改：`tools/tests/bash_guest_test.c`
- 修改：`tools/tests/apk_guest_probe.c`
- 测试：`tools/test_apk_qemu.py`

- [ ] **步骤 1：编写失败的 Bash 非交互矩阵**

在 `bash_guest_test()` 增加一个临时脚本 fixture，至少包含以下命令并断言退出码和输出：

```bash
set -euo pipefail
declare -a xs=(a b c)
test "${xs[1]}" = b
f() { local x=$1; printf 'f=%s' "$x"; }
test "$(f z)" = "f=z"
for i in 1 2 3; do test "$i" -le 3; done
if [[ ${#xs[@]} -eq 3 ]]; then printf 'array-ok'; fi
printf 'redir\n' >"$TMPDIR/redir"
read -r value <"$TMPDIR/redir"
test "$value" = redir
printf 'pipeline\n' | tr 'a-z' 'A-Z' | grep '^PIPELINE$' >/dev/null
printf 'process-substitution\n' | diff -u <(printf 'process-substitution\n') -
trap 'printf trap-ok' EXIT
```

同时覆盖 `command substitution`、函数局部变量、`case`、算术展开、`wait`、`jobs`（在 `set -m` 下）和 `BASH_ENV` 非交互启动文件。

- [ ] **步骤 2：运行测试验证失败**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：当前测试只覆盖数组表达式，新增矩阵中的一个或多个 Bash 能力失败，guest 报告不为 `failures=0`。

- [ ] **步骤 3：仅修复 LeonOS ABI，不改 Bash**

根据失败的原始 syscall/errno 定位任务 2-6 中的缺口；禁止用 Bash wrapper、patched readline 或修改 Alpine package 绕过问题。每次修复只针对测试暴露的 ABI 语义。

- [ ] **步骤 4：运行测试验证通过**

运行：`python3 tools/test_apk_qemu.py --testing-only`
预期：Bash 5.3.9-r1 非交互矩阵全部通过，串口出现 `Bash guest matrix` 和 `[apk-probe] DONE failures=0`。

- [ ] **步骤 5：Commit**

```bash
git add tools/tests/bash_guest_test.c tools/tests/apk_guest_probe.c tools/test_apk_qemu.py
git commit -m "test: certify unmodified Bash noninteractive compatibility"
```

## 任务 8：验证交互 Bash、readline 和 job control

**文件：**
- 修改：`tools/tests/bash_job_control_test.c`
- 修改：`tools/tests/terminal_session_test.c`
- 测试：`tools/test_bash_qemu.py`

- [ ] **步骤 1：编写失败的交互 Bash 场景**

`bash_job_control_test.c` 通过 `terminal_open_session("/bin/bash", ...)` 启动 `bash --noprofile --norc -i`，向 PTY 写入并读取输出，按顺序断言：

1. `echo READY` 后输出 `READY`，prompt 可再次出现。
2. `sleep 30 &` 后 `jobs` 显示 `Running`，命令行回显和 prompt 不被 child 输出破坏。
3. 写入 Ctrl-Z（`0x1a`）后 `jobs` 显示 `Stopped`，`fg` 后 `sleep` 恢复。
4. 写入 Ctrl-C（`0x03`）只结束前台 child，Bash 仍存活并重新显示 prompt。
5. `bg`、`wait`、`exit` 的退出状态为 0，PTY master 读到 EOF。

- [ ] **步骤 2：运行测试验证失败**

运行：`python3 tools/test_bash_qemu.py --testing-only`
预期：当前 terminal test 只验证 `/bin/sh` 基础读写；交互 Bash 的 prompt、Ctrl-Z/Ctrl-C 或 `jobs/fg/bg` 至少一项失败。

- [ ] **步骤 3：补齐 readline 依赖的 TTY/信号交互**

只修复任务 2-6 暴露的 kernel/PTY ABI：termios 的 `ICANON/ISIG/ECHO/IEXTEN`、foreground pgrp 切换、`SIGTSTP/SIGCONT/SIGINT`、`SIGTTIN/SIGTTOU`、`TIOCGWINSZ`、`VMIN/VTIME` 和 hangup。不要修改 Bash 的 readline 或 history 文件。

- [ ] **步骤 4：运行测试验证通过**

运行：`python3 tools/test_bash_qemu.py --testing-only`
预期：交互 Bash 五个阶段全部 `PASS`，Terminal shell 回归不退化，串口无死锁、重复 prompt 或失控 child。

- [ ] **步骤 5：Commit**

```bash
git add tools/tests/bash_job_control_test.c tools/tests/terminal_session_test.c tools/test_bash_qemu.py
git commit -m "test: certify interactive Bash readline and job control"
```

## 任务 9：完成 Linux 源码对读、文档和回归闭环

**文件：**
- 修改：`docs/SYSCALLS.md`
- 修改：`docs/LINUX_ABI_PROGRESS_2026-09-08.md`
- 修改：`docs/APK_PREPARATION.md`
- 测试：`tools/tests/linux_inventory_test.c`、`tools/test_bash_qemu.py`

- [ ] **步骤 1：编写文档契约检查**

在 `tools/tests/linux_inventory_test.c` 增加文本检查，要求 `docs/SYSCALLS.md` 明确列出：

```text
setpgid/setsid/getpgid/getsid
wait4/waitid: WUNTRACED/WCONTINUED/WNOWAIT
TIOCSCTTY/TIOCNOTTY/TIOCGPGRP/TIOCSPGRP
TCGETS/TCSETS/TIOCGWINSZ/TIOCSWINSZ
rt_sigaction/rt_sigprocmask/rt_sigsuspend/sigaltstack
/proc/self/fd and /proc/PID/fd
```

每项必须标注 `implemented`、`partial` 或 `not_verified`，不能只写“支持”。

- [ ] **步骤 2：运行测试验证失败**

运行：`python3 tools/test_bash_qemu.py --testing-only`
预期：文档尚未覆盖 Bash ABI 矩阵，inventory 检查失败。

- [ ] **步骤 3：更新文档并记录源码/Context7 参考**

文档必须引用：

```text
build/linux-6.12.tar.xz:
  kernel/sys.c
  kernel/exit.c
  kernel/signal.c
  drivers/tty/tty_jobctrl.c
  drivers/tty/tty_ioctl.c
  fs/fcntl.c
  fs/select.c
  fs/proc/base.c
  fs/proc/fd.c

Context7:
  /websites/gnu_software_bash_manual_html_node
  /bminor/bash
```

说明这些是行为参考而非代码来源；记录 Bash 5.3.9-r1 的非交互、交互、信号和 job-control 证据文件，并明确 QEMU 不代表 VMware。

- [ ] **步骤 4：运行测试验证通过**

运行：

```bash
make test
python3 tools/test_bash_qemu.py --testing-only
```

预期：主机 contract tests 全部通过，QEMU 报告 Bash 非交互和交互矩阵全部通过，文档契约检查通过。

- [ ] **步骤 5：Commit**

```bash
git add docs/SYSCALLS.md docs/LINUX_ABI_PROGRESS_2026-09-08.md docs/APK_PREPARATION.md tools/tests/linux_inventory_test.c
git commit -m "docs: record Bash Linux ABI coverage and evidence"
```

## 验收矩阵

| 领域 | 必须通过的场景 | 失败时回到 |
| --- | --- | --- |
| 启动/脚本 | `bash -c`、`bash script.sh`、`BASH_ENV`、退出码 | 任务 7 |
| 语法/内建 | 变量、函数、数组、算术、条件、循环、trap、read、ulimit、times | 任务 7 |
| 管道/重定向 | pipe、here-doc、here-string、command substitution、process substitution | 任务 6/7 |
| 进程 | fork/exec、`setpgid`、`setsid`、`wait`、`jobs` | 任务 2/3 |
| 信号 | SIGINT、SIGTSTP、SIGCONT、SIGCHLD、sigprocmask、sigsuspend、sigaltstack | 任务 5 |
| TTY | prompt、readline、termios、winsize、Ctrl-C/Ctrl-Z、foreground pgrp | 任务 4/8 |
| 描述符 | `/proc/self/fd`、`/dev/fd`、pipe fd、CLOEXEC、O_NONBLOCK | 任务 6 |
| 来宾证据 | QEMU serial、guest exit/status、host contract test | 任务 9 |

## 执行顺序和依赖

1. 任务 1 先固定 Bash 黑盒失败面。
2. 任务 2 先建立 process/session 状态，任务 3 才能稳定验证 wait/job 状态。
3. 任务 4 依赖任务 2，任务 5 依赖任务 3；任务 6 可与任务 5 并行，但必须在任务 7 前合入。
4. 任务 7 只接受由任务 2-6 暴露并修复的 ABI 缺口。
5. 任务 8 依赖任务 4-5；不得用修改 Bash 的方式通过交互测试。
6. 任务 9 在所有 guest 证据完成后收口。

## 自检

- 规格覆盖度：本计划覆盖 Bash 非交互、交互、进程组/会话、wait、信号、TTY、proc fd、资源限制和证据文档；未把 networking、namespaces、io_uring、IA32/x32 混入 Bash 必需范围。
- 占位符扫描：任务步骤包含具体测试代码、运行命令、预期结果和 commit 命令；未出现任何禁止占位符模式。
- 类型一致性：统一使用 `sched_set_process_group`、`sched_create_process_session`、`sched_process_group_is_orphaned`、`sched_process_group_has_session`、`bash_guest_test`、`bash_tty_job_control_abi`、`bash_wait_status_abi`、`bash_signal_mask_abi`、`bash_fd_and_limits_abi` 名称。
- 许可证边界：Linux/Bash/Context7 只提供行为观察和测试断言来源，不提供待复制实现；所有内核代码和测试代码均为本仓库原创。
