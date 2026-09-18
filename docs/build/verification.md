# LeonOS 4 构建迁移验证记录（P0 与 P1 局部）

配套计划：`docs/superpowers/plans/2026-09-19-make-c-build-rewrite.md`
分支：`xiaobai/dev/buildsystem`，基线提交 `6be4c69`
记录时间：2026-09-19（宿主 Linux 7.2.4-3-cachyos-bore-lto，x86_64）

本文只写**实际执行过并看到输出**的检查。第 5 节逐条列出未执行项。凡未执行的验收编号，一律写"未运行"，
不以 skip 状态冒充通过（计划第 13 节要求）。

## 1. 环境与依赖探测

命令：`make --version`、`gcc --version`、`clang --version`、`qemu-system-x86_64 --version`、
逐工具 `command -v`、`git submodule status`、`df -h .`

结果：GNU Make 4.4.1；GCC 16.2.1；Clang 22.1.8；QEMU 11.1.1；`ld.lld`、`llvm-ar`、`llvm-objcopy`、
`llvm-config`、`xorriso`、`mformat`、`mkfs.fat`、`qemu-img`、`curl`、`flock`、`gperf`、`autoconf`、
`automake`、`libtoolize`、`flex`、`bison`、`msgfmt`、`pkg-config`、`patch`、`tar`、`xz`、`install`
均存在。

两点必须记进 `make doctor` 的设计：`mke2fs` 解析到
`/opt/android-sdk/platform-tools/mke2fs`（不是 e2fsprogs 的系统路径）；`squashfs-tools` 不存在，
但当前镜像链未使用它。22 个 submodule 全部已检出，含 `third_party/kconfig-frontends`
（kconfig-frontends-3.12.0.0）与 `third_party/zlib`（v1.3.2，含 `contrib/puff/puff.c`）。

磁盘：`/home/xiaobai/Projects` 已用 312G、可用 387G；现有 `build/` 占 78G。

## 2. 旧系统行为基线（P0-b）

测量方法：`tests/build/measure.sh`，把被测命令放进独立进程组，每 0.2 秒采样该组 RSS 之和取峰值。
共享页按进程各计一次，因此数值**跨运行可比**，但不等于唯一驻留集。GNU time 未安装（首次尝试以
rc=127 全表失败），故改用此自研脚本，新旧系统将使用同一方法。

| 用例 | 命令 | 退出码 | 墙钟 | 峰值组 RSS | 日志 |
| --- | --- | --- | --- | --- | --- |
| kernel-1 | `python3 build.py -v run kernel` | 0 | 1.52 s | 150 MB | `buildsystem/logs/migration-baseline/kernel-1.log` |
| kernel-noop-again | 同上，紧接第二次 | 0 | 1.53 s | 149 MB | `.../kernel-noop-again.log` |
| sdk | `python3 build.py -v run sdk` | 0 | 146.7 s | 1.25 GB | `.../sdk.log` |
| image-vmdk | `python3 build.py -v run image-vmdk` | 0 | 193.4 s | 1.58 GB | `.../image-vmdk.log` |
| image-iso | `python3 build.py -v run image-iso` | 0 | 198.4 s | 1.64 GB | `.../image-iso.log` |
| installer | `python3 build.py -v run installer` | 0 | 266.7 s | 1.60 GB | `.../installer-retry.log` |
| all | `python3 build.py -v run all` | 重跑进行中 | — | — | `.../all-retry.log` |

`image-iso` 日志尾部为 `Started 6 tasks / Entered 159 folders / Built 563 files / Generated 424
files / Ran 541 commands / Downloaded 0 files / 0 errors`。
`installer` 日志尾部为 `Started 12 tasks / Entered 162 folders / Built 565 files / Generated 427
files / Ran 545 commands / Downloaded 0 files / 0 errors`。

**首次 installer 与 all 采集的 rc=126 不是构建失败**，是我自己的工具链缺陷：批量缩进规范化用
`expand -t4 $f > $f.tmp && mv` 覆盖了 `tests/build/measure.sh`，`mv` 带上了重定向产生的 0644，
可执行位丢失，于是 `"$MEASURE"` 无法执行。已 `chmod +x` 并以新文件名重跑，上表取自重跑结果。

产物哈希（本轮旧系统实际产出）：

