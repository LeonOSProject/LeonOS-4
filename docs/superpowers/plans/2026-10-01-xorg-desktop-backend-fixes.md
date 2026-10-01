# Xorg/TWM 桌面问题修复计划（2026-10-01）

> **面向 AI 代理的工作者：** 本计划是
> [2026-10-01-xorg-desktop-backend.md](2026-10-01-xorg-desktop-backend.md)
> 的修复轮次。根因不是单一 xterm/鼠标配置，而是 Xorg 后端未完整实现
> Linux VT 所有权协议，内核输入与 framebuffer 仍把图形会话当作全局消费者。

**目标：** 补齐标准 Linux VT process ownership，把 evdev/fbdev 的消费
边界绑定到 VT，重写会话生命周期，并在 QEMU 中验收 VT 切换与输入隔离。

**约束：** 只用标准 Linux UAPI 与 POSIX 接口；`RELIEFOS_EVIOCSVT` 只保留
兼容用途；不修改 Xorg 上游源码；不引入 DRM/KMS、udev、libinput、Wayland；
x86_64 结构体布局与 ioctl 编码保持 Linux 一致；失败返回标准 errno。

## 任务拆分

### A. 内核 ABI（VT process ownership 与所有权边界）

- [x] `pty.c` 每 VT 保存 `struct vt_mode`（默认 `VT_AUTO`）与控制器 pid。
- [x] `VT_GETMODE`/`VT_SETMODE`/`VT_RELDISP`：`VT_PROCESS` 保存
      relsig/acqsig；非控制终端 `EPERM`；重复控制器 `EBUSY`；非法
      模式/信号/指针 `EINVAL`/`EFAULT`。
- [x] 切换离开图形 VT 时发 release 信号，未 `VT_RELDISP 1` 前不切换
      显示与输入；切回发 acquire 信号，`VT_RELDISP VT_ACKACQ` 完成恢复。
- [x] 控制器死亡自动回退 `VT_AUTO`；`KDSETMODE(KD_GRAPHICS)` 的 owner
      死亡后惰性回收为文本模式（图形进程被杀后 getty 仍可上屏）。
- [x] fbdev 修改类 ioctl（`FBIOPUT_VSCREENINFO`/`FBIOPAN_DISPLAY`/
      `FBIOBLANK`/`FBIOPUTCMAP`）检查 VT ownership，release-pending 时
      `EAGAIN`；补齐 `fb_var_screeninfo`/`fb_fix_screeninfo`/`fb_cmap`
      布局与 `FBIOGETCMAP`/`FBIOPUTCMAP`/`FBIOBLANK`。

### B. 输入路由（evdev VT scope 与文字抢占）

- [x] evdev 描述符在 `open` 时绑定调用者的 VT；tty2-tty6 的普通键、
      鼠标与按钮不再进入 tty1 图形会话的事件流。
- [x] 图形 VT 活动时内核文字路径不向任何文字 PTY 写普通字符。
- [x] Ctrl+Alt+F1..F6 由内核 VT 层处理并走 release 握手，组合键本身
      不写入文字 PTY。
- [x] 图形 VT 恢复时重发指针坐标、按钮、修饰键状态与 `SYN_REPORT`。

### C. 会话生命周期（退出边界与逐项状态）

- [x] 启动前校验控制终端是 `/dev/tty1` 并显式打开；不在 tty1 立即
      报错退出。
- [x] 去掉 `-novtswitch`，Xorg 走标准 VT 切换协议（`vt1 -keeptty`）。
- [x] 退出/超时/信号终止后收尸 Xorg/twm/urxvt，逐项记录 Xorg、client、
      twm、urxvt 与认证登录 shell 的退出状态；登录超时视为正常结束。
- [x] 默认会话保持 `twm + urxvt + 已认证登录 shell`，不给 root shell；纯
      POSIX `/bin/sh`。
- [x] KD_TEXT 恢复：Xorg 干净退出自复位，异常路径由内核 owner 死亡
      回收兜底（POSIX sh 无法 ioctl，故不引入新用户态助手）。

### D. QEMU 验收（切换、隔离与压力）

- [x] 鼠标移动与点击进入 X 会话（xeyes 瞳孔跟踪、twm 根菜单弹出）。
- [x] Ctrl+Alt+F2 进入 tty2 文字控制台；tty2 输入唯一标记只出现在
      tty2；X 终端捕获流（`cat > /root/xkeys.log`）不含该标记。
- [x] Ctrl+Alt+F1 回到 TWM/urxvt 桌面（而非 tty1 文字登录），键盘与
      鼠标继续工作。
- [x] 重复切换压力后 X 桌面仍存活；退出 `/bin/login` 后 tty1 恢复
      文字登录。
- [x] 收集并检查 `Xorg.0.log`、`xorg-session.log`、serial、截图与
      evdev/fbdev 探针输出。

## 交付验收命令

```sh
sh tests/build/test-xorg-session.sh
python3 tools/test_linux_pty.py
python3 tools/test_installer_input.py
python3 tools/test_xorg_abi.py
make -s O=out/xorg-plan-test test-build
make fetch
python3 tools/test_xorg_qemu.py --output build/xorg-qemu-fixed
```
