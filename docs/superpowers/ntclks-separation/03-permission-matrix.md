# Desktop/windowd 身份与权限调用矩阵（阶段 0/4）

日期：2026-09-25，基于 `refactor/ntclks-separation@30b1db6` 静态分析（逐行核实，
DEAD=定义但无内核内调用者）。目标：为"会话/测试 autospawn 策略迁出内核"提供
调用矩阵与负例挂钩点；**不删检查、不默认放行**（设计 §6、计划阶段 4）。

## 1. 决策点矩阵

| ID | 决策点（file:line / 函数） | 检查内容 | 授予/动作 | 用户态依赖方 | 归类 | 负例挂钩 |
| --- | --- | --- | --- | --- | --- | --- |
| M1 | `user/userland.c:1146-1159` `userland_exec_current_node` | root 执行者（uid==0&&euid==0）+ 镜像 root 所有且非组/世界可写（`mode & 0022`）+ **路径名**（大小写不敏感全等 `/usr/lib/leonos/apps/desktop/desktop.elf`、`windowd/windowd.elf`、`imd/imd.elf`，userland.c:168-184） | `TASK_FLAG_SERVICE`（desktop 另加 `WINDOW_SERVER`）、记录 desktop/windowd/imd pid | desktop、windowd、imd（杀不掉/挂断/登出免疫 B2-B4、网络可见性 B5、会话身份 B6） | **混合：机制外壳 + 名字策略核心** | 非 root exec 真 desktop/windowd → 无 SERVICE（kill 应成功）；root exec 用户可写副本 → 无授予；**大小写变体路径当前会授予（弱点，先写失败负例）** |
| M2 | `userland.c:1142` | — | exec 只保留 `ELEVATED_ADMIN\|WAITABLE_CHILD`，其余清零 | 全部 | 机制（特权复位） | desktop exec `/bin/sh` 丢 SERVICE/WINDOW_SERVER；`ELEVATED_ADMIN` 跨 exec 保留（待确认是否有意，F4：今日无人置位） |
| M3 | `sched/sched.c:975-983`（clone/fork） | — | 子进程剥离 SERVICE/WINDOW_SERVER | 全部 | 机制 | desktop 的 fork 子进程必须可杀 |
| M4 | `userland.c:475-516` + `421-438` `userland_prepare_exec_credentials` | setuid/setgid 位、`MS_NOSUID`、`no_new_privs`、securebits、cap 集 | euid/egid/caps/AT_SECURE/nondumpable | su/sudo/setuid 程序 | 机制（Linux commoncap 语义） | setuid-root 被用户 exec → AT_SECURE=1、ambient 清、nondumpable；`no_new_privs` 阻 setid |
| M5 | `userland.c:1306-1375` `userland_yield_if_runnable` | `current pid == desktop_pid` | **以 desktop 为父**拉起 autospawn 测试/演示（hello/uidemo/terminal/memtest/linuxabi/ltp/gcc/vim/ioctlcloexec/python315/inventory） | `tools/test_*.py`（经 `autospawn=` cmdline） | **策略——迁用户态** | autospawn 恰好一次且仅在 desktop 下；迁移后父进程/uid/session 变化要显式保留 |
| M6 | `userland.c:986-1004,1011-1020` | `init=` 必须 `/` 开头 | PID 1 路径（默认 `/sbin/init`） | init 链 | 策略（路径）+机制（PID1） | `init=relative` 必须停机 |
| M7 | `userland.c:981-985` | cmdline `installer-session=tui` | 给 PID 1 导出 `LEONOS_INSTALLER_SESSION` | installer UI | 策略 | — |
| M8 | `userland.c:903-932` | — | 建 `/run`、`/run/leonos`、`/dev/shm` 等固定属主/模式 | sessiond、windowd.sock、全部 IPC | 策略（可迁 init） | `/run/leonos` 必须保持 root:0755 |
| M9 | `userland.c:1255-1285` + `sched.c:3258-3298` | `user->uid!=0`、`session_id!=0` | 设 uid/角色；uid==0 给全 cap | 仅死路径（F2） | 机制 | 若复用：`leonos_user_info` 来源必须经 ucred 鉴权 |
| M10 | `syscall.c:1893-1950,4797-4799,4921-4923,5844-5846,6641-6643` | `role==ADMIN` \|\| (`uid==0` && installer root active) | 裸块设备读写、BLKRRPART | installer(live)、diskmgr | 机制 | 非 admin 读 `/dev/disk0` → -EACCES；安装后系统 root（无 installer root）→ -EACCES；**保持 installer 旁路原样** |
| M11 | `syscall_device.c:22-36` | `CAP_SYS_MODULE` | 驱动装载/卸载/重扫 | drvmgr | 机制 | 无 cap → EPERM |
| M12 | `syscall.c:6776-6811` | controlling tty 或 `CAP_SYS_TTY_CONFIG` | VT 切换、KDSETMODE | login.elf、desktop | 机制 | 非本会话进程 `VT_ACTIVATE` → EPERM |
| M13 | `pty.c:1012-1035` | session leader；steal 需 `CAP_SYS_ADMIN` | controlling TTY | shell/terminal | 机制 | steal 无 cap → 失败 |
| M14 | `sched.c:2524-2540/2976-3000/3032-3052` | `TASK_FLAG_SERVICE` | kill/挂断/登出免疫 | windowd、imd、desktop | 机制（**权威来自 M1 名字检查**） | ⚠️ **2026-09-25 负例实测订正**：`kill(2)` 路径（`kernel_signal_queue_task_info`/`LINUX_SYS_KILL`）无服务门禁；`sched_kill_user_task` 唯一调用者是死代码 F3（`auth_kill_session_tasks_for_logout`，syscall.c:2724），`sched_kill_user_tasks_for_pty/_for_logout` 零调用者——"杀服务 → -1"今日仅死代码可达。负例套件以冒充者可杀性 + `/proc` 标志位直接锁定；test_vt_qemu 中 root 可杀 desktop 与此一致 |
| M15 | `net.c:3194-3209` | role/SERVICE/owner_uid | 连接列表可见性 | netctl、taskmgr | 机制 | 普通用户只见自己的 socket |
| M16 | `input.c:483-536` | pid + grab token | evdev 独占 | windowd、apps | 机制（已是所有权制） | ungrab 他人设备 → 无效 |
| M17 | `syscall_socket.c:1087-1107` | SCM_CREDENTIALS 分字段 cap | 真实 ucred | sessiond、windowd 客户端 | 机制 | 伪造 uid → EPERM |
| M18 | `permissions.c:104-115,179,193,397` | 文件名 `LEONACL.SYS` | sidecar 写/chmod/unlink 拒绝 | FAT 元数据 | 机制 | 对 LEONACL.SYS chmod/chown/unlink → EPERM |
| M19 | `permissions.c:158-163` | sysfs basename | product_serial 等 0400 | settings | 弱（名字） | 非 root 读 → EACCES |
| M20 | `procfs.c:347-358` 等 | uid/gid/caps/nondumpable | /proc 可见性 | taskmgr | 机制 | 跨 uid 隐藏 mm 字段 |

