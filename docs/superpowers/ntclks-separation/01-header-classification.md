# 头文件分类表（阶段 0/1）

日期：2026-09-25。基于 `refactor/ntclks-separation@30b1db6` 的静态分类（源码已检查，
未编译验证分类本身）。分类目标见
[设计 §5](../specs/2026-09-24-ntclks-separation-design.md)。

类别：**UAPI**（wire ABI，内核唯一源、headers_install 导出）／**runtime**（用户态库 API，
归 userland/runtime）／**boot**（loader↔kernel 协议）／**module**（Ring-0 .drv API）／
**private**（内核私有）／**resource**（资源/实现数据，非 ABI）。

## 1. include/leonos/ 逐头分类（35 头 + 2 字体资源）

| 头文件 | 类别 | 消费者 | 判定 |
| --- | --- | --- | --- |
| admin.h | runtime | apps, libc | 纯用户态 |
| api.h | runtime（含 .api 清单磁盘格式） | apps, libc | 纯用户态 |
| audio.h | **混合**：wire（`leonos_audio_*`、`LEONOS_AUDIO_STATUS_*`）+ runtime（`leonos_audio_*()`） | ntclks 3, libc 1, devtools 1 | 两侧 |
| auth.h | **混合**：UAPI（`LEONOS_AUTH_ROLE_*`、`struct leonos_auth_status`、再导出 uapi/auth_user.h）+ runtime（`leonos_auth_*()`） | ntclks 5, apps 13, libc 7, 其他 3 | 两侧 |
| boot_handoff.h | boot（v7，含裸指针，仅内存交接） | boot/loader 1, ntclks 5, drivers 1 | 仅内核+loader |
| device.h | **混合**：UAPI（devfs 路径宏、`LEONOS_VT_GETGENERATION 0x800856f0`、`LEONOS_EVIOCSVT 0x400445f0`、wire struct）+ runtime（`leonos_device_list()`） | ntclks 3, apps 4, libc 9 | 两侧 |
| devmgr_service.h | runtime（devmand IPC 客户端） | apps 2, libc 1 | 纯用户态 |
| driver.h | **混合**：module API（`LEONOS_DRIVER_MODULE_MAGIC 0x4c445256`、`struct leonos_driver_module/kernel_api`、ops struct）+ UAPI（`LEONOS_DRIVER_CONTROL_IOCTL 0xc0504c64`）+ runtime（`leonos_driver_list/control()`） | drivers 5, ntclks 3, libc 5 | 两侧 |
| elf_abi.h | private（`struct leonos_dynamic_launch` 经 r8 交接） | ntclks 2 | 仅内核 |
| fb.h | UAPI（ioctl wire，无函数） | ntclks 1, libc 1 | 两侧 |
| fs.h | **混合**：再导出 uapi/fs_abi.h + runtime（`leonos_list_dir/stat_legacy/fs_acl_*`）+ 游离的 libc `lseek` 声明 | ntclks 12, apps 30, libc 14 | 两侧 |
| gpu.h | **混合**：UAPI（`LEONOS_IOCTL_GPU_* 0x4c475001-5`、size/version 前缀 wire struct）+ runtime（`leonos_gpu_*()`） | ntclks 2, libc 3 | 两侧 |
| gpu_sdk.h | runtime | apps 3, libc 1 | 纯用户态 |
| http.h | runtime | apps 6, libc 3 | 纯用户态 |
| ini.h | runtime | libc 3 | 纯用户态 |
| inputm.h | **混合**：服务 wire + runtime + GUI 便捷函数 | ntclks 1, apps 2, libc 6 | 两侧 |
| kernel_debug.h | **混合**：控制 wire（`struct leonos_kernel_debug_control`）+ runtime（`leonos_kernel_debug_*()`） | ntclks 2 | 今日仅内核消费 |
| launch_result.h | runtime（固定枚举值是结果 wire） | apps 4, libc 1 | 纯用户态 |
| layout.h | resource（路径常量；`tools/leonos_layout.py` 镜像） | ntclks 5, drivers 1, apps 44, libc 11 | 两侧 |
| license.h | runtime | apps 2, libc 1 | 纯用户态 |
| net.h | **混合**：wire struct（`uapi/leonos/net_control.h` ioctl union 的成员类型）+ runtime（`leonos_net_*/leonos_socket_*()`） | ntclks 4, apps 1, libc 7 | 两侧 |
| net_service.h | runtime | apps 5, libc 1 | 纯用户态 |
| openrc.h | runtime | apps 2, libc 3 | 纯用户态 |
| png.h | runtime | apps 6, libc 1 | 纯用户态 |
| psf_font.h | resource/实现（内嵌 PSF 数据 + header-only 解析器） | apps 23, libc 2, ntclks 2, ostui 1, boot 1, drivers 2 | 两侧 |
| pty.h | UAPI 别名层（`LEONOS_PTY_*`=`LINUX_*`）+ wire struct（`leonos_pty_winsize`） | ntclks 3, libc 1, 其他 2 | 两侧 |
| signal.h | UAPI 别名层 + private（`leonos_rt_sigreturn_trampoline`） | ntclks 2 | 仅内核消费 |
| startup.h | **混合**：IPC wire + runtime | ntclks 2, apps 4, libc 4 | 两侧 |
| system.h | **混合**：UAPI wire（`leonos_system_info` 等，注释冻结偏移）+ runtime（`leonos_system_*()`）；头级依赖 net.h、kernel_debug.h | ntclks 9, apps 9, libc 5 | 两侧 |
| tar.h | runtime | apps 1, libc 2 | 纯用户态 |
| text.h | runtime + 共享编码常量 | ntclks 2, apps 2, libc 3 | 两侧 |
| text_input.h | runtime | apps 5, libc 1 | 纯用户态 |
| utf8_stream.h | 实现（header-only UTF-8 解码器） | drivers 1, ostui 1 | 仅内核/驱动 |

