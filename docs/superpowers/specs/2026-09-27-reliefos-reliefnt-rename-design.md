# ReliefOS / ReliefNT 全项目品牌迁移设计规格

日期：2026-09-27
状态：依据用户确认的命名与迁移范围编写，尚未实施
对应计划：`docs/superpowers/plans/2026-09-27-reliefos-reliefnt-rename.md`

## 目标与已确认决策

把系统正式名称改为 **ReliefOS**，把独立内核正式名称改为 **ReliefNT**。迁移覆盖用户界面、文档、源码技术标识、构建与发布、安装路径、SDK 和内核子仓；已发布程序与现有安装需要过渡兼容。用户明确选择“全面迁移并保留兼容”和无数字后缀的显示名“ReliefOS”。现有版本数字仍由 `configs/build-version` 和 `VERSION_ID=4` 表达，不再拼进正式显示名。

| 语义 | 新的规范值 | 旧值的地位 |
| --- | --- | --- |
| 系统显示名 | `ReliefOS` | `LeonOS 4` / `LeonOS` 只留历史、迁移提示和兼容测试 |
| 系统机器 ID | `reliefos` | `leonos` 只用于识别旧安装/配置 |
| 内核显示名 | `ReliefNT` | `NTCLKS` / `ntclks` 只留历史、旧路径/API 兼容 |
| 内部前缀 | `RELIEFOS_`、`reliefos_`；内核私有为 `RELIEFNT_`、`reliefnt_` | 旧公开 ABI 符号保留兼容，旧私有前缀逐步退出 |
| 来宾路径 | `/etc/reliefos`、`/usr/lib/reliefos`、`/usr/share/reliefos`、`/var/lib/reliefos`、`/run/reliefos` | 旧配置/数据先迁移再兼容读取，不覆盖用户文件 |
| 开发/发行名 | `reliefos-*`、`libreliefos`、`reliefnt-*` | 旧 SDK、APK、ELF、配置入口按兼容矩阵保留 |

## 扫描依据与边界

主仓当前分支是 `feature/rename`，HEAD 为 `d1a16c5`；主仓受控文件 1,654 个。内核是独立 gitlink，固定在 `5bd8425d10d6879cc578342c0173a92af9468b62`，子仓受控文件 314 个。主仓 `git grep -I -i` 的扫描结果：`leonos` 命中 783 个文件、17,106 行；`ntclks` 命中 217 个文件、1,474 行；并集 831 个文件。子仓对两类名称的合并搜索命中 283 个文件、5,805 行。主仓约 146 个受控路径名含旧名称。数字是行数或文件数，不是可直接替换的品牌文案数。

扫描覆盖 Git 受控文本与文件名、根 `logo.png` 和 `system/resources/wallpaper-metro.bmp` 的视觉检查、其他受控位图的类型与用途清单。根 logo 是无文字的蓝色图形，壁纸是无品牌字样的地球照片；本轮沿用两项资产并更新说明/引用，不修改像素或来源署名。主仓和子仓均存在用户未跟踪的 `max_size = 50G/` 大目录；它们以及 `out/`、缓存、第三方子模块内部源码、旧发布二进制均不作为源码改名输入。本次仅规划，未运行构建、镜像或虚拟机。执行前应重新生成清单，逐项核实技术标识的生产端与消费端。

逐文件清单见 `docs/superpowers/specs/2026-09-27-reliefos-reliefnt-rename-inventory.tsv`：列出主仓和内核子仓所有旧名文本或路径命中的受控文件，共 1,142 条文件记录，包含每个文件的命中行数及二进制/不可读标记。该表使用 UTF-8 容错读取和 NUL 字节识别，统计口径与 `git grep -I` 不同，因此不能直接将表内汇总和上段 `git grep` 行数相加或比较。图像像素、用户未跟踪内容与第三方子模块内容不在此表内。

## 方案比较

1. **仅改可见文案**：成本低，但 `/etc/leonos`、SDK、包名、构建产物和内核工程名继续暴露旧品牌，不满足已确认的范围。
2. **一次性全局替换**：文本看似统一，但会破坏 UAPI 头文件路径、既有 ELF 的 `DT_NEEDED`、旧包升级、安装器与启动载荷配对、子仓 gitlink；拒绝采用。
3. **分阶段双名迁移（采用）**：先给新名称建立规范入口，再切生产者与消费者，同时把旧名字限制在明确的兼容层、历史材料和已发布地址中。每层有可验证的迁移完成条件。

