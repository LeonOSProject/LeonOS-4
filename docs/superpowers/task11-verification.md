# 任务 11 本地验证记录

日期：2026-09-28。范围：ReliefOS / ReliefNT 迁移计划任务 11；没有重新实施任务 1–10。

## 快照与边界

- 主仓任务 10 基线：`2484e0e6ec715b151493767881cdc0e8183f7e57`。
- 内核本地任务 11 提交：`3132fb6a15dd341c70ddca861f342cc2da08676a`。修改配置帮助、旧 splash 依赖文件兼容和 UAPI README；未改变冻结 ABI 数值。
- 最终产品隔离快照：`84b2179000aeb4c3447a2e382560f6a58364c24c`。之后仅规范生成头换行、修正验证脚本的 world 排序比较及补充验证文档；产品代码一致。
- 旧系统夹具：主仓 `d1a16c592a81cf78e856ab8a9cfe87197bf526f5`，内核 `5bd8425d10d6879cc578342c0173a92af9468b62`，真实构建的旧系统磁盘。
- 所有克隆、O、临时文件、日志和镜像均位于 `/home/xiaobai/Projects/Projects` 数据盘。命令设置 `TMPDIR=/home/xiaobai/Projects/Projects/.reliefos-task11-verify-tmp`。内核提交通过 `127.0.0.1:12334` 推送到 `.gitmodules` 已配置的原远端及既有 `feature/rename` 分支；未推主仓、未创建 PR、未替换远端地址或删除用户文件。
- 用户两份未跟踪交接以及子仓 `max_size = 50G/` 保留且不进入提交。主仓仅记录内核 gitlink。

## 实现与红绿验证

真实回归发现并修复：旧账户 seed 写旧目录；安装器错误组合 canonical 目录与旧 SONAME；安装器拒绝完整旧 namespace；APK 在配置迁移前引入 canonical 默认值；APK 组件归属与媒体排除仍使用旧路径；旧 splash depfile 阻止增量构建。断言均保留，分别获得失败和通过证据。

测试设施修复包括：canonical 构建输出观察路径、旧环境变量回退隔离、从真正图形安装器日志取证、等待图形登录就绪后切换 TTY、读取真实用户会话输出。依赖签名测试改为实际构建签名目标；`make -n` 本来就不能写候选文件，读取旧 candidate 不是有效测试。

APK world 按 canonical 包名迁移后由 APK 重新排序。验证比较排序后的完整依赖行列表，保留版本约束与重复项；不允许额外依赖增删。自定义配置、用户文件和账户仍逐字节比较，权限与属主另行验证。

日志目录：`/home/xiaobai/Projects/Projects/.reliefos-task11-logs`。

