# Make 全量迁移接管记录（2026-09-19）

本轮在 `xiaobai/dev/buildsystem` 继续工作，未提交或推送。原分阶段审查包保留为历史证据；当前生产入口以 `make help` 为准。

最新状态：源码对照发现的九项旧规则遗漏已补入，完成相关定向检查，尚未重建和验证最终镜像。详见 [规则对照审计](rule-parity-audit-2026-09-19.md)。按用户要求不追加全量测试；下文历史测试记录仅证明当时覆盖的范围。

## 已实现范围

构建日志：APK 和 xorriso 的输出采用固定宽度 `APK` / `XORRISO` 标签，警告和错误单独标为 `WARN` / `ERROR`；回车进度转换成普通日志行，去除终端颜色控制序列。APK 原始日志位于相应 `packages/apk*/logs/`（`install.log`、`index.log` 和各包日志），xorriso 原始日志位于 ISO 旁的 `*.xorriso.log`。现有 `logs/apk-stage.log`、`logs/installer-stage.log` 保存格式化后的阶段输出。失败仍返回原命令退出码，并输出原始日志路径。

GNU Make 管理内核、loader、Rust middlelayer、运行库、应用、认证/存储/终端上游、资源、SDK、签名 APK、rootfs、VMDK、Live ISO、Installer ISO 和 RPR 站点。项目自有生成器使用 C；上游采用其 Make/Autoconf，Linux-PAM 使用固定功能的 Make 适配层。生产构建不依赖 Python/Meson/Ninja。

已删除的受跟踪旧构建源文件见 `retired-build-files.txt`。旧缓存、旧镜像、独立 OS Python 回归测试及其必要参考实现保留；保留文件不是生产回退入口。CI 已改为 Make，但未执行远端 CI。

## 本轮修正

- 被动目标和 dry-run 不再重建生成的 include；递归 recipe 在 `-n` 下不清理上游工作目录。
- SDK 检测清单中缺失的文件与链接；APK、ESP 和 RPR 额外检查目录模式及链接目标，rootfs 检查清单中的类型与模式。
- SDK 修复空 zlib/png 归档，按 SDK 组件选择导出头文件/库/示例/许可证，移除旧 Python 编译入口。
- APK 使用裸包名维护 world；`1.<epoch>.<content>-r0` 排在旧 `0.<time_ns>-r0` 之后。发布版本仍必须提升 epoch/版本前缀，不保证同 epoch 的不同内容哈希单调递增。
- ISO 固定 Rock Ridge 文件时间；ext2 在用户命名空间中仍保存来宾 UID/GID。VMDK 的 QEMU CID 是已记录的非确定字段。
- libxcrypt 配置不再运行可选 passlib/Python 测试依赖探测。
- 第三方验收使用私有输出树和锁文件副本，避免继承 `O=` 后误查旧目录。

## 已取得的集成证据

- SDK：带空格的迁移路径下编译动态、静态、StardustUI C++ 示例；最小动态/静态 libc 程序均实际运行。见 `sdk-final-validation.md`。
- APK：`tests/integration/test-apk-upgrade.sh` 在私有 Linux 用户/挂载命名空间中运行实际 guest updater。旧 `0.1789751409733933185-r0` 包成功升级，BusyBox post-upgrade/trigger 执行成功，外部签名测试包、world 和本地配置保留。日志 `/tmp/leonos-apk-upgrade4.log`，退出码 0。此项不等同于 LeonOS 来宾内升级，也不覆盖全部 Alpine build-base 包组合。
- 格式复现：固定 epoch 下两份 raw/ext2 与 ISO 字节一致；VMDK 仅 CID 的 8 个字节不同，`qemu-img compare` 确认磁盘内容一致。见 `image-reproducibility-2026-09-19.md`。这不是两个全新 O 的完整系统逐字节对照。
- 独立 OS 回归入口 `make test-legacy` 退出码 0，日志 `/tmp/leonos-legacy4.log`。

## 未闭合的来宾验收

