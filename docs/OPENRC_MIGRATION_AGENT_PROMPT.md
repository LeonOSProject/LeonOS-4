# LeonOS4 Alpine 风格 OpenRC 迁移实施要求

本文是待实施规格，不代表功能已经完成。用户已确定采用 Alpine 风格启动体系，并删除 serviced；执行者应直接开展实现、自审和验证。

## 1. 目标与边界

仓库：`/home/xiaobai/Projects/Projects/LeonOS-4`。目标分支：`xiaobai/openrc-support`。开始前核实分支、工作区及适用的 AGENT.md/AGENTS.md，保留其他人的修改。

最终架构为 **BusyBox init 作为 PID 1，OpenRC 统一管理系统服务**。删除旧 init.elf 和 serviced 的生产实现及构建依赖。用户确定的三项要求是：

1. **彻底删除 serviced，不改名保留。服务启动、停止、依赖、监督、状态全部交给 OpenRC。** BusyBox PID 1 保留其上游的初始化、子进程回收和控制台职责，不构成另一套自研服务管理体系。
2. **DHCP/NTP 等优先使用 Alpine 的标准工具，由 OpenRC 启动。** 不以拆出自研 netmand 或复制旧轮询代码作为默认方案。
3. **必须保留的 LeonOS 专用设备、会话功能独立出来，只处理业务，不再管理服务。** 标准工具能承接的功能不再保留重复的自研实现。

删除 serviced 不得导致网络、设备管理、用户会话或现有 GUI 功能丢失。不得另造一个集中式 daemon 或若干自研管理器重新实现它的服务管理职责。

- 由当前 agent 完成，不创建或委派子代理。
- 不修改独立的 ntclks 实验仓库，不擅自 commit、push 或创建 PR。
- 不需要复杂的历史 rootfs 自动迁移，但新构建、安装器运行环境和安装后的系统必须完整一致。
- 保持 Alpine 非 usr-merge 文件布局、现有 PAM/shadow/sudo 权限体系和 APK 归属规则。
- 不借机重写完整 Linux ABI；迁移实际需要的内核缺口必须真实修复。
- 禁止伪成功、空实现、伪造包数据库、删除功能、关闭认证、关闭 TLS/签名验证或修改测试预期掩盖失败。
- 不硬编码开发者代理、下载目录或本机特有环境。允许正常的环境代理配置。

## 2. 已知现状：必须复核后实施

重点阅读：

```text
kernel/ntclks/user/userland.c
kernel/ntclks/kernel.c
kernel/ntclks/sched/sched.c
kernel/ntclks/syscall.c
userland/apps/init/main.c
userland/apps/desktop/services.c
userland/apps/serviced/（包括 netmand、sessiond、devmand）
userland/apps/login/main.c
userland/libc/src/pam_session.c
userland/libc/src/sudo_client.c
userland/busybox/leonos.config
tools/build_busybox.py
tools/apk_distribution.py
tools/apk_ownership.py
tools/make_live_root.py
tools/make_installer_root.py
tools/make_installer_iso.py
tools/make_image.py
configs/apk-ownership.json
system/rootfs/
build.py
```

当前静态观察，不应当作不可更正的事实：

1. init.elf 主要回收子进程、响应电源信号，没有统一编排启动。
2. 内核直接启动 imd/windowd/desktop 和 TTY 登录；installer 有绕过 init 的分支。
3. desktop/services.c 启动、监控和重启 serviced。
4. serviced/main.c 调用 netmand_poll、sessiond_poll、devmand_poll，并管理 DHCP/NTP 等行为及服务配置、状态、命令文件。
5. serviced 中 network_icon、rtc_clock 等项目是界面开关，不是独立守护进程。
6. 部分系统服务启动路径设置 TASK_FLAG_SERVICE、窗口服务标志，标准 exec 路径未必具有相同处理。
7. TTY login 包装器处理控制终端后执行 /bin/login；GUI 走现有 PAM 会话链路。
8. BusyBox 正式构建使用 --official-source，不能仅因脚本包含旧补丁函数就认定生产源码被修改。需要检查最终配置及 applet，确认 init/getty 等真实启用。