## 2. 名字/路径判定（弱检查）与替代

| 弱检查 | 位置 | 弱点 | 替代（现有标准机制） |
| --- | --- | --- | --- |
| Desktop/windowd/imd 身份（M1） | userland.c:1146-1159 + 168-184 | 大小写不敏感字符串等值；ext2 大小写敏感下大小写变体是不同文件仍匹配 | 把权威编码进**镜像文件本身**（root 所有 + set-id 语义已在 A7 执行锁内验证，无 TOCTOU）或文件能力位；路径退出信任判定 |
| `autospawn=` 子串 | userland.c:81-98,944-954 | 子串匹配（`autospawn=helloworld` 会误触发 `hello`） | 迁用户态（init/sessiond 读配置）；无内核安全依赖 |
| `init=` 路径 | userland.c:986-1004 | 仅 cmdline 来源 | 留用户态启动配置 |
| sysfs basename→0400 | permissions.c:158-163 | 名字比较 | 改用每设备元数据存储（`device_metadata`，permissions.c:78-102） |
| rcctl 服务名白名单 | `userland/apps/rcctl/main.c:10-19` | 用户态策略（无妨） | 留用户态 |
| launch.c desktop 名字检查 | `userland/libc/src/launch.c:151-154,972` | 仅单实例 UX | 留用户态 |