```
b93d66628f80796e0b837fae3c229f44176390f4f9672e64d49394cd80ac087c  build/images/leonos4.vmdk
8a672b5f5e52f29a35bee3028dd8513e12095ef8c8d9134b93d2a05c8804cf6a  build/images/leonos4.iso
87b7da68dee236932f8e0d4c787e327d6dab6d4c8b0d4043b3fd028d0d6e6f2c  build/system/kernel.sys
357275d0b76111905491f875768def6069cea5dbe5f7536e9f033d08a66f51ea  build/system/kernel.debug
2c7c89ced5b5d219fe2e13bcdd925a75ad8c5d4d121a217f9c010c2e46690479  build/images/leonos4-installer.iso
```

本轮**未在 QEMU 中启动**这些镜像，因此它们只是文件产出证据，不构成第 11 节要求的运行验证。

### 2.1 关键发现：旧系统的"重复构建"不是空操作

对 `kernel-noop-again.log` 逐行核对，第二次 `run kernel` 实际执行了 7 条命令：

```
python3 tools/generate_component_kconfig.py
python3 tools/build_info.py --header include/generated/build_info.h --state buildsystem/state/build_number.txt
python3 tools/kconfig_sync.py --config buildsystem/config/leonos.conf ...
clang -target x86_64-unknown-none -O3 ... -c kernel/ntclks/version.c -o build/obj/kernel/kernel/ntclks/version.c.o
ld.lld -nostdlib -z max-page-size=0x1000 -T kernel/ntclks/arch/x86_64/linker.ld -o build/system/kernel.unstripped <87 objects>
llvm-objcopy --strip-debug ... ; llvm-objcopy --only-keep-debug ...
```

机制在 `build.py:4213-4217`：`build_roots()` 给除 `BUILD_NUMBER_EXEMPT_TARGETS` 外的每个目标前置
`build-info`，而该目标是 `always=True`。实测两次构建把受跟踪的 `buildsystem/state/build_number.txt`
从 3833 推到 3836，并重写受跟踪的 `include/generated/build_info.h`。

→ 结论：A02（空操作不重编译）与 A17（受跟踪文件零变化）在旧系统上**本来就不成立**。新系统必须做到，
而不是"沿用旧行为"。

## 3. 新 C 宿主工具（P1-a，TDD）

### 3.1 红灯

命令：`gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wshadow -Wstrict-prototypes
-Wmissing-prototypes tests/host/test_common.c`

首次输出（正确的失败原因——生产实现不存在）：

```
tests/host/common/... fatal error: ../../tools/host/common/buffer.h：没有那个文件或目录
```

红灯过程中还暴露了测试脚手架自身的一个真实缺陷：`test-support.h` 检查 `rewind()` 返回值，
而 `rewind()` 返回 `void`，`-Werror` 下编译失败。已改为 `fseek(stream, 0, SEEK_SET)`。

### 3.2 绿灯

`tools/host/common/{buffer,io,process}.{c,h}` 实现后：

| 检查 | 命令 | 结果 |
| --- | --- | --- |
| GCC 严格警告集 | `gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wshadow -Wstrict-prototypes -Wmissing-prototypes` | 编译无输出，`ok - host/common: 74 checks passed`，rc=0 |
| Clang 同一警告集 | `clang` + 同上 | `ok - host/common: 74 checks passed`，rc=0 |
| ASan + UBSan + 泄漏检测 | `gcc -g -O1 -fsanitize=address,undefined` 后 `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1` 运行 | rc=0，无 sanitizer 报告 |

覆盖的契约（计划第 8 节）：扩容保留旧内容、`SIZE_MAX` 请求被拒且旧缓冲不丢、
`buffer_destroy` 接受零值结构、按请求 mode 建文件、零字节输入产出空文件、
**内容相同则 mtime 与 inode 均不变**、内容变化则原子替换且目录内不留临时文件、
只读目录中发布失败**且原文件完好**、子进程正常退出、非零退出码进 `child_status`、
信号死亡用 `WIFSIGNALED`/`WTERMSIG` 可辨、程序不存在与非existent 工作目录返回 -1 且置 `errno`、
`working_dir` 真实生效。

### 3.3 `leonos-emit`

单独编译通过（同一严格警告集），手工契约验证：

| 场景 | 观察 |
| --- | --- |
| 首次写入 `--mode 0600` | 文件建立，权限 `-rw-------` |
| 相同内容再写一次 | mtime 纳秒级完全不变（`...08.468586876`） |
| 不同内容再写 | 内容更新，mtime 改变 |
| `--help` | 打印用法，rc=0 |
| 缺 `--output` | `leonos-emit: --output is required`，rc=1 |
| 目标目录只读 | `cannot publish ...: Permission denied`，rc=1，原文件保留 |

