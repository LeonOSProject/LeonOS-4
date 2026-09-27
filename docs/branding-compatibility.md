# ReliefOS / ReliefNT 品牌迁移词表与兼容矩阵

本文件是品牌迁移的命名契约单一规范来源：新旧名称的对应关系、例外清单、
允许残留旧名的分类规则都以本文为准。对应规格与计划见
`docs/superpowers/specs/2026-09-27-reliefos-reliefnt-rename-design.md` 与
`docs/superpowers/plans/2026-09-27-reliefos-reliefnt-rename.md`。

机器可读的旧名允许清单在 `tests/build/brand-allowlist.tsv`，由
`tests/build/test-brand-identity.sh` 在每次 `make test` 中核对。

## 1. 展示规则

| 语义 | 新的规范值 | 旧值的地位 |
| --- | --- | --- |
| 系统显示名 | `ReliefOS`（不拼版本数字） | `LeonOS 4` / `LeonOS` 只留历史、迁移提示和兼容测试 |
| 系统机器 ID | `reliefos` | `leonos` 只用于识别旧安装/配置 |
| 内核显示名 | `ReliefNT` | `NTCLKS` / `ntclks` 只留历史、旧路径/API 兼容 |
| 内部前缀 | `RELIEFOS_`、`reliefos_`；内核私有为 `RELIEFNT_`、`reliefnt_` | 旧公开 ABI 符号保留兼容，旧私有前缀逐步退出 |
| 版本字段 | `configs/build-version` 与 `VERSION_ID=4` 保留现有数值语义 | 不把 `4` 拼进正式显示名 |

词汇映射的展示规则：

- `LeonOS 4`（带版本的展示名）→ `ReliefOS`；`LeonOS`（裸名）→ `ReliefOS`。
- `NTCLKS`（内核展示名）→ `ReliefNT`；`ntclks`（技术标识、路径、私有符号）→
  `reliefnt`；`NTCLKS_*` 私有宏 → `RELIEFNT_*`；`ntclks_*` 私有函数 →
  `reliefnt_*`。
- `leonos` 机器 ID、路径段、包名前缀 → `reliefos`；`LeonOS` 产品名在第三方
  归属、许可证原文与历史材料中保持原样。
- 复合名按段替换：`LeonOS 4 desktop` → `ReliefOS desktop`；`NTCLKS kernel` →
  `ReliefNT kernel`；`libleonos.so.2` 的新库名为 `libreliefos.so.2`。

## 2. 新旧路径、库、包与配置符号矩阵

| 类别 | 旧规范值 | 新规范值 |
| --- | --- | --- |
| 内核子仓路径 | `kernel/ntclks/` | `kernel/reliefnt/` |
| 内核私有源/头 | `kernel/ntclks/kernel/ntclks/`、`include/ntclks/` | `kernel/ntclks/kernel/reliefnt/`、`include/reliefnt/` |
| 公开 UAPI 头 | `include/uapi/leonos/*.h` | `include/uapi/reliefos/*.h`（旧路径转发头兼容） |
| 主仓公共头 | `include/leonos/` | `include/reliefos/`（旧目录转发头兼容） |
| 运行库头 | `userland/runtime/include/leonos/` | `userland/runtime/include/reliefos/` |
| 共享库 | `libleonos.so.2` | `libreliefos.so.2`（同一对象集链接两个 SONAME） |
| SDK 归档/驱动 | `leonos-musl-sdk.tar.gz`、`leonos-musl-cc` | `reliefos-musl-sdk.tar.gz`、`reliefos-musl-cc` |
| 导出符号 | `leonos_*` | `reliefos_*`（旧符号真实 ELF alias 兼容） |
| 公共宏/结构 tag | `LEONOS_*`、`struct leonos_*` | `RELIEFOS_*`、`struct reliefos_*`（旧名宏别名到同一布局） |
| 来宾目录 | `/etc/leonos`、`/usr/lib/leonos`、`/usr/share/leonos`、`/var/lib/leonos`、`/run/leonos` | `/etc/reliefos`、`/usr/lib/reliefos`、`/usr/share/reliefos`、`/var/lib/reliefos`、`/run/reliefos`（旧路径迁移后兼容读取） |
| 启动载荷 | ESP `/leonos/kernel.sys`、`/boot/leonos` | ESP `/reliefos/kernel.sys`、`/boot/reliefos`（旧载荷保留回滚） |
| ISO 标记 | `leonos-installer-iso.marker` | `reliefos-installer-iso.marker` |
| 产物名 | `leonos4.vmdk`、`leonos4-live.iso`、`leonos4-installer.iso` | `reliefos.vmdk`、`reliefos-live.iso`、`reliefos-installer.iso` |
| APK 包组 | `leonos-*` | `reliefos-*`（显式 replaces/升级路径） |
| Kconfig 组件符号 | `CONFIG_LEON_COMPONENT_*` | `CONFIG_RELIEFOS_COMPONENT_*`（显式迁移命令导入旧配置） |
| 构建/环境变量 | `LEONOS_*` | `RELIEFOS_*`（旧名为输入别名，新名优先） |
| OpenRC 服务 | `leonos-*` 服务与 runlevel 链接 | `reliefos-*`（旧名脚本转发入口，服务只启动一次） |
| 布局契约 | `include/leonos/layout.h` | `include/reliefos/layout.h`（旧头转发） |
| 标记/所有权 | 旧 `LEONOS_BUILD_OWNER` 等 marker | 新/旧 marker 均识别，拒绝未知目录 |