## 架构边界与顺序

```text
ReliefNT 子仓：内核私有名 / UAPI / loader / 内核标识
           │ headers_install + kernel.sys/loader.elf 产品契约
           ▼
ReliefOS 主仓：公开头 / 运行库 / Kconfig / 构建适配器
           │ stage + APK / SDK / ISO / VMDK
           ▼
来宾：启动 → 内核 → init/OpenRC → 登录/桌面 → 更新/安装
           │
           └─ 旧安装及旧 ELF 的兼容读取、旧包升级、旧配置保留
```

内核子仓与主仓独立管理：内核改动在子仓形成可引用的提交之后，主仓才更新 gitlink 和 `.gitmodules`。不能把子仓源码当作主仓普通文件提交。迁移 `kernel/ntclks` gitlink 到 `kernel/reliefnt` 前要先处理用户未跟踪内容，并验证新远端确实可用；远端未就绪时，名称可先在源码/显示层落地，现有 URL 必须继续保持可 fetch。

### 1. 内核与 UAPI

- 子仓内核私有目录 `kernel/ntclks/`、`include/ntclks/` 和 `NTCLKS_*` 优先迁为 `kernel/reliefnt/`、`include/reliefnt/`、`RELIEFNT_*`；构建图和所有 `#include`、测试、日志前缀同步更新。
- 独立构建和 CI 显示 ReliefNT。对外产品仍由 `kernel.sys`、`kernel.debug`、`loader.elf`、五个 `.drv` 和 `kerneldebug.sys` 组成；这些载荷文件名是主仓/引导器已有协议，本轮不改。
- UAPI 新规范路径为 `include/uapi/reliefos/`。旧 `<leonos/...>` UAPI 继续导出只含转发 include/typedef/宏别名的兼容头。`configs/header-export.list` 应同时列新旧路径，头文件导出测试要验证自包含、结构大小与 ioctl/syscall 编号不变。
- 已发布 syscall 编号、结构布局、磁盘元数据（尤其 `LEONACL.SYS`）、权限语义不因文字更名而改变。`LEONACL.SYS` 是格式兼容标识，保留原名并在文档解释。

### 2. 主仓公开 API 与运行库

- 新代码以 `<reliefos/...>`、`reliefos_*`、`RELIEFOS_*` 为规范。主仓 `include/leonos/`、`userland/runtime/include/leonos/` 作为仅供旧源码编译的兼容头保留，且不得与新定义形成两份可分叉的 ABI。
- 旧已编译应用的 `libleonos.so.2` 及其旧导出符号保留；新 SDK 提供 `libreliefos` 和新头文件。源实现改用新函数名时，在同一翻译单元为旧函数名提供真实 ELF alias 或等价包装；共享对象从同一受控对象集分别链接出新旧 SONAME。旧名与新名对应的函数、结构和枚举必须有逐项映射与 ABI 校验，不能只改头文件而让动态链接失败。
- 运行库和 SDK 装配从 `mk/runtime.mk`、`mk/sdk.mk`、`tools/build/musl-sdk.sh` 协同切换；真实旧 ELF 与新 ELF 都用随包 SDK 编译/链接/运行验证。
- 内部 `LEONOS_*` Make/环境变量与 `CONFIG_LEON_COMPONENT_*` 迁移时提供明确的旧配置导入，仅以新名生成配置。`Kconfig.components` 从 `configs/components.toml` 的生成器同步产生，不直接手改。

### 3. 来宾文件系统、服务与安装升级

- 规范根目录为 `/etc/reliefos`、`/var/lib/reliefos`、`/var/cache/reliefos`、`/run/reliefos`、`/usr/lib/reliefos`、`/usr/share/reliefos`；规范路径集中在新 `include/reliefos/layout.h`，构建期布局与来宾消费者统一使用它。
- 旧安装升级先盘点旧配置与用户数据，再按“新路径不存在才复制/迁移”的规则发布新路径；旧路径在过渡期保持可读。任何已有新路径内容优先于旧内容，冲突记录但不覆盖。不能以软链覆盖真实旧目录，也不能删除旧数据库。
- OpenRC `leonos-*` 服务与 runlevel 链接改为 `reliefos-*` 时，要求同一服务只启动一次；旧服务名给脚本调用提供转发入口。Installer 运行时与安装后系统都需要覆盖。
- `/boot/leonos`、ESP `/leonos`、`leonos-installer-iso.marker` 涉及启动与回滚识别。新安装使用 `/boot/reliefos` / ESP `/reliefos`；升级器先验证新旧成套载荷、原子发布新 GRUB 配置，再保留旧载荷供回滚。UEFI 标准 `EFI/BOOT/BOOTX64.EFI` 不改。

