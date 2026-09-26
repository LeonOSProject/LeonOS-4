# NTCLKS 职责目录重排移动清单（阶段 5.7）

日期：2026-09-27。范围：NTCLKS 仓内职责目录重排，按交接 §5.7 分组
（arch/mm/fs/net/exec/console/debug）逐组实施；主仓只跟 gitlink 与测试路径。
落点依据规格 §4.2 最终职责布局与 §4.3 移动映射；本清单保持"可比较"——
每组一张 from→to 对照表，完成即勾，未动项如实列出。

## 0. 落点决策

- 组名沿交接 §5.7；**落点按 §4.2 职责树**：
  - `exec` 组 → `kernel/exec/`（§4.2：kernel/ 职责含"核心、sched、exec、syscall、
    同步和信号"，与既有 `kernel/ntclks/sched/` 同级归 kernel/ 的语义一致）。
  - `console` 组 → `drivers/console/`（§4.2：drivers/ 职责含"console/TTY/显示后端"；
    §4.2 树无顶层 console/）。`kernel/ostui` 归内核 console 内部实现（§4.2 正文）。
  - `debug` 组 → 顶层 `debug/`（§4.2 树明示）。
- 纯机械移动：被移 *.c 的 include 全为 `<ntclks/…>`/`<leonos/…>` 角括号全局
  include，无源码改动。唯一相对 include（arch 组内 `idt.h`/`port.h`/
  `keyboard_led.h`）随组整体迁移，组外无消费者（已核）。
- 对象路径随源相对路径变化（`O_OBJ/kernel/<src>.o`），每组重建即全量重编内核，
  无陈旧对象混链风险。

## 1. 分组移动表

### G1 arch（18 文件）

| from（NTCLKS 仓内） | to |
| --- | --- |
| `kernel/ntclks/arch/x86_64/*`（15：apic.c arch.c boot.S gdt.c idt.c idt.h irq.c keyboard_led.h linker.ld paging.c pci.c port.h power.c smp.c smp_trampoline.S） | `arch/x86_64/*` |
| `kernel/ntclks/multiboot2.c` | `arch/x86_64/multiboot2.c` |
| `kernel/ntclks/cpuinfo.c` | `arch/x86_64/cpuinfo.c` |
| `kernel/ntclks/platform.c` | `arch/x86_64/platform.c` |

构建旋钮：`mk/kernel.mk` `KERNEL_LD_SCRIPT` → `arch/x86_64/linker.ld`；
`KERNEL_SOURCE_DIRS` 增 `arch`。
引用修复：`tests/build/test-incremental.sh:356`、`tests/long/test-jobs.sh:63,122`
（linker.ld 路径）、`docs/ABI_PRIVATE_INVENTORY.md`、NTCLKS `README.md`。

状态：**已完成**（NTCLKS `e398731`：18 文件 + mk/kernel.mk + 5 处组外
include 路径；主仓测试路径修复与 gitlink 同组提交）。验证：G1_KERNEL_EXIT=0、
NTCLKS make test 0、test-incremental 32 检查、test-jobs 19 检查、
test_linux_abi_contract 全过。

### G2 mm（3 文件）

| from | to |
| --- | --- |
| `kernel/ntclks/mm/mm.c` | `mm/mm.c` |
| `kernel/ntclks/heap.c` | `mm/heap.c` |
| `kernel/ntclks/page_cache.c` | `mm/page_cache.c` |

构建旋钮：`KERNEL_SOURCE_DIRS` 增 `mm`（`kernel/ntclks/mm/` 移空后 find 不再产出）。

状态：**已完成**（NTCLKS `5921cb5`：3 文件 + SOURCE_DIRS；主仓test_linux_memory/physical_pages_test 路径修复、paging_protection_test补 smp_flush_user_tlb 桩——该桩缺失是 NTCLKS a3d403d 起的既有缺口，与本次移动无关，按文件既有桩模式补齐后断言原样）。验证：G2_KERNEL_EXIT=0、NTCLKS make test 0、test_linux_memory 8 用例全过。

### G3 fs（5 文件）

| from | to |
| --- | --- |
| `kernel/ntclks/tmpfs.c` | `fs/tmpfs.c` |
| `kernel/ntclks/procfs.c` | `fs/procfs.c` |
| `kernel/ntclks/sysfs.c` | `fs/sysfs.c` |
| `kernel/ntclks/permissions.c` | `fs/permissions.c` |
| `kernel/ntclks/object.c` | `fs/object.c` |

注：`drivers/bootstrap/storage*` 存储 facade **本阶段整体保留**（§4.3
"第一阶段整体保留 facade，后续按真实依赖拆分"）。

