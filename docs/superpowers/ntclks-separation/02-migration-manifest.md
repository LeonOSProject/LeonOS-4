# 迁移清单：内核输入 → 目标所有者（阶段 0/2）

日期：2026-09-25，基于 `refactor/ntclks-separation@30b1db6` 的构建图静态分析。
覆盖制品：`kernel.sys`、`kernel.debug`、`loader.elf`、五个 `.drv`（mouse/serial/
e1000/ac97/es1371）、`kerneldebug.sys`。逐文件枚举以本文目录/计数为准，细节以
`git ls-files` 为准；原始分析日志见 `/home/leon/build/ntclks-sep/logs/phase0/`。

## 0. 构建图入口

| 片段 | 角色 | 分离动作 |
| --- | --- | --- |
| `Makefile` | 变量（ARCH/PROFILE/O/TOOLCHAIN/SOURCE_DATE_EPOCH）、构建锁、目标 `kernel/tools/config-sync/build-info` | 子仓自有入口；父仓保留产品入口 |
| `mk/logging.mk`、`mk/host.mk`、`mk/toolchain.mk` | 日志、HOSTCC 工具、命令签名、工具链变量 | 两仓各持一份（工具链描述 `configs/toolchains/llvm-x86_64.mk` 归内核） |
| `mk/config.mk` | Kconfig 前端、`.config`、`autoconf.h` | 内核自有 schema；组件映射归父仓 |
| `mk/kernel.mk` | kernel.sys+kernel.debug（编译 ntclks/ostui/bootstrap 三树） | 迁入内核仓；父仓变适配器 |
| `mk/boot.mk` | loader.elf、五个 .drv、kerneldebug.sys | 迁入内核仓；父仓只消费制品 |
| `mk/resources.mk:6-11` | `cjk_font.h` 生成且挂在 `framebuffer.c.o` 上 | **跨界**：生成规则随内核（内含 unifont 依赖闭包） |
| `mk/userland.mk:7-13` | parse-time `include components.mk` | **跨界**：`make kernel` 也生成并解析用户态组件数据，子仓切断 |

## 1. 逐制品输入清单

### 1.1 kernel.sys + kernel.debug（一次链接的两个 objcopy 视图）

- 链接脚本：`kernel/ntclks/arch/x86_64/linker.ld`（自包含，无 INCLUDE）。
- 源码：`find kernel/ntclks kernel/ostui drivers/bootstrap`（排除
  `drivers/bootstrap/storage/*`）——**84 个翻译单元**（82 .c + 2 .S）：
  - `kernel/ntclks/arch/x86_64/`（11）：apic arch boot.S gdt idt irq paging pci power smp smp_trampoline.S
  - `kernel/ntclks/` 顶层（42）：cpuinfo driver_manager futex gpu heap input kernel kernel_debug
    lock multiboot2 net net_control net_packet net_udp object page_cache permissions platform
    procfs pty random shm signal signal_queue signalfd syscall syscall_clone syscall_device
    syscall_file_io syscall_fs syscall_gui syscall_ipc syscall_locks syscall_mm syscall_process
    syscall_process_vm syscall_security syscall_socket syscall_socket_batch syscall_sysv_msg
    syscall_sysv_sem syscall_time sysfs time time_discipline tmpfs uts version wait
  - `kernel/ntclks/lib/`（4）：bugcheck panic string text_utf16
  - `kernel/ntclks/mm/`（1）、`kernel/ntclks/sched/`（1）、`kernel/ntclks/user/`（4）：
    mm.c、sched.c、elf.c usercopy.c usercopy_task.c userland.c
  - `kernel/ostui/`（1）：ostui.c
  - `drivers/bootstrap/`（13）：console efi_fs framebuffer storage（facade）usb_uhci vga
    svga/{device,fifo,gb,gmr,render,svga3d,triangle}
- 文本包含（非独立编译）：`drivers/bootstrap/storage/*.c` 23 模块 + `storage_internal.h`
  （由 storage.c facade 包含；清单剪枝 + facade 显式依赖 + depfile 三重保证）；
  `kernel/ntclks/sched/eevdf.inc`、`drivers/bootstrap/storage/storage_exfat_upcase.inc`。
- include 顺序：`-I$(O_INCLUDE) -Ikernel/ntclks/include -Iinclude/uapi -Iinclude`
  （未用 `-nostdinc`，stdint 等来自 clang resource dir）。
- 头：`kernel/ntclks/include/ntclks/` 全部 57 个 + arch/x86_64 私有 3 个（idt/port/
  keyboard_led）+ drivers/bootstrap/svga 私有 5 个；`include/uapi/` 全部 51 头；
  `include/leonos/` 消费 20 头（audio auth boot_handoff device driver elf_abi fb fs gpu
  inputm kernel_debug layout net psf_font pty signal startup system text utf8_stream）。
  `include/linux/` 被 `-Iinclude/uapi` 遮蔽，可留在父仓。**不使用** userland/libc/include
  与 devtools/include。