实施前形成简明的旧职责→新组件→启动方式→权限→消费者映射，并据源码纠正上述观察。

## 3. PID 1 与内核启动职责

采用 Alpine 默认模式，不把 /sbin/openrc 当作 PID 1，也不默认改用 openrc-init。

```text
NTCLKS → /sbin/init（BusyBox PID 1）
  → openrc sysinit → openrc boot → openrc default
  → 控制台登录及 OpenRC 管理的 LeonOS 服务
  → openrc shutdown → 最终内核电源操作
```

内核负责执行真实 PID 1，提供正确 argv/env/cwd、0/1/2 和可用控制台；明确 init= 覆盖及失败诊断。正常 GUI、TTY、installer 都必须具有明确的 PID 1 模型。保留执行 init 所必需的根挂载和设备初始化，避免循环依赖。

生产路径不得继续由内核硬编码拉起同一批用户态服务。检查 PID 1 orphan reparent、SIGCHLD、wait、僵尸回收、退出和信号语义，以及 fork/exec、进程组、setsid、控制终端和前台进程组。缺口按 Linux 6.12 native x86-64 语义修复，不能假装成功。

不得因启动或认证失败静默进入免密 root shell。显式选择的 live/recovery 环境应有独立政策，不能泄漏到已安装系统。

## 4. 删除 serviced：必须迁移全部实际功能

在删除前逐函数和逐消费者审计 serviced，而非只迁移三个 poll 调用。包括配置写入、DHCP 更新、NTP、DNS 发布、设备事件、会话 IPC、用户自动启动、状态查询和授权检查。

按以下优先级迁移，而不是默认把 serviced 拆成三个自研 daemon：

1. 服务生命周期、依赖、监督和状态移交 OpenRC，删除自研调度、重启、状态数据库和管理主循环。
2. DHCP、NTP 及其他通用系统业务优先选用 Alpine v3.24 的标准工具与服务脚本。核实软件包、实际功能和依赖，给出选择依据，并真实接入构建。例如可以评估 Alpine 使用的 DHCP 客户端、网络配置工具和时间同步工具，但不得未经核实就固定某一工具或同时启用多个提供者。
3. 若标准工具受 NTCLKS ABI 缺口阻碍，优先修复必要的内核行为；不得自动退回旧自研实现并宣称迁移完成。存在具体阻塞时记录失败调用及证据，将对应验收项保留为未完成。
4. 只有标准组件无法承接的 LeonOS 专用设备协议、桌面会话 IPC、用户自动启动等业务才迁入职责独立的组件。逐项解释必要性，优先复用已有合适组件，不强制每个模块都新增常驻进程。

不得把旧 netmand/sessiond/devmand 原封不动搬走就视为完成；必须区分可被标准工具替代的功能和确需保留的专用业务。允许复用必要业务代码，不允许复用旧服务管理层。

每个保留的 LeonOS 专用业务组件必须：

- 只承担自己的业务职责，不充当通用服务管理器。
- 有明确的资源所有权、初始化、信号退出和 socket/文件清理流程。
- 使用必要的最低权限，保留 IPC 对端凭据和操作授权检查。
- 能独立停止、重启，不依赖桌面不断调用维护函数。
- 不自行实现已有标准工具承接的 DHCP/NTP，不重复注册设备或重复启动用户应用。
- 不监督或重启其他系统服务，不维护另一份服务运行状态；需要请求服务操作时只能通过受保护的 OpenRC 接口。用户会话内按用户身份启动应用属于会话业务，不能借此启动 root 系统服务。
- 跨进程依赖使用明确接口，不依赖拆分前的全局变量共享。

会话管理须保留登录/注销竞态防护，按真实用户 UID/GID/补充组执行用户程序；系统 OpenRC 服务不等于用户会话。清理旧服务命令/状态协议及其消费者，界面偏好迁入合适的设置配置，不再伪装成服务状态。

最终删除 serviced 可执行文件、旧主程序、组件条目、APK 文件归属、内核路径特判、桌面自动启动、失效 SDK 常量及测试依赖。迁出的业务源码放到新的明确归属目录。历史文档可以保留，但标注已被替代。禁止只从镜像排除 serviced 而保留失效生产调用链。

