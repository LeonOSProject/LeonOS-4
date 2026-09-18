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

## 4. 契约测试（P1-c）

`tests/build/test-bootstrap.sh` 已写入，内容是计划第 4 节命令契约与第 6.3 节输出目录安全契约：
`make`（默认 goal）在无编译器 PATH 下也必须出 help、help/clean 不得生成输出树或改动受跟踪生成物、
HOSTCC 与目标 CC 独立、未知 ARCH/PROFILE、`O=` 空值、`O=/`、`O=.`、含空格、含换行的 O、
未知 `CONFIG_*` 命令行覆写、无所有权标记的 clean、`doctor` 在目标链接器/编译器缺失时非零退出。

**该测试当前是红的**：仓库根目录没有 `Makefile`（本轮探索性的入口与 `mk/host.mk` 在确认无法在同一
预算内做到可验证完成后被删除，没有留下半成品）。它**尚未接入** `make test-build`，所以不会把失败
伪装成通过。

## 5. 验收矩阵状态

| 编号 | 本轮状态 |
| --- | --- |
| A01–A17 | **全部未运行**。没有任何一项新构建验收通过；不存在可构建的新 Make 入口 |

与矩阵相关、本轮**已取得的先验证据**：A02 与 A17 的旧系统基线为"不成立"（第 2.1 节）；
A13 的阻塞点已定位到 `tools/apk_distribution.py:421` 的 `time.time_ns()` 包版本与
`tools/make_image.py:114,128` 的 `uuid.uuid4()`；A06 的阻塞点定位到 `build.py:1426,1436` 的
`ar rcs` 追加式归档；A16 的违例集中在 72 处 Python 调用点，其中 `leonos-musl-cc` 与 PAM 的
meson/ninja 是最硬的两处。

## 6. 本轮发现的需要修的计划/环境冲突

`.gitignore` 第 1 行的 `build/` 会匹配**任意层级**名为 `build` 的目录，导致计划第 5、12、15 节要求的
`docs/build/migration-inventory.md`、`docs/build/legacy-removal.md`、`docs/build/verification.md`
和 `tests/build/` 契约测试目录**全部无法提交**。已用 `!docs/build/`、`!tests/build/` 加注释修复，
并对两条路径分别验证 `git check-ignore` 不再命中。任何后续新增的 `*/build/` 目录都会踩同一个坑，
P4 整理 `.gitignore` 时应把首行改成锚定根目录的 `/build/`。

## 7. 已知限制

- 基线不完整：`all` 聚合目标的重跑在本文写就时仍未结束，缺有效数据。
- 没有 QEMU 运行证据：本轮未启动任何镜像。
- P1 的 Make 入口、`leonos-config`、`leonos-version`、`leonos-boot-logo` 均未交付；`P2–P5` 未开始。
- `leonos-emit` 与 `tools/host/common/` 只有直接 `gcc` 编译与手工/单元测试证据，
  **没有 `make tools` / `make test-tools` 目标**，因此不算已接入构建系统。
- `docs/build/research/` 三份底稿是未复核的调研稿，其中两条论断已确认有误（见
  `migration-inventory.md` 第 5 节），不得当验收证据引用。
- 本轮改动未推送、未合并、未发布镜像。