- 资源：`include/leonos/lat15_vga16_psf.inc`（VGA 字体数据，经 psf_font.h 内嵌，
  loader/kernel/drivers 共用；`lat15_vga16.psf` 是惰性参考）；生成的
  `cjk_font.h`（仅 framebuffer.c 消费，需 unifont 缓存）。
- 生成头：`autoconf.h`（每个 TU 强制包含）、`build_info.h`（version.c）、
  `cjk_font.h`。**勿用**已提交的 `include/generated/autoconf.h`（陈旧快照）与
  `rustcfg.args`（Rust 遗留）。
- 工具链：`KERNEL_CC_BASE`（clang -target x86_64-unknown-none … -mcmodel=kernel）、
  `KERNEL_AS_BASE`、`ld.lld -nostdlib -T linker.ld`、`llvm-objcopy --strip-debug/
  --only-keep-debug`。对象在 `$(O_OBJ)/kernel/<src>.o`，清单 sources.list/sources.mk。

### 1.2 loader.elf

- 源码（2）：`boot/loader/boot.S`、`boot/loader/main.c`；链接脚本 `boot/loader/linker.ld`。
- include：`-I$(O_INCLUDE) -Iinclude/uapi -Iinclude`（无 kernel/ntclks/include）。
- 头：`leonos/boot_handoff.h`、`leonos/psf_font.h`（→lat15_vga16_psf.inc、layout.h→rootfs.h）。
- 生成头：`autoconf.h`、`loader_integrity.h`（kernel.sys 的 SHA-256）。
- Kconfig：`CONFIG_LOADER_VERIFY_SHA256`（唯一被 Ring-0 C 读取的符号）。

### 1.3 五个 .drv

- 源码各 1：`drivers/{mouse,serial,e1000,ac97,es1371}/*.c`；用 `KERNEL_CC_BASE`
  编译（即拥有整个 ntclks include 面）；链接 `ld.lld -r` 可重定位，无链接脚本。
- 依赖签名 `kernel-cc.sig`/`kernel-link.sig`（内核标志变化使其失效）。
- **事实**：`KERNEL_CFLAGS` 未用于 .drv 与 kerneldebug（但签名包含它）——拆分时
  保持或有意识修复。

### 1.4 kerneldebug.sys

- 源码（1）：`kernel/kerneldebug/kerneldebug.c`（内含 ELF note "LEONKDBG"）；
  头 `ntclks/kernel_debug.h`→`leonos/{boot_handoff,kernel_debug}.h`+`ntclks/types.h`。
- 单文件编译 + objcopy 重命名 `.note.leonos.kerneldebug` 为 alloc/load 段。
- 是 rootfs stage（mk/rootfs.mk:24）的前置，不是任何 Ring-0 链接的前置。

## 2. 共享基础设施输入（随内核仓迁移）

- 构建片段：`mk/{logging,host,toolchain,config,kernel,boot}.mk` + resources.mk 的 cjk 规则。
- 工具链描述：`configs/toolchains/llvm-x86_64.mk`（TRIPLE_KERNEL=x86_64-unknown-none、
  TRIPLE_USER=x86_64-linux-musl、clang/lld/llvm-objcopy）。
- Kconfig：`Kconfig`、`Kconfig.components`、`configs/default.conf`。
- 版本/锁：`configs/build-version`（kernel_name=ntclks、release_version=4.7.2）、
  `configs/dependencies.lock.json`（unifont 条目）、`resources/licenses/unifont-LICENSE`。
- 脚本：`scripts/{build-lock,logging}.sh`、`tools/build/{loader-integrity,kconfig-frontends,fetch}.sh`。
- 宿主 C 工具（6 个 + 公共）：leonos-emit、leonos-config、leonos-version、
  leonos-deps(+json.c)、leonos-cjk-font、leonos-components（仅 parse-time 用，切断
  components 耦合后可弃）；`tools/host/common/{buffer,io,process}.c/.h`。
- 第三方：`third_party/kconfig-frontends`（submodule，autotools 构建）、
  `third_party/zlib/contrib/puff/puff.c`（仅存在性门禁）。
- 缓存：`cache/downloads/unifont_all-16.0.04.hex.gz`（fetch 校验；构建期只 `--verify-only`）。
- git 元数据（软依赖，有回退）：`rev-parse --short HEAD`→build_info、`show -s %ct`→SOURCE_DATE_EPOCH。

生成物闭环：`.config`（kconfig-conf）→ `autoconf.h`（leonos-config）→ 每 TU；
`build_info.h`（leonos-version）；`loader_integrity.h`（loader-integrity.sh+leonos-emit）；
`cjk_font.h`（leonos-cjk-font+gzip+leonos-deps+fetch.sh --verify-only）；
`components.mk`（leonos-components，parse-time，用户态耦合点）。

## 3. 跨界依赖（迁移风险，逐项处置）