注意：`leonos-emit` **尚未有 Make 目标**，以上是我用 `gcc` 直接编译二进制后手工执行的。它被计入
"已编译并验证行为"，不计入"已接入构建系统"。

## 4. 新 Make 系统的 P1 结果

### 4.1 契约测试

| 套件 | 命令 | 结果 |
| --- | --- | --- |
| C 单元测试 | `make test-tools` | `ok - host/common: 74 checks passed` 两次（普通构建与 ASan+UBSan 构建各一次），rc=0 |
| 入口契约 | `sh tests/build/test-bootstrap.sh` | **20 checks, 0 failures** |
| 增量契约 | `sh tests/build/test-incremental.sh` | **20 checks, 0 failures** |

### 4.2 目标面

`make help`（PATH 中无编译器时仍可用）、`make doctor`、`make tools`、`make defconfig`、
`make kernel`、`make test-tools`、`make clean` 已实现并实测；`fetch`、`userland`、`runtime`、
`sdk`、`rootfs`、`apk-repo`、`image-vmdk`、`iso`、`installer`、`run*`、`test-smoke`、`test-legacy`
由 `scripts/not-migrated.sh` 显式 **exit 2**，不伪装成功。

### 4.3 验收行实测证据

| 编号 | 观察 |
| --- | --- |
| A02 | 第二次 `make kernel` 输出的 `CC/AS/LD/IMAGE/GEN/HOSTCC/CONFIG` 动作数为 **0**；`$(O)/obj/kernel` 下全部 85 个对象与 `kernel.sys`、`kernel.debug`、`kernel.unstripped`、`autoconf.h`、`boot_logo.h` 的 mtime（含纳秒）逐项不变 |
| A03 | `touch kernel/ntclks/futex.c` → 恰好 1 次 `CC`（且就是该文件）、1 次 `LD`、2 次 `IMAGE` |
| A04 | `touch kernel/ntclks/include/ntclks/types.h` → 重编译数 **恰好等于** depfile 记录了该头的对象数 **82**，既无漏编也无过编；随后 1 次 `LD` |
| A04b | `touch kernel/ntclks/arch/x86_64/linker.ld` → 0 次 `CC`、1 次 `LD` |
| A05 | `KERNEL_CFLAGS=-DLEONOS_SIG_PROBE` → 83 个 C 对象全部重建、2 个汇编对象一个都不重建；产品 mtime 改变 |
| A06 | 移走 `kernel/ntclks/signalfd.c` → 重新链接，且 `make -n` 的输出不再包含该对象；恢复后再链接一次 |
| A07 | 删除 `kernel.sys` 或删除 `boot_logo.h` → 各自被重新生成（多输出阶段无 stamp 误判） |
| A12 | `O=` 空、`O=/`、`O=.`（含解析为源码根的相对形式）、含空格、含换行、含非法字符全部被拒；`clean` 对无所有权标记的 O 拒绝执行；未知 `CONFIG_*` 命令行覆写报错 |
| A14 | `tools/host` 与 `tests/host` 在 GCC 16 与 Clang 22 的 `-std=c11 -Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wshadow -Wstrict-prototypes -Wmissing-prototypes` 下零告警；ASan+UBSan（`detect_leaks=1`、`halt_on_error=1`）无报告 |
| A17 | 新构建不改动任何受跟踪文件：`buildsystem/state/build_number.txt` 与 `include/generated/build_info.h` 哈希保持 `ebdbfc0b` / `b135f754`；跨文件系统 `O=/tmp/...` 构建后源树 `git status` 干净，且 `kconfig-frontends.sh` 会回收它留下的 `.tmpconfig.*` 与空 `include/config` |

### 4.4 产物等价性（内核）

新系统从空 `O` 产出的 `kernel.sys` 为 **1196352 字节**，与旧基线 `build/system/kernel.sys` 同尺寸。
两者不可能逐字节相同：旧基线嵌入 `LEONOS_KERNEL_VERSION "4.7.1-3838"` 与调用时刻的
`LEONOS_BUILD_TIME`，新系统按 §7 不递增计数、不读挂钟。逐字节比较需在两侧固定
`SOURCE_DATE_EPOCH` 与 `BUILD_ID` 后进行，属 A13，**本轮未做**。

