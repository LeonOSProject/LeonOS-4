# 阶段 0：基线与证据

日期：2026-09-25。工作树：`/home/leon/projects/c/LeonOS-4-ntclks-separation`，
分支 `refactor/ntclks-separation`，HEAD `30b1db65f01b3cc3e1f56009e32b063c1055e875`
（= main；调研基线 `7123bff` 之后已有文档/任务管理器等提交，已在下文注明差异）。
构建输出 O=`/home/leon/build/ntclks-sep/baseline`（独立绝对路径）；
日志：`/home/leon/build/ntclks-sep/logs/`。

## 1. 环境与前置

- `make doctor`：全部通过（clang/ld.lld/llvm-objcopy、mke2fs/mkfs.fat/xorriso、
  qemu-img/qemu-system-x86_64/grub-mkstandalone、libext2fs、OVMF.fd）。
  worktree 需 `git submodule update --init --recursive`（本机从 .git/modules 本地克隆）。
- 主机回归依赖：tesseract、`/dev/kvm`、`/usr/share/edk2/x64/OVMF*.4m.fd` 齐备。
- worktree 的 `cache/`、`buildsystem/` 以符号链接共享主检出的下载缓存（189M + 3.8G，
  只读消费；`.gitignore` 已忽略两目录，不会入库）。
- 工具链默认 `configs/toolchains/llvm-x86_64.mk`（clang、ld.lld、llvm-objcopy，
  TRIPLE_KERNEL=x86_64-unknown-none，TRIPLE_USER=x86_64-linux-musl）。

## 2. 基线完整构建（已编译 + 已打包）

```
make O=/home/leon/build/ntclks-sep/baseline defconfig
make O=/home/leon/build/ntclks-sep/baseline fetch
make O=/home/leon/build/ntclks-sep/baseline -j$(nproc) all      → EXIT=0
```

制品齐全：`generated/system/{kernel.sys,kernel.debug,kerneldebug.sys}`、
`generated/boot/loader.elf`、`generated/drivers/{mouse,serial,e1000,ac97,es1371}.drv`、
`images/{leonos4.raw,leonos4.vmdk,leonos4-live.iso,leonos4-installer.iso,root.ext2,
disk-root.ext2,installer-root.ext2}`。日志：`logs/baseline-build.log`。

## 3. 基线测试结果（源码/构建层验证，非 guest 运行）

| 测试 | 命令 | 结果 |
| --- | --- | --- |
| make test（test-tools+test-build，含 ASan/UBSan 宿主工具测试与 34 个 shell 契约测试） | `make O=... test` | **EXIT=0**（`logs/baseline-make-test.log`） |
| UAPI 归属与自包含 | `python3 tools/test_uapi.py` | **PASS**：53 个 UAPI 头独立 C/C++ 编译通过，syscall 表与生成器一致 |
| musl ABI 布局 | `python3 tools/test_musl_abi.py --prefix $O/sysroot/musl` | **PASS**：Linux v6.12 数值/共享 UAPI 布局比较通过 |
| console 启动策略 | `python3 tools/test_console_boot_policy.py` | **6/6 OK**（inittab 六 VT、console-session 顺序、setsid/TIOCSCTTY 等静态断言） |
| installer 输入回归 | `python3 tools/test_installer_input.py` | **11/11 OK**（挂载过渡、键盘 LED ioctl、块设备 errno 等宿主单测） |

## 4. 基线 QEMU guest 回归（真实运行验证；独立 scratch 磁盘，未触碰用户镜像）

| 测试 | 状态 | 证据 |
| --- | --- | --- |
| `tools/test_vt_qemu.py`（live/已装镜像 VT 切换、登录、stdio、Desktop 暂停恢复、登出） | **PASS（VT_EXIT=0）** | `logs/baseline-vt-tests.log`、`logs/vt-qemu/{serial.log,qemu.log,*.png}` |
| `tools/test_vt_installer_qemu.py --install`（installer GUI、tty1-6、全新盘安装） | **PASS**（`--install-timeout 3600`） | `logs/baseline-vt-tests-b2.log`、`logs/vt-installer-b2/` |
| `tools/test_vt_installed_qemu.py --disk …`（安装后系统登录/六 VT/tty 归属） | **PASS** | 同上、`logs/vt-installed-b2/` |

失败归类记录：首轮 `--install` 在测试硬编码 1200s 上限处超时（`logs/vt-installer/`），
画面证据显示安装正常推进至约 85%（拷贝 terminfo，串口 busybox 拷贝循环活跃），
判为**夹具时限短于本机安装时长（约 25 分钟）**，非产品缺陷。按 executing-plans
"测试有问题→修复测试并说明"，为 `test_vt_installer_qemu.py` 增加显式
`--install-timeout` 参数（默认 1200 不变），以 3600 重跑通过。该夹具参数变更
与两轮日志均已留档，未以放宽时限掩盖产品失败。

说明：`test_vt_qemu` 覆盖 Ctrl+Alt+F1~F6、tty2 登录、`test -t 0/1/2`、controlling tty
（`tty`=tty2）、Desktop STOP/CONT 暂停下的 VT 切换恢复、GUI 登录（session-user 标记）、
杀 desktop 后回登录、getty 重生成。`test_console_boot_policy` 是静态断言，不替代 guest 验证。