本机 GRUB 2.14/OVMF 启动 Live ISO 在进入 LeonOS loader 前出现固件页错误，未到 kernel boot complete/PID 1。原交接已有新旧内核同表现记录，但不能由此认定问题已解决，也不能把超时计作启动成功。三类镜像启动、安装和来宾内升级验收必须保留为未通过；本轮没有盲改 OS 调度/内存实现来掩盖此阻塞。

全目标构建、execve 跟踪和完整测试的最终结果在本文件后续记录。

## 全量构建首轮验收

`PATH=/usr/bin:/bin:/home/xiaobai/.cargo/bin unshare -Urn strace -f -qq -e trace=execve -o /tmp/leonos-full-production-fixed.exec make O=out/full-migration RUSTC=/home/xiaobai/.cargo/bin/rustc -j8 all rpr-pages` 返回 0（`/tmp/leonos-full-all5.rc`）。网络命名空间阻断网络，使用事先校验的缓存。跟踪中实际执行的 Python/Meson/Ninja 程序为 0。

宿主工具 `make O=out/host-clang-final HOSTCC=clang tools` 返回 0；修复了 Clang 严格警告发现的 variadic 格式函数标注缺失。此修正之后还需刷新全目标产物并检查无改动构建。

三类当前介质再次运行 `SMOKE_TIMEOUT=30 sh scripts/test-smoke.sh ...`，退出码 1，disk/live/installer 均未达到启动标记。日志为 `out/full-migration/logs/smoke-{disk,live,installer}.log`，汇总 `/tmp/leonos-smoke-current.log`。此失败单独保留，不折算为主机测试通过或跳过。

CI 两个 workflow 的 YAML 语法解析通过；RPR 产品存在性及私钥边界扫描通过。SDK 配方补充 `set -eu`，可选 ncurses share 只在目录存在时复制，复制失败不再被忽略。

## 完整主机测试

`PATH=/usr/bin:/bin:/home/xiaobai/.cargo/bin TMPDIR="$PWD/out/test-tmp" make O=out/acceptance-final RUSTC=/home/xiaobai/.cargo/bin/rustc test` 返回 0（`/tmp/leonos-test-full4.rc`）。日志 `/tmp/leonos-test-full4.log` 包含 common 74、JSON 66（普通及 ASan/UBSan 各一次），以及全部 build 契约脚本。认证 23、bootstrap 22、并发 17、依赖锁 44、增量 21、第三方 29 等检查无失败。

先前 full3 的第三方签名失败是测试固定默认 O 却继承外层 O 所致；已改为私有目录/锁副本，定向复测和上述完整复跑均通过。早期与增量测试并行运行的 no-op 检查作废：测试会 touch 共享源码，必须等测试结束后串行复跑。

独立新输出目录的 `test-execchain` 全生产跟踪通过：14 项检查、0 失败；生产段捕获 186789 次 execve，未执行 Python/Meson/Ninja。包含真实 SDK、签名 APK、三类镜像与 RPR，已取消原来的未迁移 stub 预期。日志 `/tmp/leonos-test-long-final.log`。

`test-jobs` 19 项、0 失败：`-j1`/`-j8` 三轮选择同样的失效对象集合；kernel.sys/debug/unstripped 与生成配置内容一致；中断后不保留伪完成镜像，重跑恢复原图像。此比较覆盖内核，并非三套全系统并行复现。

完整 `make O=out/acceptance RUSTC=/home/xiaobai/.cargo/bin/rustc test-long` 返回 0（`/tmp/leonos-test-long-final.rc`）。最后的认证/musl 缺失成员恢复、更新 producer 后稳定 no-op 检查也通过。

## no-op 验收发现并修复的缺陷

全目标第二次构建触发 rootfs/APK 重打包。真实 `--debug=b` 证据显示 `userland-prune` 每次删除禁用的 `cmd.elf`，而 rootfs 的上游工具依赖又恢复它。修复为 C 组件清单单独导出 `LEONOS_DISABLED_APPS`，清理只消费禁用 `*-app` 组件，不删除 `tool`。回归夹具同时声明禁用应用与禁用工具，断言前者被清理、后者保留；`test-userland-graph.sh` 和组件解析测试通过。修复后需重新进行全目标/no-op 验收。

