# ntclks 分离：Agent 执行提示词与阶段计划草案

> **面向 AI 代理的工作者：** 设计获批后使用 superpowers 的 executing-plans 逐任务实现；功能或缺陷修复采用 test-driven-development，异常采用 systematic-debugging，完成声明前采用 verification-before-completion。是否使用子 Agent 以用户授权及当前技能规则为准。

**目标：** 将 ntclks 与用户态源码、构建和产品策略分离，以新的 Git 仓库和真实 submodule 交付，并整理文件职责。

**架构：** 内核仓库拥有 Ring-0、loader、UAPI、独立工具和配置。LeonOS-4 拥有 runtime、应用、服务、发行版配置、SDK 装配和整机集成，通过安装制品与导出头文件消费内核。

**技术栈：** C/汇编、GNU Make、clang/LLVM、musl、现有 shell/C 宿主工具；Python 仅用于现有非生产回归和维护。

**状态：** 配套设计尚待批准，以下步骤均未执行。本文可直接复制给后续 Agent；交付本文不构成创建仓库、实现、commit 或 push 的授权。

设计：[ntclks 独立仓库与用户态分离](../specs/2026-09-24-ntclks-separation-design.md)。调研基线为 `main@7123bff3ebc5b9042b2d43270c5978d21c2604f7`。

## 一、可直接复制的主提示词

```text
请使用 superpowers，为 LeonOS-4 执行 ntclks 独立仓库与用户态分离重构。

先阅读仓库 AGENT.md/AGENTS.md、当前 Git 状态，以及：
- docs/superpowers/specs/2026-09-24-ntclks-separation-design.md
- docs/superpowers/plans/2026-09-24-ntclks-separation-agent-prompt.md

先检查本消息是否包含明确的设计批准。没有批准时，只复核设计、报告与当前
代码的差异及需要决定的事项，不开始实现。用户已批准时，不重复请求同一批准。
不要沿用历史 feature/new-tty 任务的分支或提交授权。

采用设计中的方案 B：
1. 新内核仓库默认名 ntclks，挂载在 kernel/ntclks/。它拥有内核核心、所有
   Ring-0 驱动、OSTUI 的内核 console 实现、kerneldebug、loader、UAPI、
   最小启动资源及独立构建/配置/测试工具。
2. 主仓库保留用户态、musl/PAM 和第三方适配、服务与会话策略、GRUB 配置、
   rootfs、SDK 装配、安装器和镜像发布。userland/libc 归整为 userland/runtime。
3. 内核可以在完全没有主仓库源码的 checkout 中构建；用户态只引用已安装 UAPI
   与用户态 API；禁止通过父目录、符号链接或宽泛 include 绕过边界。
4. 混合头按声明拆分，UAPI 唯一源归内核；boot/module API 与用户 UAPI 分开；
   SDK 不能维护重复 ABI 定义。不更改现有 syscall/TTY ABI、结构布局或 libc。
5. kernel/ntclks/user 是 Ring-0 进程机制，不是用户态程序。将服务、Desktop、
   Installer、测试 autospawn 策略迁到用户空间；保留 exec/凭据/TTY 等机制。
   对 Desktop/windowd 的权限依赖建立调用矩阵和负例测试，不得通过删除检查
   或默认放行来完成解耦。替代不完整时必须如实报告，不能宣称彻底分离。
6. 保留 GNU Make 生产链与 jobserver；内核有独立入口，父 Make 只调用公开入口
   并消费制品。配置、版本和输出各自有明确所有者；生产链不调用 Python。
7. 保持 kernel.sys + loader.elf 配对、五个 .drv、kerneldebug.sys、SDK 和镜像
   内现有路径，保持更新失败回滚。完成真实 VT/Desktop/Installer QEMU 回归。

按照本文阶段清单执行。每阶段先定义失败测试/基线，完成最小改动，运行对应
测试，审查后进入下一阶段。机械移动与行为修改分开。不要自行升级整个工具链
或重写不相关子系统。保护已有工作，不清理用户镜像、不重写父仓历史。

真实 submodule 需要用户提供或明确确认新仓库远端、可见性和历史策略。不要猜
URL，不提交本地 file URL，不把嵌套 .git 目录或复制源码冒充 submodule。
远端尚未准备时可以完成已授权的本地边界工作和验证，发布动作等待所需信息。

当前仓库规则要求明确授权才能建分支、commit、push、创建 PR。遵守最新规则，
不要把本提示词里的示例提交名理解成授权。授权发布后，先推送可获取的子仓
提交，再更新父仓 gitlink；最后用全新递归 clone 验证。

最终报告：源文件迁移映射、两个仓库状态与 SHA、实际远端、构建/测试命令及
退出码、QEMU 日志与截图路径、未完成项和失败原因。严格区分静态检查、编译
成功、镜像生成、真实 guest 验证。每次交接写清下一步，不夸大验证范围。
```

## 二、执行前参数