## 5. 测试入口盘点（执行前已读参数与 fixture）

| 入口 | 参数/fixture | 备注（迁移影响） |
| --- | --- | --- |
| `tools/test_uapi.py` | 无参数；依赖 `generate_linux_syscalls.py` | 硬编码 `kernel/ntclks/include/ntclks/syscall.h`、`userland/libc/include/leonos/syscall.h`（阶段 1/2 改） |
| `tools/test_musl_abi.py` | `--prefix`（默认 `build/musl/sysroot`） | 需真实 musl sysroot（本基线用 `$O/sysroot/musl`） |
| `tools/test_console_boot_policy.py` | 无参数 | 编译 `tools/tests/vt_console_test.c` 用 `-Ikernel/ntclks/include`（迁移改） |
| `tools/test_installer_input.py` | 无参数 | 编译 `tools/tests/*.c` 用 `-Ikernel/ntclks/include`、`-idirafter userland/libc/include`（迁移改） |
| `tools/test_vt_qemu.py` | 环境 `VT_IMAGE`/`VT_OUTPUT`/`VT_TEXT_ONLY`/`VT_SNAPSHOT`/`VT_PROBE` | 默认镜像路径 `out/x86_64/release/images/leonos4.raw`；需 tesseract+KVM |
| `tools/test_vt_installer_qemu.py` | `--output`（必须不存在）、`--iso`、`--tui`、`--install` | 自建 4G scratch.raw；`--install` 走完整安装（≤20min） |
| `tools/test_vt_installed_qemu.py` | `--disk`、`--output`（必须不存在） | 消费安装器产出的磁盘；root 口令 rootpass、用户 alice/password |

`make test` 只聚合 test-tools/test-build；上述 Python/QEMU 回归不在其中（设计 §2 已注明）。

## 6. 跨树引用基线（迁移必须逐项消除/改指）

`rg -n 'kernel/ntclks|drivers/bootstrap|include/uapi' mk tests tools .github` →
276 处（`logs/phase0/cross-tree-refs.txt`）。主要分布：mk/kernel.mk、mk/boot.mk、
mk/{userland,runtime,sdk,resources}.mk、tools/analyze_boot_log.py（内核源位置映射）、
tools/vscode/generate_compile_commands.py、tools/build/{upstream-app,musl-sdk,pam-stage}.sh、
tests/long/test-jobs.sh（硬编码内核源路径作 touch 目标）、tests/host/test_motd.c
（`#include "kernel/ntclks/include/ntclks/loadavg.h"`）等。

## 7. 与设计文档的实际差异（更新设计用）

1. **`KERNEL_CFLAGS` 不作用于 .drv 与 kerneldebug**（mk/boot.mk:37,46），但其失效签名
   `kernel-cc.sig` 包含它（mk/kernel.mk:45）——拆分时保持现状，阶段 3 决定是否统一。
2. **`include/linux/` 被 `include/uapi/linux/` 遮蔽**（include 顺序），内核不依赖前者；
   13 个转发壳仅服务旧路径兼容。
3. **用户态 include 遮蔽**：`-Iuserland/libc/include` 先于 `-Iinclude`，8 个同名头中
   4 个（devmgr_service/inputm/net_service/text_input）用户态实际用 libc 分叉副本——
   混合头拆分前必须先合一，否则会静默改变用户态编译内容。
4. **反向依赖实锤**：`include/uapi/leonos/net_control.h` → `leonos/net.h`（wire struct
   住在非 UAPI 头）；设计 §5 的原则已含，但具体拆法需 net wire 迁入 UAPI。
5. **cjk_font.h 的跨界闭包**：生成规则在 mk/resources.mk，却是 kernel 前置；unifont
   锁条目、`resources/licenses/unifont-LICENSE`、fetch.sh/leonos-deps 均需随内核仓
   （设计 §4.2 的 resources/ 未展开此闭包）。
6. **parse-time 用户态耦合**：mk/userland.mk:13 硬 include `components.mk`，
   `make kernel` 也生成并解析用户态组件数据——子仓必须切断（设计 §7 已有原则）。
7. **死代码先例**：内核 startup/autostart 策略块（syscall.c:2763-3176）与 auth 会话块
   已 DEAD、由 sessiond 取代；`task->role` 活代码恒 NONE，M10 磁盘门禁实际=installer-root
   旁路——重构须逐字保留（详见 03-permission-matrix.md §4）。
8. **autospawn 子串匹配弱点**（`autospawn=helloworld` 触发 `hello`）与 **大小写变体路径
   可获 SERVICE 标记**（M1）是现有弱点；按计划不得以删检查方式"修复"，负例先行。
9. README/文档中旧 `build.py` 命令确已非生产入口（AGENTS 约束与现状一致）。

## 8. 未做/待补

- `make test-long`/`make test-smoke`/`make test-legacy` 未在基线执行（长耗时；
  test-smoke 即 QEMU 启动测试，其覆盖已由 VT 套件部分等价；test-legacy 含 Python 参考实现）。
- debug profile 构建未做（计划要求 release/debug 都保；阶段 3 验证）。
- 可重现性双路径构建比较未做（阶段 3 验证项）。
