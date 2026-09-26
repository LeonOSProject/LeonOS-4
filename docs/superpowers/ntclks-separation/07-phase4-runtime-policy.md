# 阶段 4 执行记录：运行时策略迁移与权限负例

日期：2026-09-25/26。父仓提交：`10f934c`（负例）、`1cc7549`（矩阵订正）、
`0ee967a`（runtime 改名）、`4c56dd1`（策略迁移）。**内核侧改动在独立 checkout
`/home/leon/build/ntclks-sep/ntclks`（无 git），逐项留痕如下，阶段 5 历史提取时须纳入。**

## 1. 权限负例（先于任何解耦落地）

`tools/test_privilege_negatives.py` + `tools/tests/privilege_negatives_probe.c`（guest
级 QEMU，一次性介质）：12 ok + 1 xfail（M1(c) 大小写变体弱点钉为待替代目标）+ 1
UNVERIFIED（M14 kill 拒绝仅死代码可达——矩阵已订正，SERVICE 活消费者收敛为 B5/B6）。
含正对照（root-owned 精确路径授予 0x4b）防"删检查后空洞通过"。

## 2. 内核 checkout 改动（供阶段 5 提取）

| 文件 | 改动 |
| --- | --- |
| `kernel/ntclks/user/userland.c` | 删 autospawn_* 标志块（11 项，含死解析 vim）、cmdline 解析（944-966）、`userland_yield_if_runnable`（1303-1375）及其**两个**调用点（userland_schedule_from_frame:817、syscall.c `LINUX_SYS_SCHED_YIELD`:6444）、`name_contains`（无残余调用者）、M7 env 导出（979-985，PID1 env 现仅 PATH/HOME/PWD/TERM）；A16 裁剪：`directories[]` 仅 `/run`、`/run/lock`、`/dev/shm`，删 `linux_uts_load_hostname()`；新增 `boot_cmdline` 存储 + `userland_boot_cmdline()`（Doxygen） |
| `kernel/ntclks/include/ntclks/userland.h` | 删 yield 声明；新增 `userland_boot_cmdline` 声明（Doxygen） |
| `kernel/ntclks/procfs.c` | 新增 `/proc/cmdline`：`proc_fill_content` 分支（单行+尾换行，Linux 对等）、`proc_readdir` files[]、`proc_lookup` 可读表 |
| `kernel/ntclks/uts.c` + `include/ntclks/uts.h` | 删 `linux_uts_load_hostname`（唯一调用者已删）及相关 include |

## 3. 父仓改动（4c56dd1）

| 文件 | 改动 |
| --- | --- |
| `userland/apps/desktop/autospawn.c`（新） | 读 `/proc/cmdline` 精确 token 解析（修子串弱点）；映射逐字取自旧内核块（linuxabi 双拉起、inventory/ioctlcloexec 带 argv、python315 argv 逐字）；`leonos_spawn_argv` 为 desktop 子进程（uid/cwd/session 同旧语义）；fd0/1/2 重绑 `/dev/console`（探针输出进串口，同旧空 fd 表回落语义）；`leonos_launch_use_session(0)` latch（安装系统启动期无 PAM 标记者否则 126）；缺失目标负 pid（同旧 failed-lookup 语义）；`[desktop]` 稳定日志前缀 |
| `userland/apps/desktop/desktop_run.c` | 窗口服务器就绪后、login 前调用一次性拉起 |
| `system/rootfs/usr/lib/leonos/console-session` | `LEONOS_INSTALLER_SESSION` env 判定 → `/proc/cmdline` 精确 token grep |
| `system/rootfs/etc/init.d/leonos-runtime` | hostname 初始化 `hostname -F /etc/hostname`（busybox `CONFIG_HOSTNAME=y` 已验证；失败保持默认名） |

## 4. 验证（全绿）

| 项 | 结果 |
| --- | --- |
| 内核 checkout `make all`+`make test` | 0 |
| 父仓 `make all`+`make test`（p4-mig） | 0 |
| guest autospawn 证据 | `[desktop] autospawn hello pid=300` / `ioctlcloexec pid=301`（38 checks PASS、code=0）；`/proc` 证明 PPid=desktop；探针 stdout 在串口 |
| 权限负例（p4-out 与 p4-mig 两镜像） | 均 `ok=12 xfail=1 unverified=1 fail=0` |
| VT 全套（test_vt_qemu） | PASS |
| `test_console_boot_policy` 等既有断言 | 通过（console-session 语义保持） |

## 5. 事实订正与缺口（不夸大）

- `userland_yield_if_runnable` 有**两个**调用点（计划预设一个）；`autospawn=vim`
  为死解析（只写不读），未复活；`leonos_spawn` 不存在，实为 `leonos_spawn_argv`；
  p4-mig 镜像默认 GRUB 模板并不带 autospawn 串（那是诊断 ISO 模板）。
- **python315 无目标物**：本工作区无 `/opt/python/bin/python3.15`（打包现钉
  CPython 3.14.7）；token 按 ENOENT 负 pid 语义化，端到端 python 输出未证。
- 失败形态微差：预检负 pid vs 旧内核查找不到；若 fork 后 exec 失败现为
  子进程 code=127。拉起环境为 `leonos_environment_build` 向量（旧为近空环境）。
- 测试工具遗留（未做）：`tools/test_linux_ioctl_cloexec.py`/`test_linux_inventory.py`
  的证据过滤仍引用已退役的 `[ntclks] …` 内核字符串；其宿主断言不受影响。