### 4. 包、SDK、发布与网站

- 新包系列使用 `reliefos-*`，更新 `configs/apk-ownership.json`、`tools/build/apk-stage.sh`、RPR 元数据/依赖/签名与 `tests/integration/test-apk-upgrade.sh`。旧 `leonos-*` 包需要显式替换/升级路径；不能伪造 `provides`、绕过签名、删除旧包数据来过测。包版本按现有递增规则发布。
- 新产物为 `reliefos.vmdk`、`reliefos-live.iso`、`reliefos-installer.iso`、`reliefos-musl-sdk.tar.gz`；Pages 下载、SHA256SUMS、CI artifact 与发布文档均由同一变量/目标得到，不允许页面指向不存在的文件。旧下载 URL 可由发布端保留重定向或归档，不靠修改历史校验和伪装新包。
- `Kconfig`、`/etc/os-release`、GRUB 菜单、安装器、登录、桌面、Fastfetch、MOTD、帮助文件、翻译、网站使用 ReliefOS；内核介绍、版本页、bug 模板使用 ReliefNT。沿用现有无文字 `logo.png` 和地球壁纸；更新 logo alt、引用与资产说明，并核验 GRUB 主题、站点和镜像中没有烘焙的旧品牌文字。
- 代码不能假定 `github.com/LeonOSProject/ReliefOS`、`.../ReliefNT` 或新 Pages URL 已存在。`.gitmodules`、默认 RPR URL、MOTD、CI sync/publish 地址仅在外部仓库、Pages 和签名密钥准备好且可访问后切换；过渡期旧 URL 属于可解释的兼容残留。

## 错误与回滚策略

- 旧配置迁移只复制有效内容；失败时保留旧安装可启动，并在日志中报告旧/新路径和错误码，不删除源文件。
- 包名切换的升级失败不得产生“数据库认为新包已装、文件仍是旧包”的状态；先验证仓库签名与完整 payload，再交给 APK 的真实事务。
- 子仓改名或远端切换失败时继续使用已验证的 gitlink/旧远端，不把不可 fetch 的 URL 写进 `.gitmodules`。
- 启动载荷发布失败时旧 GRUB 项和旧载荷保持可用；新安装不读取旧安装的遗留路径作为唯一依据。
- 新旧运行库、头文件或 UAPI 一旦发现 ABI 不一致，停止该层切换，保留旧名称入口并修正映射，不通过修改 syscall 编号或布局掩盖错误。

## 验收矩阵

| 层级 | 证明 |
| --- | --- |
| 源码 | `git grep`/路径清单中旧名只存在于历史、第三方归属、兼容实现和兼容测试，逐条可解释；新规范名覆盖全部生产路径 |
| 构建 | `make test`、`make test-long`、子仓测试、`make -j8 all`、`make release` 在干净隔离输出目录通过；Kconfig 旧配置导入、新配置生成和增量重建成立 |
| 产物 | SDK 新旧头/库与 ELF 链接均通过；APK 数据库和升级测试通过；ISO/VMDK/Installer、Pages 与 SHA256SUMS 的文件名一致；镜像树包含新路径和兼容入口 |
| 运行 | QEMU 冷启动、新安装、旧盘升级/回滚分别验证 GRUB、登录、桌面、内核版本、包更新与配置保留；涉及 VMware 的可见体验另在 VMware 验证 |
| 外部 | 远端、Pages、RPR、CI Secret、SourceHut 同步目标经真实访问与干净递归 clone 验证后才宣称完成 |

## 保留与排除

`LeonMMcoset` 等人名/账号、`BY_AUTHOR.md` 的旧产品沿革、历史安全审计和旧设计记录、第三方版权/许可证、已发布下载物的校验值均保持其历史真实性。根 `LICENSE` 是 Apache-2.0 原文，不做品牌替换；`NOTICE` 仅更新当前项目介绍并保留正确归属。`kernel.sys`、`loader.elf` 等引导器协议名不因为 ReliefNT 品牌而改动。仓库中由用户创建的未跟踪内容、缓存和现有镜像不参与批量改名或清理。