## 最终串行结果

修复 prune 后 `unshare -Urn make O=out/full-migration RUSTC=/home/xiaobai/.cargo/bin/rustc -j8 all rpr-pages` 返回 0（`/tmp/leonos-post-prune.rc`）。紧接着重复完全相同目标并跟踪 execve，返回 0（`/tmp/leonos-noop.rc`）。对 images/packages/sdk/userland/system/rpr-pages 的路径、类型、大小、mtime、链接目标排序快照逐字节相同（`/tmp/leonos-noop-{before,after}.txt`）；未重编译或重打包。日志中的 GEN 是内容相同则不改文件的检查，不是交付产物重写。

最终变更后的定向组件解析、组件元数据、应用图回归和 Clang tools 均返回 0；完整 test/test-long 的通过记录在上述最后 prune 修复之前，因此以这些定向回归补充覆盖最后改动，未伪称再次全量复跑。

结论：生产 Make 全量迁移、离线构建、旧引擎退休以及主机测试完成；A15 的来宾启动/安装/升级仍未通过，原因和日志如上。未提交、未推送。

## 用户实机反馈后的安装器修复

用户提供日志证明来宾已启动到 OpenRC；此前本机 OVMF 阻塞不代表用户环境无法启动。安装器服务报 `installer.elf does not exist`：组件 `installer` 有意不进入普通 rootfs，但 installer-stage 遗漏了安装器专用复制步骤。现已显式声明 installer ELF 构建依赖，在 APK 分包前加入运行根目录并创建命令链接；签名安装后检查文件可执行性。安装目标系统根目录仍不加入安装器。

用户随后报告 GEN 后静默“卡住”：installer-stage 全部输出被重定向，APK 逐项复制约 12000 个条目时无提示。新增 run-logged.sh 通过 FIFO/tee 实时展示并保存日志，保留原命令退出状态（输出及 exit 7 已验证）；APK 显示分包阶段及每 2000 条复制进度。普通 APK 与 installer 两条入口均采用实时日志。按用户偏好，不为此继续完整 TDD 流程，只进行针对性验证及实际 ISO 重建。

安装器修复后实际 `make ... installer` 返回 0（`/tmp/leonos-installer-progress.rc`）。从最终 ISO 的 `/install/root.fat` 提取 ext2，再用 debugfs 导出 `/usr/lib/leonos/apps/installer/installer.elf`：与编译 ELF cmp 相同，文件模式 0755，验证返回 0（`/tmp/leonos-installer-iso-check.rc`）。这证明最终介质已包含程序，不等同于已在用户 VMware 中验证 GUI 启动。

## 已安装 Alpine binutils 的升级回归

用户的完整日志显示 BusyBox 包争用 `/usr/bin/ar`、`/usr/bin/strings`；本机 `build/issue28-alpine-prior/root` 实际复现相同错误，升级返回 3：两个旧外部包 broken 状态，加一个新 BusyBox 安装失败。旧 Python 打包器曾移除这两个 fallback 链接，新 APK staging 遗漏了该规则。

修复：分包前移除这两个符号链接，继续使用现有 post-install/post-upgrade/trigger 仅在命令缺失时创建 BusyBox fallback；不会覆盖 binutils 的真实程序。升级规则未放宽为忽略 exit 3。修复后的真实旧 Alpine 系统副本升级返回 0，保留外部包/world/配置以及 binutils 文件内容；日志 `/tmp/leonos-binutils-upgrade.log`。

由于同 epoch 的内容哈希版本不保证递增，修复将默认包代际提升为 `2.<epoch>.<content>-r0`，确保已经部分升级到错误 `1.*` 的用户也能获得修复。补充真实旧错误仓库→失败状态→新仓库恢复的集成验证。
