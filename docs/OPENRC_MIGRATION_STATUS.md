# OpenRC 迁移实施记录

实施中，尚未通过目标机验收。依据 OPENRC_MIGRATION_AGENT_PROMPT.md，当前分支
xiaobai/openrc-support；保留原有需求文档，不委派、不提交。

## 职责审计和实施顺序

| 旧职责 | 新组件 / 启动方式 | 权限 / 消费者 |
| --- | --- | --- |
| 内核启动 init、imd、windowd、desktop、TTY、installer | 内核只执行 /sbin/init；BusyBox inittab 调用 OpenRC | PID 1 root；OpenRC 决定 runlevel |
| desktop/services.c 监督 serviced | 删除；OpenRC supervise-daemon | 图形服务独立生命周期 |
| serviced 配置、状态、命令文件 | 删除；rc-service、rc-update、rc-status | GUI 通过现有 sudo 认证后执行固定参数命令 |
| DHCP 重试和租约更新 | 上游 BusyBox udhcpc | OpenRC，唯一 DHCP 客户端 |
| DNS 发布 | udhcpc 脚本 | root；libc resolver、网络 GUI |
| NTP 轮询 | 上游 BusyBox ntpd | OpenRC，唯一同步进程 |
| 网络配置/查询 IPC | libc 查询内核；配置经授权命令桥接 | 查询允许普通用户，变更保留 root 检查 |
| 启动项数据库、用户会话 IPC | 独立 sessiond 业务进程 | root 接收凭据；用户程序应用 PAM UID/GID/组 |
| LeonOS 设备目录与驱动协议 | 独立设备协议业务组件 | 只处理设备协议；不监督服务 |
| network_icon、rtc_clock | taskbar.cfg 偏好 | 设置和桌面；不再表示服务运行状态 |
| 直接 reboot syscall | BusyBox PID 1 信号协议 | 现有 sudo/PAM 授权；OpenRC shutdown |

顺序：接入经过校验的 Alpine APK 和启动配置；迁移业务及消费者；统一内核启动与
电源；删除旧实现、归属和增量残留；正式构建、自审、主机回归和 VM 验证。

## 已知需验证的内核缺口

初步源码审计：孤儿进程被改成 parent_pid=0 而非交给 PID 1；exec 仅按 serviced
路径恢复 SERVICE 标志，需改为基于 root 身份及受保护可执行文件的明确图形服务
授权。标准网络客户端所需 AF_PACKET、网络接口 ioctl 和时间调整需运行证明。

## 验收状态

十项目标机验收均尚未完成。后续记录以实际命令与日志更新，不把源码变更视为运行通过。

## 设备协议实际修正

旧 devmand 的驱动列表是五个硬编码“已加载”条目，控制操作对 root 直接返回成功。
现在列表从只读 `/proc/leonos-drivers` 获取实际 `leonos_driver_info` 记录（ABI version
沿用公共 driver.h），内核通过 driver_manager_list 填充。旧伪控制返回已改为真实
ENOTSUP；驱动控制的正式内核接入仍需完成，不能将此项列为已迁移成功。

## 主代理接手后的实测记录（2026-09-15，进行中）

本节不代表整体迁移验收完成，也不代表所有新增 syscall/网络选项完整兼容。

- 已修复 supervise-daemon 控制 FIFO 所需的 mknod/mknodat、ext2/tmpfs FIFO
  节点和阻塞打开/读写/描述符生命周期。`tools/test_linux_fifo.py --guest`
  使用同一静态 musl 测试在宿主和 QEMU/KVM q35、2 vCPU 通过。
  非 ext2/tmpfs 后端的 FIFO、字符/块设备 mknod 仍不支持。
- 监督服务的标准输入改为 `/dev/null`，避免图形登录程序误入 TTY 模式。
  QEMU 已显示图形登录界面；未据此声明登录后会话、安装器或 VMware 已通过。
- 增加真实 `/proc/<pid>/environ` 字节读取、跨页/偏移和读取权限回归。
  仍需进一步检查 Linux 的打开时权限、元数据和持有旧 mm 的完整生命周期语义。
- 发现 BusyBox standalone shell 在宿主 Linux 上同样使 OpenRC 的
  `VAR=a/b md5sum /proc/self/environ` 得到相同结果。关闭
  `CONFIG_FEATURE_SH_STANDALONE`，不修改 BusyBox 源码；新增行为测试先失败后通过。
- AF_PACKET cooked/raw 和 IPv4 raw socket 已加入真实收发路径；修复 socket
  内部通用类型标志遗漏导致 recvmsg 返回 ENOTSOCK 的问题。
- 新增 native Linux 接口索引、MAC、IPv4 地址/掩码/广播、上下线、接口枚举和
  默认网关 ioctl。非默认路由、可变 MTU 等未实现功能明确拒绝，不保存伪状态。
  默认路由零掩码允许零 sa_family，依据 Linux v6.12
  `net/ipv4/fib_frontend.c:rtentry_to_fib_config`。