| 参数 | 建议/要求 |
| --- | --- |
| 设计批准 | 用户明确批准后实施 |
| 主仓库 | 当前 checkout；重新读取 HEAD，不能假定仍为调研基线 |
| 新仓库远端 | 用户提供真实 URL；未提供不得发布 gitlink |
| 挂载点 | 建议 `kernel/ntclks/` |
| 历史策略 | 建议保留相关历史，在一次性 clone 中提取 |
| 分支与提交 | 由当前用户授权决定；不自动使用旧 `feature/new-tty` |
| 输出目录 | 独立、绝对路径；不得覆盖用户现有测试磁盘 |
| 架构/profile | 先保留 x86_64 release/debug 支持 |

## 三、文件职责清单

| 工作单元 | 当前需要读取/修改的文件 | 拟新增或迁移的结果 |
| --- | --- | --- |
| ABI | `include/uapi/`、`include/leonos/`、`include/linux/`、`userland/libc/include/`、`devtools/include/` | 内核 UAPI；runtime API；boot/module 私有域；头文件分类表 |
| 内核完整集 | `mk/kernel.mk`、`mk/boot.mk`、`kernel/`、`drivers/`、`boot/loader/` | 新仓 Makefile/mk/configs/tools/resources 与核心源码 |
| 产品集成 | `Makefile`、`mk/config.mk`、`mk/runtime.mk`、`mk/userland.mk`、`mk/sdk.mk`、`mk/rootfs.mk`、`mk/images.mk`、`mk/rpr.mk` | 内核适配器、导出制品依赖、sysroot 装配 |
| 配置/版本 | `Kconfig`、`Kconfig.components`、`configs/components.toml`、`configs/build-version`、`tools/host/` | 两套有边界的配置/版本生成和有限参数映射 |
| 策略 | `kernel/ntclks/user/userland.c`、相关权限调用者、`userland/apps/gptinit/`、`userland/apps/sessiond/` | exec 机制与会话/测试策略分别归属 |
| SDK/更新 | `devtools/`、`tools/build/`、`userland/storage/leonos-kernel-update` | 无重复头的 SDK、成对更新/回滚 |
| 测试/CI | `mk/tests.mk`、`tests/build/`、`tools/test_*.py`、`tools/tests/`、`.github/workflows/` | 子仓单元测试、父仓集成测试、递归 checkout |
| 规范 | `AGENT.md`、README、ABI/启动/边界文档 | 新路径、作用域、贡献/双仓工作流 |

实现者应补充逐文件 CSV/Markdown 映射；本表不能替代头文件与权限审计。

## 四、阶段执行清单

### 阶段 0：基线与依赖闭包

- [ ] 读取政策、Git status、HEAD、递归 submodule 状态；保护现有工作。
- [ ] 列出所有内核输入：源码、文本包含、头、资源、生成器、配置、链接脚本。
- [ ] 逐个标注 include/leonos 声明属于 UAPI/runtime/boot/module/private/resource。
- [ ] 记录当前完整构建及已有 VT/Installer 回归结果；失败先归类，不暗中降低验收。
- [ ] 产出迁移清单与权限调用矩阵；更新设计中的实际差异。

可直接执行的只读基线命令：

```sh
git status --short
git rev-parse HEAD
git submodule status --recursive
git ls-files kernel drivers boot include userland mk tests tools
rg -n 'kernel/ntclks|drivers/bootstrap|include/uapi' mk tests tools .github
```

退出条件：能解释独立内核需要哪些当前父仓输入，以及每项如何迁移或替代。

### 阶段 1：可验证的头文件边界

- [ ] 先写头文件导出白名单、自包含编译、ABI size/offset/value 比较测试，确认能捕获私有头泄漏与布局改变。
- [ ] 拆分混合声明；保持现有 include 名的必要兼容转发，不保留跨仓相对 include。
- [ ] 实现独立 headers_install，SDK/runtime 改用其结果，清理重复模板。
- [ ] 跑 UAPI 与实际 musl sysroot 的 ABI 测试；从空目录编译 SDK 示例。
- [ ] 删除一个已导出头后再安装，确认没有残留；私有头变化不触发用户态全编译。

退出条件：用户态编译命令中没有内核私有 include；布局比较通过。

### 阶段 2：独立内核 checkout

- [ ] 先建立“只有内核源码和工具链”的构建测试，当前未闭合依赖应可被识别。
- [ ] 在独立临时目录迁移完整 Ring-0、loader、字体、配置和必要工具；保留许可证。
- [ ] 实现设计规定的 all/headers_install/install/test/clean 入口和 manifest。
- [ ] kernel/loader 配对完整性、模块产物与 debug 产物分别核验；避免重复编译 storage 子文件。
- [ ] 从无父源码的 checkout 完整构建、并行构建、二次增量、独立 host 测试。

退出条件：不依赖父仓绝对路径、环境注入头文件或旧输出；所有依赖可解释。

### 阶段 3：父构建和镜像集成