| # | 事实 | 处置 |
| --- | --- | --- |
| 1 | 内核规则不读 userland/system/devtools/tests/tools/resources | 已核实；但 `tests/build/test-{pages,reproducible,concurrency,incremental,rpr-packages}.sh`、`tests/long/test-jobs.sh` **消费**内核制品/路径，随迁移改指 |
| 2 | mk/kernel.mk 三树合编（ntclks+ostui+bootstrap） | 子仓保持合并链接；按设计 4.2 目录归位 |
| 3 | storage facade 文本包含 23 模块 | 第一阶段整体保留 facade（设计 4.3） |
| 4 | parse-time 读用户态组件数据（mk/userland.mk:13） | 子仓删除该耦合；组件映射归父仓 |
| 5 | cjk_font.h 在 mk/resources.mk 却是 kernel 前置 | 生成规则+unifont 闭包随内核（fetch.sh/leonos-deps/cache/license） |
| 6 | 新 O 必构建 kconfig-frontends（autotools） | 子仓保留；文档注明 |
| 7 | 多处 parse-time `$(shell …)` 探测（grub-mkstandalone、msgfmt 等） | 子仓不引入这些片段即消除 |
| 8 | git 是软输入（有 fallback unknown/0） | 保持 |
| 9 | `KERNEL_CFLAGS` 不作用于 .drv/kerneldebug，却在其签名中 | 保持现状进入阶段 2，阶段 3 决定是否统一 |

## 4. loader-integrity 链（内核/loader 配对的构建体现）

`mk/boot.mk:9-11`：`loader_integrity.h` 依赖 `kernel.sys`+`leonos-emit`+
`tools/build/loader-integrity.sh`。脚本对 kernel.sys 做 sha256sum，生成
`LEONOS_LOADER_KERNEL_SHA256[32]`，经 leonos-emit 内容变更才发布。`boot/loader/main.c`
在 `CONFIG_LOADER_VERIFY_SHA256` 下比对装载的内核（main.c:1858,1881）。
**顺序后果：loader.elf 必须等 kernel.sys 定稿；kernel.sys 本身独立于 loader。**
对应运行时更新契约（RPR format_version=2、kernel+loader 成对切换）见
`docs/KERNEL_USERSPACE_BOUNDARIES.md`。

## 5. 生产链非 C 工具审计

**无 Python**（Makefile/mk/*.mk/链上脚本 grep 验证）。实际使用：POSIX sh、coreutils/
findutils/sed/awk、sha256sum、gzip、flock、tar、git（软）、kconfig-frontends 构建的
autotools（perl 仅 no-op 探测）。HOSTCC=cc；目标 clang/lld/llvm-objcopy。

## 6. 目标与变量（子仓入口需复制的语义）

- 变量：`ARCH`（仅 x86_64）、`PROFILE=release|debug`、`O`、`TOOLCHAIN`、
  `SOURCE_DATE_EPOCH`、`V`、`HOSTCC`、命令行 `CC/CXX/AR/LD/OBJCOPY/STRIP` 覆盖、
  `LEONOS_OPTIMIZATION_FLAGS`（-O3 / -O0 -g）、`KERNEL_{CFLAGS,AFLAGS,LDFLAGS}`。
- 目标：`kernel`（Makefile:205）、`loader`、`drivers`、`boot`（mk/boot.mk:50-53）、
  `tools`、`defconfig/olddefconfig/menuconfig`、`config-sync`、`build-info`、
  `clean/distclean`。下游消费（不属输入）：mk/images.mk:18-19（ESP 装 loader.elf+
  kernel.sys+GRUB）、mk/rootfs.mk:24（.drv+kerneldebug.sys）、mk/rpr.mk:14-15。

## 7. 子仓最小文件集（阶段 2 迁移底稿）

```
kernel/            （全部，含 kernel/kerneldebug/）
drivers/           （全部，含 bootstrap/storage*、bootstrap/svga/）
boot/loader/       （仅 boot.S main.c linker.ld；boot/grub* 留父仓）
include/uapi/      （全部 51 头 + README）
include/leonos/    （§1.1 的 20 头 + lat15_vga16_psf.inc/.psf；其余按阶段 1 分类表拆分后归位）
mk/{logging,host,toolchain,config,kernel,boot}.mk + resources.mk 的 cjk 规则
configs/{default.conf,build-version,components.toml(去耦后删),dependencies.lock.json,toolchains/llvm-x86_64.mk}
Kconfig  Kconfig.components
tools/host/{common,gen,config,version,manifest,assets/leonos-cjk-font.c}
tools/build/{loader-integrity.sh,kconfig-frontends.sh,fetch.sh}
scripts/{build-lock.sh,logging.sh}
resources/licenses/unifont-LICENSE
third_party/kconfig-frontends（submodule）、third_party/zlib/contrib/puff/puff.c（门禁）
cache/downloads/unifont_all-16.0.04.hex.gz（或移植 fetch）
LICENSE、NOTICE（许可证随迁，设计 §2/§8 要求）
```

宿主工具需求：leonos-emit、leonos-config、leonos-version、leonos-deps、leonos-cjk-font、
leonos-components（去耦后可删）。
