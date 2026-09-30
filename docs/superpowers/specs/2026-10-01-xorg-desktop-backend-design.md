# Xorg 桌面后端设计规格

## 目标

为 ReliefOS 增加一个 Kconfig 桌面后端选择，使系统可以在以下两种互斥模式中选择一种：

1. 默认的 ReliefOS Desktop 栈（用户可见名称为 `ReliefOS Desktop + desktopd`，实现由现有的 `windowd`、`desktop.elf` 和 `sessiond` 组成）。
2. Alpine 官方二进制包提供的 Xorg + `xinit` + `xterm` 最小 X11 会话。

默认构建必须保持现有 ReliefOS Desktop 行为不变。Xorg 模式只承诺提供可启动、可登录的最小 X11 会话；现有 ReliefOS 原生 GUI 应用不在本规格内迁移到 X11。

## 背景与已确认事实

- 当前普通系统的图形启动链是 `console-session` → `login.elf --graphical-session` → `desktop.elf`。
- 当前 ReliefOS 图形栈由 `windowd`、`desktop.elf`、`sessiond` 及现有 framebuffer/VT/input ABI 组成；仓库中没有名为 `desktopd` 的独立程序。
- 当前 APK 流程由 `configs/dependencies.lock.json` 提供下载身份，`make fetch` 填充缓存，生产构建只消费已校验缓存。
- 安装器运行时与安装后的系统共用一部分 rootfs staging 输入，但安装器必须继续使用 ReliefOS 原生桌面。
- 内核已经提供面向 `/dev/fb0` 的标准 Linux fbdev ioctl 兼容路径，以及 `/dev/input/event0` 键盘和 `/dev/input/event1` 鼠标的 Linux evdev 事件流；Xorg 接入应优先复用这些接口。

## 范围

### 本次实现包含

- 在顶层 Kconfig 增加互斥桌面后端选择，默认 ReliefOS 后端。
- 将桌面后端选择传递到 rootfs staging 和运行时启动脚本。
- 为 Xorg 后端加入 Alpine 官方 x86_64/musl APK 及完整传递依赖的锁定、签名、所有权和离线 staging。
- 使用 `xf86-video-fbdev` 和 `xf86-input-evdev` 对接现有显示和输入设备。
- 增加 Xorg 配置、Xorg 会话包装脚本和最小 `xterm` 登录会话。
- 在 Xorg 模式下禁用安装后系统的 `reliefos-windowd` 和 `reliefos-session` default runlevel，防止图形资源争用。
- 在安装器运行时强制恢复 ReliefOS 原生图形会话。
- 增加构建、staging、APK、ABI 和 QEMU 启动验证，以及对应文档。

### 本次实现不包含

- 不新增名为 `desktopd` 的守护进程，不重命名现有 `windowd`、`desktop.elf` 或 `sessiond`。
- 不将 ReliefOS 原生 GUI 应用迁移为 X11 客户端。
- 不引入 DRM/KMS、Mesa modesetting、libinput 或 Wayland 后端作为本次 Xorg 的前置条件。
- 不允许普通构建在缺少缓存时隐式联网。
- 不修改 Xorg 上游源码，不为 Xorg 添加 ReliefOS 私有 ABI 分支。

## Kconfig 设计

在顶层 `Kconfig` 增加桌面后端 `choice`：

- `DESKTOP_BACKEND_RELIEFOS`：`ReliefOS Desktop + desktopd`，默认值为 `y`。
- `DESKTOP_BACKEND_XORG`：`Xorg + xinit + xterm`。

两个符号必须互斥。`configs/default.conf` 必须显式保存默认选择 `DESKTOP_BACKEND_RELIEFOS=y`。配置继续由现有 Kconfig front-end、generated autoconf 和 Make 配置链路生成；不在 Makefile、C 源码常量或 rootfs 脚本中维护第二套默认值。

Kconfig help 文案必须明确：