## 5. OpenRC 服务与图形服务管理

正式提供 /etc/inittab、/etc/rc.conf、/etc/init.d、/etc/conf.d、/etc/runlevels 和 /run/openrc。根据实际功能配置 sysinit、boot、default、shutdown 及启动模式选择。

- 使用真实的 OpenRC 依赖声明，不用大量固定 sleep 模拟顺序。
- readiness 对应资源/协议确实可用；启动失败返回真实错误。
- 需要监督时选用适当的 OpenRC 机制，设置有限的重启频率，防止崩溃风暴。不能假定 OpenRC 自动监督所有服务。
- stop/restart 正确处理进程、子进程和 PID 复用；不误杀其他进程。
- 不手工伪造 started、softlevel 或运行状态。
- 桌面不再启动、重启系统服务；imd/windowd/desktop 依据真实关系排序。
- 保留现有服务管理 GUI，但服务状态、启用、禁用、启动、停止、重启以 OpenRC 为权威。区分当前运行与加入 runlevel。
- GUI 的授权接口只能执行经过校验的服务操作，不接受任意 shell 文本；普通用户可写文件不能成为 root 命令入口。可保留最小授权桥接组件，但它不得实现第二套服务管理逻辑。
- 不因名字将按需运行的 sudod 授权界面改为常驻 daemon。
- 服务重启及 UI 断线有可恢复行为，不得卡死桌面。

## 6. 权限与登录安全

保持 GUI/TTY PAM、root 与普通用户隔离、sudo/su/wheel、用户环境、会话登录注销及自动启动功能。

重点检查由内核专用 spawn 改为标准 fork/exec 后的服务标志和会话身份。不得让所有 init/OpenRC 后代自动拥有服务特权，也不能凭用户可控制路径、argv 或环境变量赋权。明确特权标志在 exec、fork、降权、注销时的处理。

不得将现有 PAM 登录替换为不经过 PAM 的 BusyBox login。若使用 getty，核对参数和终端行为，使其进入正确的正式登录实现。

## 7. 文件系统、网络与时间

明确 /proc、/sys、/dev、console、PTY、/run、/tmp、/var/tmp、fstab、hostname、hosts、resolv.conf、日志及时间的初始化归属。

运行时状态不能从构建机打包，重启后不能保留假运行状态。临时目录权限正确，/var/tmp 的持久性不应因迁移改变。

DHCP、DNS、HTTPS、NTP 和网络 GUI 的现有功能必须保留。网络超时不能无限阻塞离线登录。使用所选 Alpine 标准工具及其 OpenRC 集成配置网络和同步时间；DHCP、DNS、NTP 每项明确唯一写入/运行权威。网络 GUI 的查询和更新操作应适配这一链路，不能保留旧自研网络管理主循环作为隐藏后备。必要的 LeonOS 授权或状态读取桥接只做协议转换，不重新实现 DHCP/NTP 或服务监督。

不得照搬不存在的 Linux 功能；可选的不支持服务明确不启用，不能假装已挂载 cgroup、启动 udev 或完成 sysctl。对实际必需行为修复内核。

## 8. 软件包与正式构建

以 Alpine v3.24 对应 OpenRC 包/源码为依据，固定版本、来源和校验；优先官方源码及 Alpine 发行版补丁。不要整体切换 edge，也不要私改上游绕过 NTCLKS 缺陷。

核实实际依赖，包括 BusyBox、ifupdown provider、openrc-user 及可选 PAM 包。不要为了满足依赖虚假 provides，或让外来基础包覆盖现有 musl/PAM/shadow/sudo/账户数据。

OpenRC、所选 Alpine DHCP/NTP 等标准工具及必要的 LeonOS 专用业务组件必须进入真实 APK 归属和正式构建。明确 /sbin/init、reboot/poweroff/halt、配置、服务脚本的唯一归属；禁止 --force-overwrite、虚假 installed 数据库、关闭签名或证书验证。保留许可证和补丁来源。

覆盖普通 ISO、VMDK、installer runtime、安装器 /install/root 与 /install/esp payload、安装后系统，以及组件配置、缓存、CI 和清单。镜像内容不能依赖手工修补 staging。增量构建应清理明确归属的旧产物，不得广泛删除未知文件。