| 验证层 | 实际结果和证据 |
| --- | --- |
| 干净递归克隆与精确远端 gitlink | `/home/xiaobai/Projects/Projects/.reliefos-task11-remote-check` 克隆主仓任务 11 提交 `83bb17ce03d00d1f1bae61a41237ae3b96b6e4f8`；递归子模块包含 `kernel/reliefnt` 和嵌套 kconfig，内核 gitlink/HEAD 均为 `3132fb6a15dd341c70ddca861f342cc2da08676a`。代理 `127.0.0.1:12334` 下 `ls-remote`、精确 checkout 和完整主仓 `make fetch` 均通过；日志 `remote-clean-make-fetch.log`，退出码 0。递归 checkout 的第三方子模块复用本地已有对象；SQLite 未从上游克隆或下载。 |
| 干净克隆 UAPI 与子仓守卫 | `python3 tools/test_header_export.py` 通过：82 个头、白名单、自包含 C/C++、私有头边界及残留检查。`test-submodule-contract.sh` 11 项、0 失败，覆盖未初始化提示、dirty 开发构建允许、dirty release 拒绝、gitlink 不匹配拒绝和一致旧 SHA 回滚守卫。运行前仅向隔离克隆的数据盘缓存复制锁定 Unifont 文件，SHA-256 与 lock 一致。 |
| 主仓 `make fetch` | 原任务本地精确依赖缓存验证见 `fetch-current.log`；另在干净递归克隆完整 fetch 退出 0，见 `remote-clean-make-fetch.log`。 |
| 子仓 `make test` / headers / all | 通过；ABI 74 records、32 constants，82 个白名单头及 C/C++ 自包含、旧新 UAPI 兼容。`kernel-test-current.log`、`kernel-all-current.log`。 |
| 主仓 `make test test-long` | 最终总目标退出 0；`make-test-seventh-long.log`。执行链 14 项、并行/中断恢复 19 项及缺失产物恢复通过。此前失败日志保留，不视作通过。 |
| 主仓 `make -j8 all release` | 最终运行退出 0；`make-final-all-release.log`。 |
| 改动语法 / 构建入口 | 124 个 Python、43 个 shell、3 个 VSCode JSON 语法通过；最终 help/doctor 退出 0（`final-help-doctor.log`）。 |
| 定向 ASan/UBSan 安装器 | 9 项通过，包括旧/新 namespace、缺失双库、非真实目录与 usr-merge 拒绝；`installer-final-private-green.log`。 |
| APK 升级集成 | 真实包替换、签名拒绝、外部包/world 和旧自定义 locale 保留通过；`apk-legacy-locale-red.log` → `apk-legacy-locale-green.log`。 |
| 内核更新主机事务 | 全部通过，包含失败恢复和旧布局；`kernel-update-host-final.log`。它本身不是 QEMU 证据。 |
| ABI / SDK | 两种 SONAME、旧库 416 个 `leonos_*` 导出保留、新旧 ELF DT_NEEDED/PT_INTERP/RUNPATH、SDK 双头/双库通过；`final-artifacts.log`。 |
| APK 发行物 | 三套仓库各 31 个包及索引验签通过；四种 staging 的真实数据库/world、双库通过；`final-artifacts.log`。 |
| 实际 ext2 内容 | live、installer runtime、installed payload 的数据库/world、签名索引、APK、desktop、双库与文档逐字节吻合最终 staging；`final-ext2-payloads.log`。 |
| ISO / ESP / VMDK | xorriso、mtools 检查双启动路径及 GRUB legacy entry；qemu-img 比较 RAW/VMDK 来宾可见字节完全一致；`final-artifacts.log`。 |
| Pages | 109 个 HTML 页面、下载校验和及 RPR 机器接口验证通过；`final-pages.log`。只证明本地生成树。 |
| 旧名审计 | NUL 安全主仓/内核文件名及内容扫描，另扫新源码；主仓 456 / 内核 107 个命中路径，未分类 0、陈旧条目 0、migration 0。563 条明确理由：468 compatibility、77 history、15 external-url、3 attribution。`final-audit.log`、`final-audit-hits.json`、`strict-main-green.log`。 |

## QEMU 实际运行

均使用 QEMU/KVM、q35、UEFI、数据盘独占测试镜像。新安装与旧系统升级是不同磁盘，均通过实际 GUI，不是 desktop-only smoke。

- 完整新装、root/普通用户真实登录、UID/GID 与文件权限拒绝、HOME、桌面和 Terminal/Fastfetch：`t11-install-final/` 已通过；最终发行 ISO 的重复运行 `t11-install-release/` 也完整通过（`qemu-install-release.log`）。人工检查最终 Terminal/Fastfetch 截图显示 ReliefOS / ReliefNT；根目录 logo.png 为无旧文字图形。
- 最终 ISO 升级旧系统：`t11-old-upgrade-final/` 通过 GUI、APK wait_status=0 和完成日志。自定义 locale 字节/mode/owner、用户文件、passwd/shadow/group、外部包版本、world 依赖和旧 ESP 内核/loader 字节均验证通过；`final-upgrade-preservation.log`。
- 最终升级磁盘启动，旧头/旧库链接的 ELF 与新 ELF 实际调用时间 API 均返回 0，来宾读取到保留的自定义 locale：`qemu-final-upgrade-boot.log`。
- 来宾中真实运行内核更新脚本：在提交 sync 阶段注入失败，验证 canonical 双载荷恢复原哈希，旧载荷/GRUB 未改；随后正常更新成功，锁清理通过：`qemu-runtime-2.log`、`t11-upgraded-runtime-2/guest/serial.log`。
- 更新后重启通过；另一个磁盘故意损坏 canonical 内核并选中既有 legacy GRUB 项，旧内核仍启动升级后的根系统，旧/新 ELF 再次返回 0：`qemu-boot-pair.log`。
- 内核更新的下载传输使用来宾测试 `rprfetch` 文件夹具；上述证明来宾文件系统事务与启动回滚，不证明公共 RPR HTTPS 服务或真实网络下载。