**可用于替代的既有机制**（均已内核强制）：POSIX uid/gid/模式+附属组+cap 集
（permissions.c:190-220,411-492）；securebits/no_new_privs/AT_SECURE（M4）；
每设备属主/模式存储（permissions.c:78-102）与 LEONACL.SYS sidecar
（drivers/bootstrap/storage/storage_sidecar.c）；AF_UNIX 内核认证 ucred（M17）——
sessiond/windowd 已在用；PTY owner_pid/session（pty.c:592-697,990-1035）、
GPU 每 pid（gpu.c:41-77）、evdev pid+token（input.c:483-536）；root-owned 0600
marker + flock 会话权威（`userland/libc/src/pam_session.c:51-80`）。

## 3. 用户态现状（会话/服务编排已在用户态的部分）

- `userland/apps/sessiond/main.c:138-260`：**startup/autostart 策略的现任所有者**
  （`/run/leonos/session.sock`；经 ucred + `/run/leonos/session-user` 属主复检鉴权）。
- `userland/libc/src/sessiond_client.c`：旧 `leonos_startup_*` API 的导出层——
  内核 startup 块（F2）已迁用户态的**先例**：策略走了，检查留了。
- `userland/libc/src/pam_session.c`：会话身份 = root-owned 0600 marker + OFD flock；
  apply = 标准 setuid/setgid/setgroups/setrlimit（无内核角色调用）。
- `userland/apps/login/main.c:184-215`：`--graphical-session` 依 B10 设 VT/KDSETMODE
  后 exec desktop.elf；`--installer-shell` 需 `geteuid()==0 && /etc/leonos/installer-runtime`。
- `userland/libc/src/launch.c:223-240`：root+launch_session → `leonos_session_apply()`。
- `userland/apps/gptinit/main.c`：普通 GPT 工具，仅依赖 B8 块设备门禁。
- 测试消费者：`tools/test_*.py` 经 GRUB `autospawn=` 用 M5（test_linux_inventory.py:45、
  prepare_gcc_probe.py:48、test_auth_guest.py:101 等）。

## 4. 死代码（可安全删除候选，删除前逐一复核）

| 块 | 位置 | 状态 |
| --- | --- | --- |
| F1 `require_background_service`、`require_driver_management` | syscall.c:330-366 | DEAD |
| F2 整块内核 startup/autostart 策略（`startup_*`、`startup_dialog_spawn`、`SYSCONFDIALOG_APP_PATH`） | syscall.c:2763-3176 | DEAD（已被 sessiond 取代） |
| F3 auth 会话块（`auth_copy_current_user`、`auth_apply_session_login`、`auth_kill_session_tasks_for_logout`） | syscall.c:2672-2756 | DEAD |
| F4 `TASK_FLAG_ELEVATED_ADMIN` | sched.h:159 | 从未置位（内核 ioctl 已删） |

**后果**：活内核任务 `task->role` 恒为 NONE（sched.c:388,419），M10 的 role 侧现不可达，
实际门禁 = installer-root 旁路。重构必须**逐字保留**该行为，不得"顺手清理"。

## 5. 负例测试清单（阶段 4 先写失败负例的挂钩点）