- ReliefOS 选项使用现有 `windowd`、`desktop.elf` 和 `sessiond`。
- Xorg 选项只提供最小 X11 会话，不保证 ReliefOS 原生 GUI 应用在其中运行。
- 两个后端不能同时启动。

## APK 与 staging 设计

### 包来源

Xorg 模式使用 Alpine 官方 x86_64/musl 包。直接需求包为：

- `xorg-server`
- `xorg-server-common`
- `xinit`
- `xterm`
- `xf86-video-fbdev`
- `xf86-input-evdev`
- `xkeyboard-config`
- `font-cursor-misc`
- `font-misc-misc`

实现时必须根据所锁定的 Alpine v3.24 x86_64 APKINDEX 将上述包的完整运行时依赖闭包逐一加入 `configs/dependencies.lock.json`。每个新增 APK 条目必须具有版本、URL、SHA-256、目录、许可证/来源信息，并通过现有 Alpine RSA 签名校验。不得只锁定顶层包而将依赖留给构建时在线解析。

### 选择性安装

- `make fetch` 可以校验并下载 Xorg 后端所需的所有锁定 APK。
- `DESKTOP_BACKEND_RELIEFOS` 构建不得把 Xorg 运行时包安装进最终 rootfs；可保留已验证的下载缓存和上游 staging 输入。
- `DESKTOP_BACKEND_XORG` 构建必须把完整依赖闭包纳入同一个已签名 APK repository 和 APK 数据库，并保留包所有权、依赖和升级信息。
- Xorg 包的选择必须由 Kconfig 派生输入决定，不能通过手工修改 rootfs 或不受追踪的环境变量决定。
- 常规构建继续使用缓存；缺包时必须报告具体依赖和 `make fetch` 修复命令。

### Xorg 配置文件

Xorg 模式的 rootfs 必须包含 `/etc/X11/xorg.conf`，至少声明：

- `fbdev` 视频驱动及 `/dev/fb0`。
- `evdev` 键盘设备 `/dev/input/event0`。
- `evdev` 鼠标设备 `/dev/input/event1`。
- 使用 tty1 作为图形 VT。

配置不得依赖动态 udev 枚举、DRM/KMS 或设备路径猜测。若设备节点或标准 ioctl 不满足官方驱动要求，必须修复对应标准 Linux ABI 或明确阻止构建，不得在包装脚本中静默降级成 ReliefOS 私有接口。

## 运行时设计

### 后端标记

rootfs staging 生成 `/etc/reliefos/desktop-backend`，内容严格为单行 `reliefos` 或 `xorg`。读取方必须使用精确值匹配，不得把该文件当作 shell 脚本执行。

### 普通系统启动

`system/rootfs/usr/lib/reliefos/console-session` 在 tty1 的图形会话路径按以下顺序工作：

1. 如果存在 `/etc/reliefos/installer-runtime`，无条件走现有 ReliefOS 原生桌面路径。
2. 如果后端为 `reliefos`，保持现有 `login.elf --graphical-session` 行为。
3. 如果后端为 `xorg`，执行新的 `reliefos-xorg-session`。
4. 对未知或缺失后端值，记录错误并回退到文本登录，不得启动两个图形后端。

`reliefos-xorg-session` 必须：

- 从 tty1 的控制终端启动 `xinit`/`Xorg`。
- 显式使用 `/etc/X11/xorg.conf` 和 vt1。
- 启动一个 X11 client：`xterm` 内执行 `/bin/login`，提供真实登录提示，而不是默认 root shell。
- 将 Xorg 和会话错误分别写入可诊断日志。
- 在 Xorg 退出后恢复文本 VT，并返回可用的文本登录路径。
- 使用 POSIX `/bin/sh` 语法和现有用户态运行时，不引入 Bash 专属语法。

### OpenRC 互斥

