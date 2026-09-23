# Console、VT 与会话

系统在启动 PID 1 前创建六个永久终端 `/dev/tty1`～`/dev/tty6`。它们共用 PTY 的 termios、规范输入、前台进程组、控制终端和会话挂断逻辑；动态 `/dev/pts/N` 从 7 开始，不能通过 pts 路径打开固定 VT。`/dev/ttyS0` 是独立串口。系统日志写入串口和日志缓冲，不混入运行中的桌面或文本会话。

BusyBox init 为每个 VT respawn `console-session`。普通系统的 tty1 在存在 `/etc/leonos/desktop-session` 时启动一次桌面；桌面作为 tty1 的 session leader 运行，退出时恢复 KD_TEXT，并进入 getty。删除该标记即可让六个 VT 都启动 getty。桌面不会由 OpenRC 再启动，`LEONOS_BOOT_MODE` 和 `startup=tty|desktop` 已删除。

Ctrl+Alt+F1～F6 切换显示与键盘目标。每个文本 VT 保留最多 64 KiB 输出历史，返回时重放 ANSI 文本；这是有界文本历史，尚不是无限滚动缓冲或完整 Linux VT 仿真。文本输出和 VT 重放在内核 execution transaction 内执行；键盘中断只入队，调度和系统调用入口消费队列，避免中断中进行 framebuffer 重放。

## VT ioctl

请求作用于固定 VT 的 slave 描述符；串口和动态 PTY 返回 ENOTTY。

| 请求 | 行为 |
| --- | --- |
| `VT_GETSTATE` | 返回当前 VT，以及 tty1～tty6 的位图 `0x7e` |
| `VT_ACTIVATE` | 激活 1～6，其他编号返回 EINVAL |
| `VT_WAITACTIVE` | 等待目标激活；使用可唤醒等待队列，信号遵循系统调用的中断处理 |
| `KDGETMODE` | 返回描述符对应 VT 的 KD_TEXT/KD_GRAPHICS |
| `KDSETMODE` | 调用进程必须以该 VT 为控制终端 |

ACTIVATE/WAITACTIVE 要求描述符对应调用者的控制终端，或者具有 `CAP_SYS_TTY_CONFIG`。会话 leader 退出或脱离控制终端时清除图形模式。`VT_SETMODE` 的进程式 release/acquire 信号协议尚未实现。

`/proc/self/fd/N` 和 `/proc/<pid>/fd/N` 提供终端、已命名文件描述符链接，支持 musl `ttyname()` 和 BusyBox `tty`。链接读取保留 procfs 的跨进程凭据检查；匿名 socket/pipe 的完整 Linux magic-link 语义仍不在本次实现范围内。

## 图形绘制与输入

桌面通过 `LEONOS_FBIOBLIT`（`0x46f2`，`<leonos/fb.h>`）原子提交像素或填充矩形。`struct leonos_fb_present` 是固定 32 字节：x/y/width/height/stride/color 为 uint32，pixels 为 uint64 用户地址。pixels 为零时填充 color，否则 stride 以 RGB32 像素计数。内核裁剪显示边界、验证源地址，并在同一事务内检查当前 graphical controlling VT、复制像素和刷新区域。

非 VT 会话返回 EPERM；当前不拥有图形显示的 VT 返回 EAGAIN。EAGAIN 必须直接返回用户态，不能按磁盘异步 I/O 重试。`leonos_fb_fill/rect/blit` 使用此请求；像素读取仍使用只读映射。传统 fbdev 原始 mmap 接口保留给自行管理显示的客户端，它不是具备 VT 撤销能力的映射；不能用它代替桌面的安全提交路径。`FBIOPUT_VSCREENINFO` 同样要求活动图形控制终端。

windowd 对 keyboard/mouse 描述符设置 `LEONOS_EVIOCSVT`（`0x400445f0`，uint32 指针）：0 表示原始流，1～6 表示只读取在对应图形 VT 产生的记录。过滤属于共享 open file description，设置时从当前事件序号开始。read/poll 使用一致规则，避免 windowd 暂停后读取文本终端密码。恢复图形 VT 时发布鼠标位置、完整按钮状态和修饰键状态，修复在文本 VT 上释放输入导致的拖拽或修饰键残留；不转发文字键。

`LEONOS_VT_GETGENERATION`（`0x800856f0`，`<leonos/device.h>`）在固定 VT 描述符上返回 uint64 显示代数。切换 VT 或重新激活可见 KD 模式时递增。Desktop 在重绘前读取并记录代数，因此即使暂停期间连续切出、切回，也能在恢复时完整重绘。

Desktop 在非活动 VT 上继续有界消费窗口 IPC，跳过常规绘制。窗口连接发生短写时，IPC 库为每个连接保留至多一帧的未发送尾部，后续消息先续传该尾部；windowd 和客户端循环调用 `leonos_ipc_flush()`，因此暂停桌面不会破坏帧边界或永久丢失输入。非阻塞发送成功表示消息已被接受，仍可能需要 flush；新消息遇到 EAGAIN 表示未被接受，可稍后重试。关闭连接使用 `leonos_ipc_close()` 释放待发送状态。

## 安装环境

安装器复用同一套 VT。默认 tty1 桌面显式以 `installer --graphical` 启动安装向导；图形 login 同样使用 `--graphical-login`，不能因为继承了 tty1 stdin 而误入文本模式。GRUB 的 `installer-session=tui` 只选择安装环境的文本会话，不改变内核启动体系。

安装介质账号没有可用密码，文本 VT 提供由 installer-runtime 标记限制的本地 root shell；普通已安装系统仍使用 getty 和认证。文本 shell 中运行 `/usr/lib/leonos/apps/installer/installer.elf` 启动交互式安装器。安装后的系统不携带 installer-runtime 标记。

## 验证入口

- `python3 tools/test_linux_pty.py`
- `python3 -m unittest tools.test_console_boot_policy tools.test_runtime_responsiveness tools.test_procfs_taskmgr tools.test_installer_setup tools.test_installer_input.InstallerInputTests.test_mouse_coordinates_survive_evdev_routing`
- `python3 tools/test_vt_qemu.py`：默认正式 raw 镜像，图形登录、F1～F6、root 文本登录、stdio、tty、桌面退出和 getty 恢复。
- `python3 tools/prepare_vt_fixture.py --output build/vt-fixture-<唯一名称> --text-only`：从正式镜像制作独立副本，注入 SDK 编译的真实 ioctl 探针。随后使用 `VT_IMAGE=<副本>/disk.raw VT_OUTPUT=<截图目录> VT_TEXT_ONLY=1 VT_PROBE=1 python3 tools/test_vt_qemu.py`。
- `python3 tools/test_vt_installer_qemu.py --output build/vt-install-<唯一名称> [--tui|--install]`：只创建并操作新的 4 GiB scratch 磁盘，验证安装环境 VT 和向导；`--install` 执行真实安装。
- `python3 tools/test_vt_installed_qemu.py --disk <安装测试目录>/scratch.raw --output build/vt-installed-<唯一名称>`：启动上一步的测试磁盘，验证图形登录界面、六路 VT、root/普通用户认证与 tty2 归属。
- `python3 -m unittest tools.test_installer_input`、`python3 tools/test_unix_ipc.py`：包括短写后暂停、续传顺序、SCM_RIGHTS 单次传递和关闭清理。

QEMU 测试使用 KVM、host CPU、OVMF 和标准 VGA；不代表 VMware 图形硬件已验收。工具依赖 Pillow/Tesseract，通过截图识别文本终端，串口只接收显式写入的探针结果。