状态：**已完成**（NTCLKS `c48d841`：5 文件 + SOURCE_DIRS + storage_sidecar 注释；主仓 13 文件 17 处路径修复（含 ABI_PRIVATE_INVENTORY/SYSCALLS/KERNEL_USERSPACE_BOUNDARIES/ABI 四活文档）、storage_mkdir_mount_test 与procfs_directories_test 各补一桩（pty_lookup_vt_path 自 NTCLKS 5f26cab、userland_boot_cmdline 自归一化快照即缺，均为既有缺口，桩按文件既有约定：pty 类 abort()、cmdline 契约空串）。验证：G3_KERNEL_EXIT=0、NTCLKS make test 0、五个宿主消费方测试全过（tmpfs/storage_rename/storage_mkdir_mount/storage_metadata/procfs_taskmgr）。

### G4 net（4 文件）

| from | to |
| --- | --- |
| `kernel/ntclks/net.c` | `net/net.c` |
| `kernel/ntclks/net_control.c` | `net/net_control.c` |
| `kernel/ntclks/net_packet.c` | `net/net_packet.c` |
| `kernel/ntclks/net_udp.c` | `net/net_udp.c` |

状态：**待实施**。

### G5 exec（4 文件）

| from | to |
| --- | --- |
| `kernel/ntclks/user/elf.c` | `kernel/exec/elf.c` |
| `kernel/ntclks/user/usercopy.c` | `kernel/exec/usercopy.c` |
| `kernel/ntclks/user/usercopy_task.c` | `kernel/exec/usercopy_task.c` |
| `kernel/ntclks/user/userland.c` | `kernel/exec/userland.c` |

注：§4.3"user 中机制放 exec，策略另拆"——本组把 user/ 整体归 exec 位；
策略/机制拆分是后续重构，不在本组。引用修复：`tools/test_service_marker.py`、
`tools/test_linux_ioctl_cloexec.py`（userland.c 路径）。

状态：**待实施**。

### G6 console（1 文件）

| from | to |
| --- | --- |
| `kernel/ostui/ostui.c` | `drivers/console/ostui.c` |

构建旋钮：`KERNEL_SOURCE_DIRS` 的 `kernel/ostui` → `drivers/console`。

状态：**待实施**。

### G7 debug（1 文件）

| from | to |
| --- | --- |
| `kernel/kerneldebug/kerneldebug.c` | `debug/kerneldebug.c` |

构建旋钮：`mk/boot.mk` `KERNELDEBUG_SYS` 依赖路径 → `debug/kerneldebug.c`；
`KERNEL_SOURCE_DIRS` 增 `debug`（并入内核枚举无害——kerneldebug.c 只随
boot.mk 独立编译为 ET_REL 模块；为避免双编译，**不**加入 KERNEL_SOURCE_DIRS，
沿 boot.mk 单独规则）。更正：kerneldebug.c 是 boot.mk 单独编译的 ET_REL 模块
（非内核对象），故只改 boot.mk 依赖路径，不入 KERNEL_SOURCE_DIRS。

状态：**待实施**。

## 2. 每组测试节奏（诚实声明）

- 每组：NTCLKS 内核全量重建（对象路径变化即全量重编）+ NTCLKS `make test`
  + 命中该组路径的主仓消费方测试（如 G1 的 test-incremental/test-jobs、
  G5 的 test_service_marker/test_linux_ioctl_cloexec）。
- 全部组完成后：主仓全量 `make test` + python tools 批 + privilege negatives
  + 契约测试一次收口（与 M1 验证链同一套），作为整体回归证据。
- 逐组的内核构建退出码与消费方测试结果记录在各组提交信息中。

## 3. 明确不动（后续组/后续工作）

- `kernel/ntclks/` 平铺核心与 syscall/信号/同步文件（kernel.c、syscall*.c、
  signal*.c、futex/lock/wait/time*、version、uts、random、shm 等）→ 将归
  `kernel/`（§4.2 kernel/ 职责）；`sched/`、`lib/` 同理（kernel/、lib/）。
  本清单 7 组不含它们（交接"不要求一次移动所有文件"）。
- `gpu.c`、`input.c`、`pty.c`、`driver_manager.c` 等驱动面文件的职责归位
  （drivers/）留待后续组。
- `kernel/ntclks/include/ntclks/`（57 私有头）→ `include/ntclks/`：属 §5
  头文件与 ABI 边界工作（逐文件分类表），不混入本重排。
- `drivers/bootstrap/storage*` facade 保留（§4.3 第一阶段）。
- 历史阶段文档（03/07/09 等）中的旧路径引用是当时事实记录，不回改；
  活文档（ABI_PRIVATE_INVENTORY.md、README.md）随组更新。

## 4. 验证级别

每组声称"已编译"（内核重建退出码）与"消费方测试通过"；整体回归见 §2 收口
证据。本清单不声称任何 guest 运行结论；guest 级验证是 §5.8 整机验收的事。