- ReliefOS 后端保留现有 `reliefos-windowd`、`reliefos-session` default runlevel 链接。
- Xorg 后端的安装后 rootfs 移除这两个 default runlevel 链接，但保留服务脚本和原生程序，以便默认构建和安装器使用同一套源码输入。
- Xorg 后端不启动 `windowd` 或 `sessiond`；不与 Xorg 共享 VT、`/dev/fb0` 或输入设备。
- 安装器 runtime root 必须显式恢复原生后端标记和 `reliefos-windowd`/`reliefos-session` default runlevel 链接，安装器图形流程不受目标系统后端选择影响。
- 不相关的设备、网络、运行时和时间服务维持现有 OpenRC 行为。

## ABI、权限与错误处理

- Xorg 只消费标准 Linux/POSIX 接口：ELF/musl 动态链接、进程、文件、控制终端、VT、fbdev ioctl、evdev read/poll/ioctl、信号和 Unix 文件权限。
- 不新增 Xorg 专用 syscall 或 ReliefOS 私有 ioctl。
- 若发现缺少标准 Linux ABI，必须按照现有公共头、内核、运行库、导出清单、测试和 ABI 文档的闭环补齐。
- `/dev/fb0` 写入和模式设置必须继续遵守当前图形控制 VT 权限；失去活动 VT 时返回标准错误，不得由用户态无限重试。
- 图形启动失败不得导致 OpenRC 或 tty1 无限快速重启；失败后应能进入文本登录并保留日志。
- Xorg 进程不应以可交互 root shell 作为默认会话；登录由 xterm 内的 `/bin/login` 完成。
- 所有 staging、APK 解包和运行时启动失败都必须清理临时状态，并保留可诊断错误。

## 验收标准

### 配置与构建

- 全新输出目录运行 `make defconfig` 时选择 ReliefOS 后端。
- `make menuconfig` 可以在两个后端间切换，生成的 `.config` 不出现同时启用两个后端的状态。
- ReliefOS 后端的现有组件、OpenRC 链接和启动日志与改动前保持一致。
- Xorg 后端构建只消费已 fetch 的锁定 APK；缺少任一包时失败并提示 `make fetch`。

### Rootfs 与 APK

- ReliefOS rootfs 不包含 Xorg 运行时包，且保留原生图形 runlevel。
- Xorg rootfs 包含完整 Xorg 依赖、`/etc/X11/xorg.conf`、`desktop-backend=xorg` 语义和 `xinit`/`xterm` 启动脚本。
- Xorg rootfs 不启用 `reliefos-windowd` 和 `reliefos-session` default runlevel。
- Installer runtime 无论目标系统后端如何配置，都保留原生 ReliefOS 图形服务和启动路径。
- APK ownership、签名、数据库、依赖和许可证测试通过。

### 运行验证

- 在目标 QEMU 图形环境中，Xorg 能打开 tty1、`/dev/fb0` 和两个 evdev 设备。
- Xorg server 进入运行状态，xterm 显示登录提示，输入可以到达登录程序。
- 退出 Xorg 后 tty1 回到文本模式，tty2～tty6 仍可登录。
- ReliefOS 后端仍能启动原生桌面；切换配置不产生两个图形 server 同时运行的状态。
- 未实现或不兼容的 ABI 必须以明确的失败日志呈现，不能用“构建成功”替代运行成功。

## 文档与验证产物

更新或新增文档必须说明：

- Kconfig 选项和默认值。
- Alpine 包来自锁定的官方二进制仓库，下载入口是 `make fetch`。
- Xorg 模式的最小会话边界，以及 ReliefOS 原生 GUI 应用不在本次迁移范围内。
- QEMU/实际目标环境的验证结果，以及尚未验证的硬件显示驱动范围。

## 设计自检

- 目标、默认行为、两种后端、APK 来源、运行时互斥、安装器例外、ABI 约束和验证标准均有明确章节。
- 没有使用 `TODO`、`待定` 或未定义的后端名称；`desktopd` 已明确映射到现有组件。
- Xorg 的显示、输入、会话、服务和包所有权边界彼此一致，没有要求 Xorg 与 `windowd` 共用图形设备。
- 规格没有承诺现有 ReliefOS GUI 应用在 X11 下工作，也没有把安装器 runtime 与安装后系统混为一谈。