1. **普通用户不能冒充 windowd/Desktop**——hook `userland.c:1146-1159`：
   (a) 非 root exec 真 desktop/windowd → 无 SERVICE（kill 成功 sched.c:2534、
   网络可见性下降 net.c:3204）；(b) root exec 用户可写副本 → 无授予；
   (c) **大小写变体路径 → 当前会授予（此负例先写、先失败）**；(d) desktop 的 fork 子 → 无 SERVICE。
2. **exec 不继承不应保留的特权**——hook `userland.c:1142` + M4：desktop exec `/bin/sh`
   丢 SERVICE/WINDOW_SERVER；setid 过渡清 ambient+AT_SECURE；no_new_privs 阻提权。
3. **服务免疫边界**——⚠️ 订正（2026-09-25 负例实测）：`sched_kill_user_task`（sched.c:2534）
   的服务免疫仅死代码可达（唯一调用者 F3 块）；`sched_kill_user_tasks_for_pty/_for_logout`
   零调用者。普通 `kill(2)` 无服务门禁，"杀 windowd → -1"不成立。替代设计时以
   B5（网络可见性）与 B6（会话身份）为 SERVICE 标志的**活**消费者，免疫三函数视为待清死代码。
4. **块设备**——syscall.c:1899/1948/4797/4921/5844/6641：非 admin -EACCES；installer-root 旁路仅 uid==0。
5. **SCM_CREDENTIALS 伪造**——syscall_socket.c:1087-1107：伪造 → -EPERM/-ESRCH。
6. **sessiond 跨用户**——sessiond/main.c:170-172,255-258：-EACCES / 断连。
7. **TTY**——syscall.c:6799、pty.c:1024-1025：VT_ACTIVATE/TIOCSCTTY steal 无授权失败。
8. **LEONACL.SYS**——permissions.c:179,193,397：chmod/chown/unlink → EPERM。

## 6. 朴素迁移的风险

1. **M1 是承重的名字检查**：不替换等价的内核锚定权威（root 所有权镜像+root 执行者
   或文件能力）就移动/删除，任何人可获得杀不掉/登出免疫（B2-B4）与全网络可见（B5）；
   反之只留路径匹配、去掉 root-owned/非可写/root-executor 条件，可写镜像即可冒充合成器。
2. **A13 的父子关系是测试契约**：autospawn 以 desktop 为父、继承 uid/session；
   迁 init/sessiond 后父/uid/session 变化会破坏测试假设（含"退出码落串口日志"）。
3. **SERVICE 语义宽于 daemon**：B5（网络可见性）与 B6（窗口服务器保留会话身份）是**活**消费
   者；B2-B4（kill/挂断/登出免疫）经 2026-09-25 负例实测为死代码可达（见 M14 订正），替代
   时按"免疫函数清退"处理而非"复刻"。
4. **角色机制残余但承重磁盘 IO**：M10 当前等价"除 installer-root uid0 外全拒"；
   任何指向用户态决定的 role 的"清理"都会重开裸盘访问。
5. 大小写不敏感路径比较（A2）在 ext2 上可别名。
6. `userland_spawn_path_argv_for_user`（M9）给 uid0 全 cap 且接受调用者提供的
   `leonos_user_info`；今日安全仅因调用者是死代码（F2）。复用时 user 来源必须经 ucred 鉴权。
7. 陈旧注释漂移：`spawn_path_internal_ex` 注释称"拒绝重复实例"（userland.c:679）但内核无此检查。
8. 删死代码（F1-F4）行为中性但要完整；`ELEVATED_ADMIN` 跨 exec 保留（M2）等关系先留注释/测试再删。

**结论**：唯一以名字/路径做安全判定的是 M1（exec 时 SERVICE/WINDOW_SERVER 标记，
有 root-owned 镜像 + root 执行者缓解）；其余已是机制。autospawn（M5）与启动环境策略
（M7/M8）是纯策略，符合 F2 先例可迁用户态——前提是 §5 负例挂在
`userland.c:1142-1160`、`sched.c:975-983/2534`、`syscall.c:1893-1950`。