字体资源：`lat15_vga16.psf`、`lat15_vga16_psf.inc`（`leonos_lat15_vga16_psf[]`/`_len`）。

## 2. 混合头的声明级拆分点

| 头文件 | 归 UAPI | 归 runtime | 其他 |
| --- | --- | --- | --- |
| auth.h | `LEONOS_AUTH_ROLE_*`、`LEONOS_AUTH_USER_DISABLED`、`LEONOS_AUTH_UPDATE_ROLE/FLAGS`、`struct leonos_auth_status`、再导出 `uapi/leonos/auth_user.h` | `leonos_auth_password_valid/status/current/list_users/users_alloc/logout/create_user/update_user/request_power` | — |
| fs.h | 再导出 `uapi/leonos/fs_abi.h`（`leonos_stat/dir_entry/dir_list/fs_acl*`、`LEONOS_FS_*/LEONOS_O_*/LEONOS_SEEK_*`） | `leonos_list_dir/stat_legacy/fstat_legacy/readdir/fs_acl_get/set/take_ownership/repair` | `lseek` 声明移出（libc 自有） |
| system.h | `leonos_system_info/perf_* /task_affinity/time_* /machine_identity` wire struct 与长度常量 | `leonos_system_info/perf_info/task_affinity_get/set/time_info/time_ntp_sync/machine_identity/reboot/shutdown` | 头级依赖 net.h（`LEONOS_NET_HOSTNAME_LEN`）、kernel_debug.h 需解耦 |
| device.h | devfs 路径宏、`LEONOS_VT_GETGENERATION`、`LEONOS_EVIOCSVT`、class/flag 常量、`leonos_device_info/list` | `leonos_device_list()` | — |
| driver.h | `LEONOS_DRIVER_CONTROL_IOCTL`、`leonos_driver_control/info/list`、状态/标志常量 | `leonos_driver_list/control()` | module API（magic/ABI 版本、`struct leonos_driver_module/kernel_api`、ops/state struct、`LEONOS_DRIVER_KIND_*`）整体移入 module 域；头级 `#include <leonos/audio.h>` 跨类依赖 |
| inputm.h | `LEONOS_INPUTM_*`、`struct leonos_inputm_provider/key_event/result/...` | `leonos_inputm_register/unregister/...` | GUI 便捷函数归应用侧便利层 |
| net.h | `leonos_net_config/dhcp/dns_policy/ping/dns/http_get/socket_*/connection_*`（net_control.h ioctl union 成员） | `leonos_net_*/leonos_socket_*()` | **反向依赖**：`uapi/leonos/net_control.h` → `leonos/net.h` 必须切断（wire struct 迁入 UAPI） |
| audio.h / gpu.h / kernel_debug.h / startup.h | 各自 wire struct 与常量 | 各自 `leonos_*()` 包装 | 同一模式 |

其余：`boot_handoff.h`（boot API，含裸指针，版本 7 拒绝 v6）、`elf_abi.h`（private）、
`pty.h`/`signal.h`（UAPI 别名层）、`psf_font.h`（resource，含 header-only 解析器）、
`layout.h`/`utf8_stream.h`（resource/实现，非 ABI）。

## 3. include/uapi 与 include/linux

- `include/uapi/README.md`：声明 Linux v6.12 native x86-64 wire ABI（无 i386/x32）。
- `include/uapi/linux/`（46 头）：`syscall.h`（`tools/generate_linux_syscalls.py` 生成，
  758 行 `__NR_*`）、`types.h`、`errno.h`（`LINUX_E*` + 守卫的 `E*`）、`ioctl.h` 编码宏、
  文件/挂载（fcntl/stat/statfs/fs/mount/openat2/dirent/limits）、进程生命周期
  （sched/prctl/capability/securebits/membarrier/rseq/arch_prctl/resource/sysinfo/utsname/reboot）、
  信号（signal/signalfd，64 位掩码与 x86-64 信号帧）、内存/同步/时间（mman/futex/poll/epoll/
  eventfd/timerfd/time/times/timex）、IPC（ipc/msg/sem）、socket（socket/if/if_packet/uio）、
  设备 wire（tty/termios/vt/kd/fb/input/soundcard）。