## 9. 安装器与关机

区分 live/installer 和已安装系统配置，不能将 installer runlevel 意外安装为默认系统。保留 GUI/TTY 安装、账户创建和安装结束重启。

统一桌面菜单、安装器按钮、命令行电源工具的正常关机入口。按上游 PID 1 协议停止服务、同步并处理卸载，最后调用内核电源操作。避免 shutdown 脚本再次发信号触发自身递归。明确超时和失败诊断，不能在服务未停止时伪报完成。

## 10. 分阶段实施与验证

先核实职责和构建归属，再接入 BusyBox/OpenRC，再以 Alpine 标准工具替换通用业务、迁出必要的专用业务并迁移消费者，最后统一所有启动/关机路径并删除旧生产实现。过渡阶段也不得同时激活两套启动权威。

增加针对真实行为的回归，按风险验证；不只写匹配源码字符串的测试。集中构建和 VM 验证，避免反复生成大镜像。

必须验证：

1. PID 1 确为 BusyBox init，真实进程树、sysinit/boot/default 顺序正确。
2. 标准工具和必要的专用业务组件由 OpenRC 管理；旧 serviced/init.elf 不在生产源码实现、构建归属、镜像和运行链路，不存在改名替身。
3. rc-service 的 start/stop/restart/status、rc-update 和 rc-status 反映实际状态。
4. 启动失败、依赖失败、崩溃重启、PID 复用和子进程退出得到正确处理。
5. GUI/TTY 登录、root/普通用户、账户切换、注销重登、sudo/su、用户自动启动正常；普通用户不能越权管理服务。
6. 输入、窗口、Terminal/PTY/管道、服务管理 GUI 无回归；没有重复服务实例。
7. 离线能登录；联网后 DHCP、DNS、HTTPS/NTP 及网络更新操作正常。提供所选标准工具的软件包、进程/命令及实际租约、DNS 发布、时间同步证据，不能只证明旧网络逻辑仍然可用。
8. 普通 ISO/VMDK、installer GUI/TTY 均可启动；实际安装后从目标磁盘登录。
9. 菜单、安装器、命令行的关机重启实际完成，日志、磁盘同步和 VM 状态提供证据。
10. 一个适合当前内核的 Alpine -openrc 服务包能被正常安装和管理，不能据此宣称全部包兼容。

运行仓库适用的组件、构建和安全回归，更新旧 init 电源测试为新链路的真实检查，不能单纯删测试。构建成功、镜像包含、QEMU 可运行、VMware 可用分别报告。无法验证 VMware 时标为待验证，不冒充成功。

## 11. 自审与交付

完成前审查：是否残留双重管理；是否遗漏 serviced 的业务功能；是否只是改名或拆分保留旧管理层；是否真正采用 Alpine 标准 DHCP/NTP 工具；专用业务组件是否具有不可替代的明确职责；是否有权限泄漏、shell 注入、认证回退；是否有配置归属冲突、假状态、重启风暴、僵尸或关机死锁；是否遗漏安装器 payload 和增量构建旧文件。

提供修改摘要、最终启动/权限关系、旧功能迁移对照表、包版本与归属、实际测试命令/日志、镜像路径/校验值、具体剩余问题和 reviewer 应重点检查的风险。更新文档为实际已实现状态，未通过的验收项逐条列出。不要未经授权 commit/push。

## 12. 一手参考

- Alpine v3.24 inittab：https://github.com/alpinelinux/aports/blob/3.24-stable/main/alpine-baselayout/inittab
- Alpine v3.24 OpenRC 打包：https://github.com/alpinelinux/aports/blob/3.24-stable/main/openrc/APKBUILD
- OpenRC 源码和说明：https://github.com/OpenRC/openrc
- BusyBox init 源码：仓库实际固定的官方 BusyBox 源码 init/ 目录。
- 内核行为：优先仓库已有的 Linux 6.12 固定源码，缺失时查阅官方对应版本。

上游资料用于核对事实，不代表可直接覆盖 LeonOS 的配置、身份体系或包归属。
