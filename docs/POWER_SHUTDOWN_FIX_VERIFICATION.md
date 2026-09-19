# OpenRC 与桌面电源请求修复记录

日期：2026-09-20。当前工作区包含先前的 SMP 与键盘归属修改；本次未提交。

## 根因与修复

1. `sched_wait_reap()` 回收资源后仍保留旧 PID，直到任务槽复用。已被父进程 wait 消费的进程仍能被 `kill(pid, 0)` 找到。OpenRC 0.63.2 的 supervise-daemon 停止路径发送 SIGTERM 后循环探测 PID，造成无意义等待。现在在完成回收时持有 scheduler_lock 清除 PID；尚未被 wait 消费的僵尸保持原语义。
2. 桌面主循环同步调用电源认证接口并等待 sudo。sudo 的 askpass 需要桌面继续处理窗口事件，形成循环等待。现在由 fork 出的工作进程执行原认证接口，主循环以 WNOHANG 回收结果；认证失败显示错误并允许重试，请求在途时禁止重复操作。认证策略与正常 OpenRC 关机路径不变。

## 验证证据

- `make O=out -j8 kernel userland` 成功；正常桌面与内核均完成链接。
- 独立 VMware 测试机：2 vCPU、2 GiB、EFI、无磁盘、无网卡。未操作用户运行中的 LeonOS4 虚拟机。
- 同一进程回收探针：旧内核在第一个已回收子进程上失败（kill0=0），新内核连续 16 次返回 ESRCH。日志为 `out/verification/power/before-serial.log` 与 `after-serial.log`。
- 来宾执行正常 `/sbin/poweroff`：OpenRC 服务依次停止，执行 sync/umount，输出 Requesting system poweroff，测试虚拟机自动退出。
- 来宾执行正常 `/sbin/reboot`：同样完成 OpenRC 停止路径，输出 Requesting system reboot，随后日志出现第二次 BusyBox init started。见 `out/verification/power/reboot-serial.log`。确认再次启动后停止了测试虚拟机。
- fixture 中没有网卡，DHCP 启动失败属于测试配置；关机时只读安装根文件系统的 umount 提示仍存在，不是本次修复范围。

## 验证边界

桌面异步请求已编译并审查控制流，尚未通过真实菜单点击验证密码窗口、认证取消和用户应用退出的完整 GUI 交互。上述来宾电源验证通过 root 命令发起，不能替代 GUI 验证。未进行全量测试、Uinxed-Kernel 构建或安装升级测试。

## 交付镜像

`PATH=/usr/bin:/bin:/home/xiaobai/.cargo/bin make O=out RUSTC=/home/xiaobai/.cargo/bin/rustc -j8 installer` 返回 0，包含普通与安装器策略桌面、更新内核及已有工作区修改。产物：`out/images/leonos4-installer.iso`。构建日志：`out/verification/power/installer-build.log`。正常发行镜像未加入测试探针；来宾运行证据来自使用同一内核的独立验证镜像。