中间失败原样保留：输入时序错误、旧 namespace 拒绝、错误日志位置、TTY 切换过早、BusyBox 无 `sha256sum -c`。最后一项改为重新计算全部哈希后用 `cmp` 比较，不降低校验要求。

## 最终制品 SHA-256

构建输出：`/home/xiaobai/Projects/Projects/.reliefos-task11-current-out`。

| 制品 | SHA-256 |
| --- | --- |
| reliefos-installer.iso | `f9d2a6ad5153b0701fe063a548b6083eccef7c92e4f06f97ce21d5b4fa6c88d9` |
| reliefos-live.iso | `1bda186cfb2b283a5c666f6102e7eee78c33f81c3cccbe18d92ed9033771e92e` |
| reliefos.raw | `95d666857d90cc32428a3852daf7bd9b93a8ce82a727af29314cd1b5f63c3298` |
| reliefos.vmdk | `33ac7984f4d8a38ac23fb2aef385df70540939e6eda0ee74fff04c47c549d2c9` |
| reliefos-musl-sdk.tar.gz | `c1f9bde52bbe7ebe1059bbda566c4765505a1a0d8b99121fc0e57a9aef756cd5` |

## 外部门禁与未运行项

- `.gitmodules` 原 URL 保持不变。先前原 gitlink `76834e55f2b9eb035856ce86825f70401ffb01d5` 曾返回 `upload-pack: not our ref`，本地任务 11 内核 SHA 首次 fetch 遇到 TLS EOF；用户修复代理后，提交 `3132fb6a15dd341c70ddca861f342cc2da08676a` 已推至原远端 `feature/rename`，代理下精确 `ls-remote`、递归克隆、主仓及子仓 `make fetch` 全部通过。早期失败日志 `clone-current.log`、`remote-final-kernel.log` 保留为历史诊断，不代表最终状态。
- 新 GitHub 主/子仓、Pages/RPR、SourceHut 地址未提供；外部发布、同步、真实 HTTPS/RPR 下载、CI Secret 切换均未运行。没有推测或替换地址。完整发行迁移尚不能认证完成。
- VMware 专属运行与 SVGA II 3D 验证未运行；QEMU 不能替代。
- 完整安装器 TTY 安装分支、所有独立 QEMU helper 的逐个运行未运行；本次执行的是以上明确列出的 GUI/TTY 登录/升级/回滚路径。
- 额外审查中运行的旧辅助测试未全绿：netmand 主机 adapter 通过，但后续 Dyne 编译器缺失；API/time 辅助入口同样缺旧编译器；sudo 辅助套件引用已删除 authd 头；legacy installer-account fixture 旧新结构不匹配。日志见 `host-c-agent-report.md`。没有删断言或将这些结果算作通过；它们不属于 `make test`/`test-long` 总目标。
- IPC ASan/UBSan 与 wind shared-memory 辅助测试通过。旧 authd/OOBE 源 fixture 是退役架构资料，未宣称当前系统测试通过。`tools/test_security_regressions.py` 全脚本未运行（其旧构建路径及 tmpfs 隔离方式不符合本轮数据盘约束）。

任务 1–10 的既有提交保留。任务 11 步骤 1–6 均完成；步骤 1 的精确远端与干净 clone 门禁已通过。主仓任务提交只记录内核 gitlink，不含子仓源码；内核子仓提交已在原远端可取。Pages/RPR、SourceHut、GitHub 新 Secret 和 VMware 等未运行项见上文，不能据此声称这些外部服务已切换。