- UDP SO_BINDTODEVICE 已连接到真实收发接口过滤；完整选项/接口组合仍待回归。
- 禁用内核隐式 DHCP 回退及旧 renew 入口，避免与 OpenRC 启动的 udhcpc 竞争。
- 最新 QEMU 中官方 BusyBox udhcpc 完成 DORA，租约 `10.0.2.15/24`、网关
  `10.0.2.2`、租期 86400 秒；hook 发布 `nameserver 10.0.2.3`。
  来宾 BusyBox nslookup 能获取 pool.ntp.org 的 DNS 回答。
  宿主 DNS 当前返回 198.18.* 地址，不能据此宣称真实公网 NTP 可达。
- `tools/test_openrc_network.py` 直接编译内核实现，ASan/UBSan 验证权限、
  真实 Ethernet 帧构造、消息边界、peek/trunc、队列释放、地址/路由与接口状态。

证据：`build/openrc-fifo-probe/guest-smp2.log`、
`build/openrc-network-build.log`、`build/openrc-busybox-build.log`。
上述 guest 日志来自临时探针镜像；其 staging 使用正式 APK root，但覆盖了最新
kernel/BusyBox/服务脚本，不是最终发行 ISO。最终普通镜像和 installer 必须统一重建。

继续处理：NTP 启动期 DNS 就绪、ntpd 需要的真实 adjtimex 时钟纪律、DHCP
续租/GUI 状态确认、网络协议边界回归、设备控制、会话/监督/关机和安装器验收。
`adjtimex` 尚未实现，不能把 ntpd 进程启动等同于成功同步。

### 时钟纪律与真实 NTP 样本（后续进展）

前述“adjtimex 尚未实现”是本次接手初期的状态，现已新增实现，但仍不表示完整 ABI。

- `time_discipline.c` 实施 scaled-ppm 频率、PLL/FLL 相位调整、单次 adjtime、
  模式/权限/限额和闰秒状态。校正实际进入 PIT tick 长度，realtime/monotonic
  共同 slew，MONOTONIC_RAW 不受影响。时钟快照和更新受 IRQ-safe 锁保护。
- native adjtimex(159)、realtime clock_adjtime(305) 路径已加入。ADJ_SETOFFSET、
  动态设备时钟仍明确不支持，完整 Linux 的错误优先级、PPS/精度、timerfd 与调时
  交互尚未认证。不可将这两个调用标为完全兼容。
- ASan/UBSan 实际时钟测试验证 +1/-1 ppm 的每秒纳秒增减、PLL 相位收敛、
  adjtime、闰秒插入、极值；集成测试验证 realtime/monotonic 受到真实调整，
  RAW 保持原速，step 不移动 monotonic，锁不覆盖 scheduler/USB 调用。
- `tools/test_linux_fifo.py --guest --ntp-probe` 使用宿主临时 UDP 端口响应正确的
  NTP origin/receive/transmit 时间戳，给出 +120 秒测试偏差。QEMU 中未修改的
  ntpd 完成采样，日志记录 offset +120.999421、实际 wall clock set、后续
  update from 与 clock drift 输出，无 adjtimex ENOSYS。这里是本地网络验证，
  不代表真实公网 NTP 或 VMware 已验证。
- 新增标准 ntpd `-S` 通知 hook，只原子发布样本状态，不实现协议/轮询/监督；
  GUI 同步 API 等待新通知及真实 PLL 状态，失败/超时返回错误。
  BusyBox ntpd 不维护 Linux maxerror，因此样本通知不等同于 TIME_OK 认证。
- DHCP hook 原子发布含 monotonic 获取时间的租约；GUI 读取检查 root 所有权、
  非组/其他可写、非符号链接、租约有效期和接口地址匹配。重启 DHCP 服务先撤销旧
  租约记录，GUI 等待新租约或超时，不再只依据服务返回值判断成功。

当前正式 image-vmdk 正在统一构建；NTP hook/GUI、installer、长时间多核运行和
监督/关机仍需要完成后续验证。探针文件不可冒充发行镜像。

### 设备控制和 PID 1 通知返修

设备代理的 root 控制请求不再统一返回 ENOTSUP：固定大小请求经 `/dev/driverctl`
进入内核，检查 CAP_SYS_MODULE、保留字段、字符串终止后调用真实
`driver_manager_control`。修复旧客户端误用包含函数指针的 module 结构发送控制
命令、文件名长度截断和 errno 不回写。宿主测试检查真实 dispatch 的拒绝路径及
后端错误传播；真实驱动卸载/重载仍待来宾验证。

收窄另一 AI 新增的 PID 1 SIGCHLD：仅收养已经退出且可回收的孤儿才额外通知，
正常子进程退出沿用组退出通知；不会每个普通进程退出都向 PID 1 发信号。
收养线程不会把非 leader pthread 标成可 wait 的独立子进程。

第二次统一 image-vmdk 构建 0 errors；正式图形登录探针正在使用该镜像的只读
基础快照验证，尚未完成 installer 及全部目标场景。