`leonos-boot-logo` 与退役的 `tools/generate_boot_logo.py` 输出**逐字节一致**（仅第一行署名不同），
`diff` 只报告第 1 行；`leonos-version` 重放出同名同形状的宏集合。

## 5. 验收矩阵状态

| 编号 | 状态 |
| --- | --- |
| A02 A03 A04 A05 A06 A07 A12 A14 A17 | **内核范围内通过**（证据见 4.3；A12/A14/A17 只覆盖已实现部分） |
| A01 | 部分：干净 O 可构建，但 `fetch` 未实现，故"只装声明依赖 + 干净 clone 全目标"未达成 |
| A08 | **未运行**：`-j1` 与 `-j8` 三轮对比未做 |
| A09 | 部分：两个不同 O 可并行且互不串用（跨文件系统实验）；同 O 双进程互斥**未实现也未测** |
| A10 | **未运行**：中断、写失败、子进程失败的产物保真未做（`.DELETE_ON_ERROR` 与 `write_file_if_changed` 已就位，但未证） |
| A11 | **未运行**：依赖 `fetch` |
| A13 | **未运行**：需固定 epoch 的双干净 O 哈希比较 |
| A15 | **未运行**：SDK/APK/三类镜像与升级尚未迁移 |
| A16 | **未运行**：execve 跟踪未做。已确认可实现性：新入口的生产规则中无 `python3`，但 `fetch`/`sdk`/镜像链未迁移，无法证明全链 |
| A18 起 | 不适用 |

### 5.1 P1-d 的来宾运行验证：受阻，未通过

`make kernel` 的产物已在 QEMU 中尝试启动，但**判定不成立**，因此本项记为受阻而非通过：

1. 首轮用 `-bios` 指向不存在的路径，QEMU 直接退出；对照组同样失败，属夹具缺陷。
2. 次轮不带固件（SeaBIOS）：`control`（旧 `build/system/kernel.sys`）与 `candidate`
   （新 `out/.../kernel.sys`）的串口日志**完全同形**，都停在
   `Booting 'LeonOS 4 Live Desktop (musl + Vim + GCC)'` 后重新回到 GRUB 横幅。
3. 三轮用 `/usr/share/edk2/x64/OVMF.4m.fd`：对照组的串口变成 EDK2 自身的异常寄存器转储
   （`Find image based on IP(...)`、`CR0/CR3/IDTR` 等），说明该固件镜像与 q35 + 这张
   `--bios` 生成的 ISO 组合不可用。

新旧内核在所有配置下行为一致，因此**没有回归证据**，但也**没有成功启动证据**。
按 AGENT.md 第 7 节"编译通过不等于镜像已含文件；镜像已生成不等于虚拟机能启动"，
以及计划第 11 节"测试必须识别来宾成功标记并核验退出状态，启动 QEMU 进程本身不算通过"，
本项必须等 `mk/run.mk` + QMP/截图判定（P3）落地后重做。用于对照的两张 ISO 与临时产物已删除。
## 6. 发现并已修的计划/环境问题

`.gitignore` 第 1 行的 `build/` 会匹配**任意层级**名为 `build` 的目录，导致计划第 5、12、15 节要求的
`docs/build/migration-inventory.md`、`docs/build/legacy-removal.md`、`docs/build/verification.md`
和 `tests/build/` 契约测试目录**全部无法提交**；本轮新建的 `tools/build/` 是第三次踩坑。
已把首行改为锚定的 `/build/` 并移除两条临时反选，验证 `docs/build`、`tests/build`、`tools/build`
不再被忽略，而 `build/linux-6.12/tools/build` 仍随 `/build/` 一起被忽略（`git status` 无新增噪声）。

## 7. 已知限制

- 基线不完整：`all` 聚合目标的重跑在本文写就时仍未结束，缺有效数据。
- 没有 QEMU 运行证据：本轮未启动任何镜像。
- P1 的 Make 入口、`leonos-config`、`leonos-version`、`leonos-boot-logo` 均未交付；`P2–P5` 未开始。
- `leonos-emit` 与 `tools/host/common/` 只有直接 `gcc` 编译与手工/单元测试证据，
  **没有 `make tools` / `make test-tools` 目标**，因此不算已接入构建系统。
- `docs/build/research/` 三份底稿是未复核的调研稿，其中两条论断已确认有误（见
  `migration-inventory.md` 第 5 节），不得当验收证据引用。
- 本轮改动未推送、未合并、未发布镜像。