- [ ] 为导出变更、子仓 dirty 修改、删除制品、构建失败、并行调用建立回归。
- [ ] mk/kernel.mk 改为递归 Make 适配器，mk/boot.mk 改为产物消费者；保留 jobserver。
- [ ] runtime/userland/SDK 使用导出头；配置映射、源版本和 SOURCE_DATE_EPOCH 明确。
- [ ] 更新 rootfs、ISO、Installer、RPR 和 kernel update 的制品输入及配对校验。
- [ ] 执行 `make test` 和独立 O 的 `make all`；额外测试现有增量、旧 O、release/debug。
- [ ] 用相同输入在两个不同路径构建，比较预期可重现产物；解释必要的非确定性。

命令中的 O 使用新绝对路径，先运行 `make help`/`make doctor` 了解当前前置条件。
退出条件：完整产品生成且无旧路径输入；父仓不再编译子仓源码。

### 阶段 4：职责目录和运行时策略

- [ ] 分组移动 arch/mm/fs/net/exec/console/debug；每组更新测试、文档和依赖。
- [ ] userland/libc 改名 runtime，更新 Make、SDK、测试和维护脚本；不改 musl 上游。
- [ ] 将 autospawn 及服务/会话编排迁入用户态，保留测试启动方式的可用性。
- [ ] 对身份/特权路径先写失败负例：普通用户不能冒充 windowd/Desktop，exec 不继承不应保留的特权。
- [ ] 通过现有标准凭据与设备所有权机制解耦；接口不足时先审查专项设计。
- [ ] 验证 fork/exec/fd/session/TTY、图形服务访问及失败恢复。

退出条件：内核不依赖产品服务清单和 Desktop 构建配置；权限负例通过。
不能以“先删除检查、之后补测试”交付，也不能将未完成的策略解耦隐去。

### 阶段 5：真实 submodule 与 CI

- [ ] 获得实际远端与历史策略；只在临时 clone 做历史提取，保存来源映射。
- [ ] 在用户授权范围内提交/发布子仓，并从远端取回该 SHA 验证。
- [ ] 受控替换同名已跟踪目录为 gitlink，设置可移植 `.gitmodules` URL。
- [ ] 更新 CI 递归 checkout；无 submodule 时明确报错；生产构建不隐式 fetch。
- [ ] 测试 detached HEAD 日常开发说明、dirty 发布拒绝、旧 SHA 回退和版本配对。
- [ ] 从全新 `clone --recurse-submodules` 重做完整构建，证明不是依赖本地对象。

退出条件：父仓指向可获取的子仓 SHA。远端或发布授权缺失时，报告具体缺项。

### 阶段 6：整机验收与 review

- [ ] 分别执行 live/installed/installer VT QEMU 测试，记录实际命令、guest 日志和退出码。
- [ ] 验证 Ctrl+Alt+F1～F6、controlling terminal、setsid/TIOCSCTTY、前台进程组及 shell 三个标准 fd。
- [ ] 验证 tty1 无 Desktop 登录、Desktop 启动/退出恢复、日志隔离、Installer 退出/恢复及键盘归属。
- [ ] 验证内核/loader 更新成功、拒绝错配和失败回滚，使用独立测试介质。
- [ ] 审查跨仓依赖、UAPI 泄漏、权限、资源所有权、输出路径和许可证。
- [ ] 更新架构文档与交接记录，清楚标注每项验证证据及限制。

现有脚本入口：`tools/test_uapi.py`、`tools/test_musl_abi.py`、
`tools/test_console_boot_policy.py`、`tools/test_installer_input.py`、
`tools/test_vt_qemu.py`、`tools/test_vt_installed_qemu.py`、`tools/test_vt_installer_qemu.py`。
先读参数与 fixture 实现，再执行；不要凭文件名宣称覆盖全部验收。

## 五、提交与交接模板

仅在用户明确授权提交时使用中文 Conventional Commits；子仓先提交，父仓再更新 gitlink。建议按独立验证单元拆分，例如：

- `refactor: 分离内核 UAPI 与用户态运行库接口`
- `build: 建立 ntclks 独立构建与制品导出`
- `refactor: 通过导出制品集成内核构建`
- `refactor: 整理内核与用户态源码职责`
- `refactor: 将会话启动策略迁至用户态`
- `build: 以 submodule 接入独立内核仓库`
- `test: 增加跨仓构建与启动回归`

每个阶段结束或额度不足时写交接，格式如下：

```text
设计批准与授权范围：
主仓库路径/分支/HEAD/dirty：
子仓库路径/远端/HEAD/dirty：
当前阶段及已完成步骤：
修改文件和迁移清单路径：
实际执行命令、退出码、日志路径：
QEMU 验证项目与 fixture/磁盘路径：
已知失败、未执行测试、剩余风险：
正在运行且由本任务创建的进程：
下一步具体动作与所需输入：
```

完成报告不能只写“编译成功”。必须同时证明独立内核 checkout、真实递归 clone、用户态头文件边界和整机行为；任何缺项都标为未完成。
