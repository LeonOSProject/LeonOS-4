# 双路径可重现性比较（阶段 3 验证项）

日期：2026-09-25。比较两个不同绝对路径、相同输入（同提交 `9eff3c3`，
SOURCE_DATE_EPOCH=提交时间 1790338572）的完整 `make all` 产物。

设置：A = `/home/leon/projects/c/LeonOS-4-ntclks-separation`（O=`…/repro-a`），
B = `/home/leon/projects/c/LeonOS-4-repro`（O=`…/repro-b`，detached 同提交 worktree）。
原始对比日志：`/home/leon/build/ntclks-sep/logs/repro-compare.log`。

## 1. 逐字节一致（可重现）

- **全部 Ring-0 制品**：`kernel.sys`、`kernel.debug`（含调试信息）、
  `kerneldebug.sys`、`loader.elf`、五个 `.drv`。
- **第一方用户态**：`userland/*.elf`（抽查 busybox/desktop/shell）逐字节一致。
- **上游预构建 APK**（busybox/vim/openrc/less/ncurses/…）一致。

结论：内核与第一方用户态构建对 checkout 路径完全不敏感；`kernel.debug` 一致
说明调试信息已被现有路径映射/布局控制覆盖。

## 2. 差异及根因（必要的非确定性）

| 差异项 | 根因 | 证据 |
| --- | --- | --- |
| `rootfs/managed/bin/{login,su}`（shadow）、`{lsblk,mount,umount}`（util-linux）、`lib/libbsd.*` | **上游构建系统编译的二进制在调试信息中内嵌构建目录绝对路径** | `strings bin/login \| grep -c repro-a` = 3；`libbsd.a` = 2 |
| `leonos-musl-sdk.tar.gz` | `bin/leonos-musl-cc`（宿主编译驱动）以 `HOST_CFLAGS=-O2 -g` 构建，调试信息含工具源码绝对路径 | `diff -r` 两棵解包树仅该文件不同 |
| `rootfs/manifest.json` | `source` 字段按设计记录暂存来源的绝对构建路径（构建簿记） | diff 仅该字段不同 |
| 本地 `leonos-*.apk` 文件名/`packages.adb`/两个 manifest/ISO/VMDK/raw | 上游二进制内容差异经 `tree_digest`（内容+模式+链接目标）进入 APK 版本 content-id（epoch 相同、content-id 不同），并传播到镜像 | A `leonos-base-2.1790338572.10268…` vs B `…91514…` |

即：**差异全部源于上游工具链产物的绝对路径嵌入与簿记字段**，而非构建图不确定
性（无时间戳、无并发顺序、无随机量混入第一方制品）。

## 3. 建议修复（未实施，属上游适配器加固，按计划"解释非确定性"不扩权实施）

1. 上游适配器 CFLAGS 增加 `-ffile-prefix-map=<upstream work>=.`（覆盖 shadow/
   util-linux/libbsd 的构建目录），或安装前 `llvm-strip --strip-debug`。
2. `leonos-musl-cc` 链接前 strip 调试信息，或 HOST_CFLAGS 加 `-ffile-prefix-map`。
3. `rootfs/manifest.json` 的 `source` 字段改为相对构建根路径（簿记可移植化）。

完成上述三项后本地 APK/镜像即应跨路径逐字节一致。

## 4. 适配器世界复跑（2026-09-25 深夜，3b 项）

父构建改递归适配器（`b6fd988`+`ef0b398`）后同法复跑（A=主 worktree、B=repro
worktree@f95be1f，各自全新 O，均指向同一内核 checkout）：Ring-0 全部 9 制品
SAME（含 kernel.sys 新哈希 `805f4164…`），证明适配器与发布链零路径依赖；
DIFF 项与 §2 完全同形（ISO←上游二进制、musl-sdk←leonos-musl-cc、
manifest.json←source 字段），**无新增非确定性**。日志：
`/home/leon/build/ntclks-sep/logs/repro-compare-b2.log`。
