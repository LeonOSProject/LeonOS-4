# VMware 双核修复与验证（2026-09-20）

基线提交：`4b81701`。本次工作区包含另一 Agent 的键盘归属修复，以及本轮的 SMP 修复；未提交或推送。

## 修复与审查

- 移除 `SMP_USER_SCHEDULER_ENABLED=0` 的硬编码限制，按 MADT 发现的 CPU 启动 AP。
- 保存 trap frame 后保留 `running_cpu`，直到调度选择在锁内切换至内核 CR3，才发布旧任务可迁移/回收。无可运行任务时也退休旧 CR3，不再让 idle CPU 持有可能被回收的页表。
- 调度交接、信号处理和装载期间使用 execution transaction，防止另一个 CPU 的 exec/exit/reap 改写正在使用的任务元数据；空闲 `sti; hlt; cli` 前释放事务，唤醒后重新获取。重试使用循环而非递归叠加锁深度。
- 唤醒保留正在退出 syscall 的 CPU 归属，并允许跨 CPU 发布 READY，避免丢失远程唤醒。EEVDF 分配跳过离线 CPU。
- fork 将父页表改为 COW 后执行跨 CPU TLB 刷新，包括部分失败路径，避免共享父地址空间的线程继续使用旧的可写映射。
- 审查并保留另一 Agent 的 GUI/TTY 键盘归属修复；补充两个新 input 函数的实现注释。

调度交接仍串行使用全局执行事务，用户代码可跨核并行。本次不宣称消除了全部内核串行瓶颈；现有多核存储路径仍采用同步事务。

## 实际验证

使用独立、无用户磁盘的 VMware 虚拟机，2 GiB RAM、1 socket × 2 cores、EFI。从现有安装器根镜像的副本注入探针，并将本次内核放入测试 ISO；未改动用户的虚拟机或磁盘。

双核日志：`out/verification/smp/final-probes-serial.log`。

- `MADT CPUs=2`、`discovered=2`、`SMP CPU1 APIC=1 online`、`SMP ready online=2/2`。
- `keyboard-owner=gui`，OpenRC 启动桌面与安装器服务。
- EEVDF 来宾探针：`active_cpus=2`，nice 0:0 比例 1.006，nice 0:5 比例 3.050；fairness、affinity、fork/yield/wait 完成，退出码 0。
- execution-lock 来宾探针：独立进程与共享地址空间线程缺页、内存内容校验完成，退出码 0。单进程 8192 页约 0.130 秒、双进程 16384 页约 0.300 秒；工作量不同，不能由此宣称加速。
- `/proc/stat` 同时存在 cpu0、cpu1，忙碌计数分别为 1087、9，证明第二个 CPU 实际执行过任务。
- `tools/test_eevdf.py` 的 ASan/UBSan 策略与调度集成检查通过。新增检查覆盖保存帧仍保留归属、切换 CR3 前旧任务仍被保留、空闲退休、跨核唤醒不释放归属以及离线 AP 不分配任务。
- 同一测试 ISO 的单 CPU VMware 回归也通过：`active_cpus=1`，fairness-rc=0、memory-rc=0、DONE；日志为 `out/verification/smp/single-cpu-serial.log`。测试虚拟机已停止，用户原虚拟机保持运行。
- 临时宿主机夹具调用真实 `pty.c`：GUI Ctrl-C 不发送控制台 SIGINT，TTY Ctrl-C 仍发送，左右 Ctrl 分离释放正确。源码与输出保存在 `out/verification/smp/keyboard-check.c`、`keyboard-host.log`。这不是 VMware GUI 按键端到端验证。

编译命令：

```sh
PATH=/usr/bin:/bin:/home/xiaobai/.cargo/bin make O=out -j8 kernel
PATH=/usr/bin:/bin:/home/xiaobai/.cargo/bin make O=out RUSTC=/home/xiaobai/.cargo/bin/rustc -j8 installer
```

构建日志位于 `out/verification/smp/build.log` 和 `installer-build.log`。测试 ISO 为 `final-probe.iso`，含诊断探针；正常安装 ISO 不包含这些额外探针。来宾测试后仅调整了函数契约注释；没有改变已测实现。

上述两条构建命令均以退出码 0 完成。正常安装 ISO 已生成于 `out/images/leonos4-installer.iso`（2026-09-20 01:39 CST）。从 ISO 启动区和安装器 ext2 的 `/install/esp/leonos/kernel.sys` 分别提取内核，与实际 VMware 探针 ISO 中的内核逐字节比较一致：

```text
kernel.sys SHA256: 69efbc0ffe957780f31a8495105a3cd2641977f7a418d18fe2df4780c38add87
installer ISO SHA256: d053d164d249af8f2ddc7ef3eea7cfd22a9265989c4a133256aa05cfc41c265a
```

## 环境与证据边界

- 用户的 `/home/xiaobai/vmware/LeonOS4/LeonOS4.vmx` 调查时为 `cpuid.coresPerSocket="1"`，没有 `numvcpus` 项。它与用户期望的两核不一致；本轮未修改该配置。使用双核需要在虚拟机关机后确认 VMware 配置实际提供两个 vCPU。
- QEMU/OVMF 在进入 LeonOS 之前出现 EFI 页故障，本次双核成功证据来自 VMware，而非 QEMU。
- 测试虚拟机最初位于 `/tmp`，VMware 因 tmpfs 空间不足暂停；随后移到项目磁盘的 `out/verification/smp`，已完成上述探针。
- 测试 VM 没有网卡，日志中的 leonos-dhcp 启动失败符合测试配置，与 SMP 结论无关。
- 未在用户已安装环境重新完整编译 Uinxed-Kernel；未完成 taskmgr 图形核数及实体键盘 Ctrl-C 的 VMware GUI 端到端操作。不以探针通过替代这些验证。