`kernel.sys`、`kernel.debug`、`kerneldebug.sys`、`loader.elf`、驱动 `.drv`、
`EFI/BOOT/BOOTX64.EFI`、syscall 编号、UAPI 结构布局与 `LEONACL.SYS`
是引导器/磁盘协议与 ABI 契约，不随品牌改名。

## 3. 例外清单（旧名允许残留的分类）

`tests/build/brand-allowlist.tsv` 的 `category` 列只允许以下取值：

- `compatibility`：兼容层本身——旧头转发、旧符号 alias、旧库、旧配置导入、
  迁移器、兼容测试与审计脚本自身（含允许清单与词表文件）。
- `history`：历史材料——`BY_AUTHOR.md` 产品沿革、旧安全审计、旧设计规格、
  本迁移规格/计划/清单、changelog、迁移提示文案中引用的旧名。
- `external-url`：真实旧外部地址——GitHub/Pages/SourceHut 旧 URL、CI 签名
  Secret 旧名等，在新目标实测可用前保留的地址。
- `attribution`：第三方归属与许可证原文——上游版权行、Apache-2.0 根
  `LICENSE` 原文、`NOTICE` 中第三方段落、移植软件的上游名称。
- `migration`（过渡期临时值）：基线清单中尚未完成迁移的文件。最终复审
  （任务 11）后不得残留该分类；每条残留必须改判为上述四类之一或清零命中。

固定例外（不得因迁移破坏）：

- `LeonMMcoset` 等人名/账号保持原样。
- `LEONACL.SYS` 是权限元数据格式兼容标识，保留原名并在文档解释。
- 已发布下载物校验值、旧安全审计结论保持历史真实性。
- 根 `LICENSE`（Apache-2.0 原文）不做品牌替换。

## 4. 单一规范来源

- 新契约的唯一规范来源：本文件第 2 节矩阵所指向的新文件/新符号
  （`include/reliefos/`、`include/uapi/reliefos/`、`kernel/reliefnt/`、
  `reliefos-*` 产物名等）。任何新实现只能定义一份布局/ABI，旧名层只能转发。
- 旧契约的兼容行为唯一规范来源：本文件第 3 节分类 + `brand-allowlist.tsv`
  逐文件条目 + 各兼容层代码中的转发/alias 注释。
- 允许清单的新增条目必须带理由列，并在最终复审中被逐条归类；不允许用一条
  全局 `grep -q` 或整目录排除替代逐文件核对。

## 5. 验证状态

迁移验证分四层记录：源码、编译/测试、打包、虚拟机运行。各任务的实际完成
层级记录在实现计划复选框与最终交付报告中；本文件只维护契约，不宣称验证
已经完成。