- `include/uapi/leonos/`（5 头）：`syscall_abi.h`（`LEONOS_SYS_NICE 0x10000`）、
  `auth_user.h`（`struct leonos_user_info`）、`fs_abi.h`（依赖 `linux/fcntl.h`）、
  `net_control.h`（`LEONOS_NET_CONTROL_IOCTL _IOWR('L',0x70,...)`，**当前错误依赖
  `leonos/net.h`**）、`rootfs.h`（`LEONOS_ROOTFS_DIRECTORIES/SYMLINKS` X 宏 + `LEONOS_DEFAULT_PATH`）。
- `include/linux/`（13 头）：到 `include/uapi/linux/` 的转发壳（兼容旧 include 路径）。

## 4. 重复镜像（SDK 模板）

| 对比组 | 相同 | 已分叉 | 备注 |
| --- | --- | --- | --- |
| devtools/include/leonos ↔ include/leonos（32+2） | 27 | 7（devmgr_service、elf_abi、inputm、net、net_service、system、text_input） | 陈旧模板 |
| devtools/include/leonos ↔ userland/libc/include/leonos（24） | 19 | 5（app、blockdev、launch、unix_ipc、windowd） | — |
| include/leonos ↔ userland/libc/include/leonos（8） | 4（gpu_sdk、launch_result、layout、openrc） | 4（devmgr_service、inputm、net_service、text_input） | **遮蔽**：`-Iuserland/libc/include` 在 `-Iinclude` 之前，用户态实际用 libc 副本 |
| devtools/include/linux ↔ include/uapi/linux（29） | 15 | 14 | 19 个 UAPI 头无模板 |
| devtools/include/leonos ↔ include/uapi/leonos | auth_user、syscall_abi 相同 | fs_abi 分叉 | rootfs、net_control **缺模板**，而模板 layout.h 却 include rootfs.h（模板自身不完整） |

另有：`devtools/include/README.md` ≡ `include/uapi/README.md`；
`tools/tests/legacy_authd/include/leonos/` 是 auth 头的测试用分叉（含已退役 `LEONOS_AUTH_OP_*`）；
`tools/leonos_layout.py` 是 `layout.h`+`rootfs.h` X 宏的构建侧镜像（需同步维护）。

## 5. 跨树/相对路径 include

- `drivers/bootstrap/usb_uhci.c:8`：`#include "../../kernel/ntclks/arch/x86_64/port.h"`（驱动以相对路径触内核私有头）。
- `drivers/*` 以 `-Ikernel/ntclks/include` 编译并 include 23 个 `ntclks/*.h` 私有头
  （console/mm/framebuffer/pci/time/port/tmpfs/syscall/storage/paging/lock/input/usb/
  text_utf16/svga/smp/sched/pty/permissions/page_cache/multiboot2/mouse/efi_fs）——
  今日 .drv 的模块 API 实为"整个 ntclks include 树"，module 域收编是阶段 1/4 的显式工作。
- `userland/libc/src/{pam_session.c,auth_accounts.c}` → `../../auth/*.h`；
  `userland/apps/installer/{main.c,installer_setup.c}` → `../../auth/standard_accounts.h`。
- `tools/tests/*.c`（宿主白盒测试，约 120 处）以相对路径 include 各树实现 `.c` 文件
  （内核 syscall*/sched/mm/arch、drivers、libc、apps）；迁移时必须逐个改写。
- include 内部链：`auth.h→uapi/auth_user.h`、`fs.h→uapi/fs_abi.h`、`layout.h→uapi/rootfs.h`、
  `uapi/net_control.h→leonos/net.h`（**反向，待切**）；http/system/gpu_sdk/text_input/
  net_service/devmgr_service/driver/api/elf_abi/psf_font 链入其他 leonos 头。
- 内核无用户态路径 include；无 `../../include/...` 用法。

## 6. 对分离的关键结论

1. UAPI 今日三分：`include/uapi/`（目标家）、`include/leonos/` 12 个混合头中的 wire 部分、
   以及反向依赖 `leonos/net.h` 的 `uapi/leonos/net_control.h`。
2. runtime API 在 `userland/libc/src/*` 实现，但与 wire struct 同文件声明；内核文件
   include 这些头只为 struct/常量——按声明拆分后内核只 include UAPI。
3. module API 是 `driver.h` + 23 个 `ntclks/*` 私有头 + 1 处相对路径；`boot` 域干净。
4. resource/实现头（psf_font、utf8_stream、layout、rootfs）无 ABI 但 include 量大
  （layout.h 70+ 文件），按设计 §5.6-5.7 各自保留唯一权威源。
5. devtools/include 是陈旧镜像（27+15 相同、7+14+5 分叉、2 缺失），应从安装结果装配而非手工维护。
6. 遮蔽组（libc 副本 vs include/leonos 的 4 个分叉头）在拆分时必须先合一，否则 UAPI
   迁移会静默改变用户态实际编译内容。
