# LeonOS 构建系统重建：设计规格与执行 Agent 提示词

> 用户交付本文件给执行 Agent 后，按本文实施；本文不是已完成工作的报告。
> 使用 `executing-plans` 技能逐阶段执行，使用 `test-driven-development`、`systematic-debugging`、`verification-before-completion` 和 `requesting-code-review` 控制质量。技能不可用时明确记录，并执行本文等价检查，不得声称已使用。

**目标：** 以 GNU Make、C 专用工具和少量 POSIX Shell 完整替代现有自研 Python 构建系统，使构建可解释、增量正确、离线可用，并清除失效的旧入口。

**架构：** GNU Make 是唯一生产构建依赖图及并行调度器。C 程序只执行明确的数据转换、验证及专用操作；外部成熟程序继续负责 APK、文件系统和 ISO 格式。

**技术栈：** GNU Make >= 4.3、C11、POSIX Shell；首期宿主为 Linux/WSL，目标为现有 x86_64。本文件同时是规格与分阶段计划。

## 1. 可直接发送的执行提示词

请在 `/home/xiaobai/Projects/Projects/LeonOS-4` 按本文件重建构建系统。先通读仓库适用的 AGENTS.md 和本文件，再盘点当前实际代码；不要假设本文件记录的提交、文件行号或构建行为永远不变。

用户已经确定 GNU Make + C 工具 + 少量 Shell，不采用 Ninja，不采用 CMake，不采用 Rust 编写新的构建辅助工具，不以 Python 执行生产构建。禁止只用 Make 包装 `python3 build.py`，也禁止把原 Python 调度引擎逐行翻译为 C。

保留现有系统功能，包括近期 Clang 兼容性修复、EEVDF、执行锁、安装升级、APK、SDK 和现有组件。功能变更与构建重建分开；不能通过禁用失败组件让构建变绿。

先按第 12 节建立行为基线和迁移台账，然后分阶段完成实现、测试、文档及旧实现删除。每一阶段交付可验证的目标，不停在脚手架阶段。正常实现选择自行决策；确实遇到会改变产品能力或违反无 Ninja/Python 约束的依赖障碍，提供具体证据和可选方案，不能自行降级要求。

最终按第 15 节提供审查包，交回原 Agent 审查。除用户另行授权外，不推送、不合并、不发布镜像；也不要替用户创建另一任务。

## 2. 已核查的项目事实与范围边界

2026-09-19 的工作区存在用户/其他 Agent 的版本号改动：`buildsystem/state/build_number.txt`、`include/generated/build_info.h`。开始时重新运行 `git status`，不得覆盖、回退或混入不属于本任务的修改。

当前入口为 `build.py`，职责覆盖编译、配置、下载、缓存、客户端/后台任务、资源生成、SDK、APK、rootfs、镜像、QEMU 与测试。`buildsystem/` 不全是源码，也包含缓存、日志和状态，不能整体盲删。

重点旧实现：

| 当前文件/区域 | 新系统需要保留的行为 |
| --- | --- |
| `build.py`、`buildsystem/core/`、`buildsystem/components.py` | 目标依赖、组件选择、编译链接参数及失败传播 |
| `configs/components.toml`、`configs/default.conf`、`configs/profiles/` | 功能默认值、必选组件、用户配置 |
| `tools/build_musl.py`、`musl_link.py`、`leonos_musl_cc.py` | sysroot、链接顺序、SDK 包装器及 ABI 参数 |
| `tools/build_*`、`fetch_auth_upstream.py` | 第三方版本、补丁、配置、交叉编译和产物 |
| `tools/apk_distribution.py`、`apk_ownership.py`、`configs/apk-ownership.json` | 包签名、归属、安装/升级语义及本地仓库 |
| `tools/make_ext2_root.py`、`make_image.py`、`make_installer_root.py`、`make_installer_iso.py` | GPT/ESP/ext2 布局、安装器及启动参数 |
| `tools/generate_*`、`make_*icons.py`、`gen_loader_integrity.py` | 图标、字体、编码表、清单及 loader 校验数据 |
| `.github/workflows/build-installer.yml`、README、开发工具配置 | 对外构建契约、产物路径和 CI 参数 |

特别注意：`tools/build_auth_upstream.py` 已有 Meson 编译路径，Meson 通常涉及 Python/Ninja。必须确认当前选中的上游版本及具体组件，提供无该依赖的受维护构建路径或等价、可测试的适配。不能保留隐藏 Ninja 调用，也不能删掉 PAM/认证功能。若只能改上游版本，应先报告 ABI、补丁及行为风险，等待用户决定。

“不用 Python”定义为：从配置、fetch、工具 bootstrap 到 kernel/userland/SDK/APK/rootfs/全部生产镜像的执行链不运行 Python，包括生成出来的 SDK 编译器包装器。不要求删除系统提供的 Python 应用或无关开发服务。已有独立 Python 回归测试可保留在显式 `test-legacy` 目标，并列出依赖；核心构建验收必须用 C/Shell，无 Python。系统原有 Rust 组件不因本次选用 C 辅助工具而被删除；它们的既有编译器依赖需在 doctor 和依赖表中明确列出。

## 3. 非目标与禁止事项

- 不实现新的依赖图引擎、线程池调度器、内容寻址构建缓存、后台 daemon、任务数据库、远程执行平台或插件框架。
- 不为美化输出拦截所有编译命令，不以随机任务 ID 替代实际目标路径。
- 不改调度器、驱动、系统调用或软件包运行语义来迁就新构建。
- 不重写 ELF 链接器、APK 签名算法、FAT/ext2/ISO 格式工具。
- 不把所有 Python 文件视为旧构建残留；按调用关系和实际职责处理。
- 不删除上游子模块中的构建文件或补丁历史来追求字符串搜索“零命中”。

## 4. 用户命令契约

默认 `make` 等价于 `make help`，不隐式下载或开始全量构建。命令稳定如下：

| 目标 | 行为 |
| --- | --- |
| `help` | 列出目标、变量、依赖获取和常见示例，不要求编译器已安装 |
| `doctor` | 分组检查宿主工具/目标工具链/第三方依赖，缺失时非零退出 |
| `fetch` | 唯一常规联网阶段，获取锁定依赖并验证 |
| `defconfig`、`olddefconfig`、`menuconfig` | 初始化、非交互更新、交互修改输出目录配置 |
| `tools` | 只用 HOSTCC 构建本项目 C 辅助工具 |
| `kernel`、`userland`、`runtime`、`sdk` | 分别构建对应产物，不顺带生成镜像 |
| `rootfs`、`apk-repo` | 构造根目录/包仓库，阶段边界由实际文件表示 |
| `image-vmdk`、`iso`、`installer` | 生成可启动磁盘、live ISO、安装 ISO |
| `all` | kernel、userland、runtime、sdk、apk-repo、image-vmdk、iso、installer 的聚合 |
| `run`、`run-iso`、`run-installer` | 构建对应镜像后运行 QEMU；不要求 root |
| `test-tools`、`test-build`、`test-smoke` | C 工具测试、构建契约测试、QEMU 启动测试 |
| `test` | test-tools 和 test-build；长时间/虚拟机测试显式执行 |
| `test-legacy` | 已列清单的原有回归测试，明确可能依赖 Python |
| `clean` | 仅清理选中输出目录内可再生成产物，保留配置和下载缓存 |
| `distclean` | 清理选中输出目录内配置及产物；不清共享下载缓存 |

变量：`ARCH=x86_64`、`PROFILE=debug|release`、`O=...`、`V=0|1`、`CPUS`、`MEMORY`、`SOURCE_DATE_EPOCH`；目标工具链通过 `TOOLCHAIN=<受版本控制描述文件>` 配置，可显式覆盖 `CC/CXX/AR/LD/OBJCOPY/STRIP`。宿主使用独立的 `HOSTCC/HOSTCFLAGS/HOSTLDFLAGS`。不支持的 ARCH/PROFILE 或未知配置键必须报错。

示例：

```sh
make doctor
make fetch
make defconfig PROFILE=debug
make kernel PROFILE=debug -j8
make installer PROFILE=release -j8
make run CPUS=2 MEMORY=4G
make kernel V=1
make test-build
```

复用 Make 原生诊断：`make --trace`、`make -n`、`make -p`。不承诺把 `make -n` 当绝对无副作用沙箱：Make 可能重建被 include 的生成文件及执行递归规则，必须在文档说明；普通 help/doctor/clean 不得触发配置再生或生产构建。

## 5. 目录及职责

```text
Makefile                         公共入口、目标选择及 mk include
mk/{host,toolchain,config}.mk     宿主工具、目标参数、配置生成
mk/{kernel,userland,runtime}.mk  编译链接依赖
mk/{third-party,sdk,apk}.mk      上游组件、SDK、软件包
mk/{rootfs,images,run,tests}.mk  组装镜像、运行和测试
mk/components/*.mk              按组件显式定义源文件和特殊参数
configs/toolchains/*.mk         可审查的工具链描述
configs/dependencies.lock.json  固定版本、URL、摘要、补丁、许可
tools/host/common/              仅确有复用的 buffer/io/path/process 工具
tools/host/{config,manifest,version,assets}/  各自独立的 C 程序
tools/build/*.sh                短小的上游配置及外部工具调用
tests/build/                    Shell 契约测试与 C 测试 fixture
out/<arch>/<profile>/           默认输出根
  host/ obj/ generated/ config/ sysroot/ stage/ packages/ images/ logs/ meta/
cache/downloads/                校验过的下载文件；与 profile 无关
```

新目标路径是 `out/<arch>/<profile>/images/leonos4-installer.iso` 等，更新 README/CI/测试消费者，不永久维护 `build/` 路径别名。迁移期间旧 `build/` 可用作只读行为基线；不要删除其中用户虚拟磁盘、Linux 参考源码或不属于本任务的资料。

C 程序按职责拆分，不强求上述目录各有一个可执行文件；不为每条 cp 命令创建工具。格式解析优先复用仓库中已有、许可清晰的 C 库；若新增 JSON/TOML 库，固定版本并加入许可清单。禁止用正则或临时字符串切片冒充完整 TOML/JSON 解析器。

## 6. Make 依赖与增量正确性

### 6.1 单一依赖图

自有组件用 include 组成同一个 Make 图；只在上游项目要求时递归 `$(MAKE)`，继承 jobserver，不写死子 make 的 `-j$(nproc)`。非 Make 上游构建不得启动无界并行；默认内部串行或使用明确受限策略并记录，不能全局 `.NOTPARALLEL` 掩盖依赖错误。

目录创建为 order-only prerequisites。链接器脚本、生成头、资源、补丁、工具本身、配置和源清单都是显式输入。编译器使用 `-MMD -MP` 等生成真实头依赖；链接顺序显式定义，不对有顺序要求的库列表随意排序。

对象路径包含组件及源相对路径，避免两个 `util.c` 冲突。源文件优先由组件清单声明；自动发现必须检测新增/删除并生成稳定排序的成员清单，使删除源文件也触发重新链接。静态库重建到新临时文件，不对旧 archive 只追加对象而遗留已删除成员。

### 6.2 参数与工具链变更

每类编译/链接操作生成内容稳定的参数签名文件，记录实际 argv、工具绝对路径及工具身份、目标、配置、相关脚本输入。签名仅内容改变时原子替换。只依赖 Makefile 时间不足以发现命令行 `CFLAGS` 等覆盖。

可使用一次轻量 FORCE 检查更新签名，但 FORCE 不直接挂到对象/链接/镜像上；被检查但内容不变的签名不得改 mtime。按组件/动作拆签名，应用专有参数变化不能让整个内核重编译。不把全部环境变量纳入签名，只记录真正消费的变量；默认未声明的环境不得悄悄改变工具选择。

识别 GNU Make 内置 `CC=cc`：使用 `$(origin CC)` 区分内置 default、用户命令行和配置，不能用 `CC ?= clang` 假装覆盖内置默认值。doctor 校验目标 triple、链接器和 compiler-rt 存在性，避免误选 PATH 中不完整的自定义 Clang。

### 6.3 原子输出、多输出与并发

启用 `.DELETE_ON_ERROR`，文件写入同目录唯一临时文件，再 rename；工具必须检查 write/close 的失败。多产物规则使用 GNU Make grouped targets `&:` 或能证明任一输出丢失都会重建的等价设计。单个 stamp 存在不能代表所有输出完整。

同一 O 目录两个顶层构建同时运行必须明确拒绝其中一个，或经测试安全串行；不同 O 目录可同时构建。锁只解决进程间互斥，不替代目标依赖；递归上游 make 不能重复获取外层锁造成死锁。若使用 flock，写明依赖和退出行为。临时目录、日志、端口和 QEMU 镜像不能跨配置共享。

源根和 O 路径首期不支持空白、换行及 Make 特殊元字符，启动时给出明确拒绝并列出受支持字符；不要声称完整支持任意路径。rootfs 内的合法文件名通过清单及 argv 处理，不按空格拆分。所有清理必须验证 O 是本项目带所有权标记的非源码目录，拒绝空值、`/`、源码根及逃逸符号链接，不能直接 `rm -rf $(O)`。

## 7. 配置、版本与生成资源

- Kconfig 是功能选择权威，`configs/components.toml` 是组件元数据权威，profile 是编译策略权威；不要再复制一套 defaults 到 C 常量或 Make 中。
- 使用仓库固定的 C Kconfig 前端；配置输出在 O/config。首个构建缺配置可从 default 初始化，但 help/clean 不得配置；旧配置迁移必须保留合法用户选择，未知/失效符号有可读报告。
- 配置转换工具输出 Make include 和 C 头，内容稳定、相同输入不更新时间。处理 generated include 的 Make 重启，禁止无限再生循环。
- 版本头移至 O/generated，通过 include 路径消费，移除源码目录自动写入。普通 build 不递增受版本控制的数字，不嵌入每次调用的当前时间。
- 版本来自发布版本、固定提交标识和显式 BUILD_ID（可选）。时间优先 SOURCE_DATE_EPOCH，未设置时以提交时间为稳定默认；dirty 标记不得依赖构建自身产物造成循环失效。
- 资源必须选择可重现的 C 生成器或明确锁定的外部非 Python 工具。不要把 Python 生成结果提交后就省略可追溯的再生路径。字体、图片编码库的引入需要依赖及许可证记录。

## 8. C 工具接口与安全质量契约

每个工具提供 `--help`、明确输入文件/输出文件选项及非零失败码；只转换传入数据，不扫描 Git 来推测构建依赖、不自己启动 make、不偷偷下载依赖。

建议共用接口（只在两个以上调用者有真实需求时抽取）：

```c
struct byte_buffer { unsigned char *data; size_t len; size_t capacity; };
/* 0 成功，-1 失败并设置 errno；扩容失败不得丢失旧缓冲区。 */
int buffer_reserve(struct byte_buffer *buf, size_t required);
void buffer_destroy(struct byte_buffer *buf);
/* 相同内容保留目标 mtime；失败保留原文件，临时文件位于目标目录。 */
int write_file_if_changed(const char *path, const void *data,
                          size_t size, unsigned int mode);
/* argv 必须以 NULL 结束；返回启动状态，child_status 单独记录退出/信号。 */
int run_process(const char *program, char *const argv[],
                const char *working_dir, int *child_status);
```

调用者在头文件注释中获得内存所有权、错误和生命周期契约。处理 EINTR、短读写、size_t 加法/乘法溢出、格式化截断、资源关闭及子进程 wait。禁止无检查 strcpy/strcat/sprintf，禁止用固定 PATH_MAX 缓冲区静默截断路径，禁止把用户字符串当 printf 格式。

执行外部程序使用 argv 和 posix_spawn 或 fork/exec；不拼接 `system()`/`popen()`。子进程失败和信号必须映射为工具失败，日志不能吞掉 stderr；若工具创建进程组，Ctrl-C 后清理其子进程，不杀其他构建进程。临时文件使用唯一创建方式，不能多个任务共用 `/tmp/output`。

新 C 工具必须在 GCC 和 Clang 下通过 `-std=c11 -Wall -Wextra -Wpedantic -Werror -Wformat=2 -Wshadow -Wstrict-prototypes -Wmissing-prototypes`，使用 POSIX API 时声明所需 feature macro。`-Wconversion` 可作为补充审查，不用大量无说明 cast 换取零警告。第三方代码单独设置诊断范围，不给整个工程关闭警告。

工具边界测试运行 ASan/UBSan；有复杂解析器时加入受大小限制的模糊测试入口。不得使用 `assert` 处理外部非法输入；assert 只用于内部不变量。不能用空实现、永远成功的返回值、无条件忽略退出码或宽泛 catch 掩盖未迁移功能。

函数超过约 80 行或文件超过约 600 行必须解释职责，作为审查信号，不为了数字机械拆函数。注释说明约束与原因，不复述代码；不引入没有调用者的泛化框架。

## 9. 第三方依赖、下载与离线

锁文件 schema_version=1，按稳定 id 保存 URL/版本或提交、SHA-256、解包目录、补丁路径及摘要、构建适配脚本、许可路径。已有 JSON/TOML 清单若承担同一职责，应迁移而非长期并存两个权威来源。

fetch 通过 curl 等成熟工具下载，保留标准代理变量和 TLS 校验。先下载 `.partial`，验证摘要/上游签名后进入缓存；并发同包使用独立临时文件及缓存锁。摘要不匹配不得自动更新锁文件。APK 索引可变内容不能伪装可重现输入，固定实际包版本和字节摘要，保留签名验证，不使用 allow-untrusted 绕过。

依赖源码和缓存只读消费；补丁在配置专属工作目录应用。构建时缺依赖直接列出 id 和 `make fetch`，不自动联网。防御解包绝对路径、`..` 逃逸和符号链接逃逸，使用已验证的解包工具或验证清单。上游 configure 允许生成 Shell/Make 文件，但禁止默认路径中调用 Meson/Ninja/Python。

## 10. Rootfs、APK、镜像与 SDK

rootfs 清单用带 schema_version 的 JSON，每项描述 type(file/dir/symlink)、source、绝对 guest path、mode、uid/gid、owner component、symlink target。主机文件路径与 guest path 明确分离。重复目标默认报错；只有显式且有顺序的覆盖规则可覆盖。

组装时从空 staging 开始，成功后原子发布；不向上次 staging 持续 cp 以免残留删掉的文件。拒绝 guest path 的 `..`、NUL 与 staging 逃逸；合法的绝对 guest symlink 不得在主机上跟随。保留权限、链接语义以及 APK 所有权契约。

普通构建不得要求 root/sudo，不操作真实块设备，不全局 mount。使用 mke2fs、mtools、xorriso、qemu-img 等既有工具及明确的 rootless 方式保留 guest 元数据。镜像分区、GRUB 配置、loader 校验、安装器尺寸和许可选项必须与基线一致。

每种镜像写出产物元数据：源码提交、配置摘要、输入包摘要、工具版本、镜像摘要。固定 SOURCE_DATE_EPOCH 时，将时间、目录顺序、UUID/volume ID 等可控字段固定；如第三方格式仍有非确定字段，具体记录并提供解包内容比较，不能宣称字节级可重现。

SDK 编译器包装器改为 C 或短 Shell，安装前缀可移动，不能嵌入开发者绝对工作区路径。验证最小 C 程序和现有 SDK 示例的编译链接；原有目标 Rust 组件、应用、认证与存储组件按台账完整迁移。

## 11. 输出、诊断与运行体验

默认输出简短的 `HOSTCC/CC/LD/GEN/PACK/IMAGE` 和产物路径；V=1 输出实际命令、工作目录及明确的环境覆盖。失败时保留外部程序原始诊断、失败目标、命令和日志位置；Shell 管道必须可靠传播生产者失败，不能让 tee 的成功覆盖编译失败。

不强制自定义主题；CI/非 TTY 默认无颜色。长第三方构建可落地日志，失败必须展示关键尾部及完整路径。Make 并行输出采用原生 output-sync 可选配置，不引入另一个任务 UI 服务。

QEMU 参数从同一输出目录获取镜像与固件，run 的 CPUS 仅表示配置虚拟 CPU 数，不能声称系统已启用 AP 调度。测试必须识别来宾成功标记并核验退出状态，启动 QEMU 进程本身不算通过。

## 12. 分阶段实施计划

每阶段使用 TDD：先写能暴露缺失行为的测试，记录失败，再实现，再运行相关回归。测试关注契约，不断言函数名/文本拼接等实现细节。开始阶段记录基线，不把旧系统结果当新系统验证。

### P0：盘点与基线

- [ ] 在 `docs/build/migration-inventory.md` 列出所有旧生产目标、实际输入输出、Python 调用、消费者、新归属及验证命令；同时列出原有测试覆盖。
- [ ] 对当前 kernel、SDK、APK、三类镜像保存日志、文件清单、权限、启动/安装行为及耗时；外部依赖不可用时明确阻塞，不能记为通过。
- [ ] 审计上游 Meson/Ninja/Python 路径，先解决构建依赖可行性；若违背约束则在实现大规模替换前向用户报告。
- [ ] 确认输出与缓存空间，使用独立 `xiaobai/rebuild-make-c` 分支或用户指定分支，保护其他 Agent 未提交修改。需要 worktree 时按 using-git-worktrees 技能处理。

### P1：宿主工具、配置与内核闭环

- [ ] 创建 Makefile、mk/host.mk、mk/toolchain.mk、mk/config.mk、mk/kernel.mk 和对应 C 工具。
- [ ] `tests/build/test-bootstrap.sh` 检查 help 无工具依赖、HOSTCC/CC 区分、缺工具错误、不合法 O 拒绝。
- [ ] C 测试覆盖空输入、截断输入、溢出、只读输出、write-if-changed、子进程非零和信号。
- [ ] 从干净 O 构建 kernel，运行现有内核启动验证；不复用旧 kernel 充数。
- [ ] 测试源文件、头文件、链接脚本、编译参数变化及重复空构建，提供实际命令次数与输出 mtime 证据。

### P2：runtime、应用、SDK、第三方

- [ ] 迁移 mk/runtime.mk、mk/userland.mk、mk/third-party.mk、mk/sdk.mk、组件列表和专用脚本。
- [ ] 从新 O 生成 musl sysroot、runtime 和 SDK，验证程序编译及在来宾加载；保留 compiler-rt 和原链接规则。
- [ ] 测试移除组件源文件不会残留 archive 成员；两个同名源文件不冲突；改变单个应用不编译内核。
- [ ] 两个不同 O/profile 并行构建，验证参数、生成头和 sysroot 不串用；同 O 并发按契约拒绝/串行。

### P3：资源、APK、rootfs 和镜像

- [ ] 迁移资源生成、清单、APK 所有权、SDK 打包与镜像工具；建立 mk/apk.mk、mk/rootfs.mk、mk/images.mk。
- [ ] 对 rootfs 添加/删除/重命名文件、关闭组件、软链接、权限和覆盖冲突编写 fixture 测试。
- [ ] 从新产物生成 VMDK、live ISO、installer ISO，验证分区和文件系统，并运行 QEMU 启动与安装/升级测试。
- [ ] 升级测试包含已安装 Alpine build-base/xorriso 等包的旧系统，验证配置及第三方软件包保留，不只测全新安装。

### P4：入口切换、离线与残留删除

- [ ] 修改 `.github/workflows/build-installer.yml`、相关发布 workflow、README、开发工具及测试路径，不遗漏可选安装器参数。
- [ ] 在隔离环境执行 fetch，再禁止网络，完成全套生产目标。跟踪 execve 证明生产路径没有 Python/Ninja；容器镜像有 Python 文件不是失败，实际执行它才是失败。
- [ ] 建立 `docs/build/legacy-removal.md`，每个旧文件列出替代者、调用者迁移证据或保留理由；完成后删除 build.py、自研引擎及无调用者旧构建工具。
- [ ] 明确区分源码删除和本地缓存/磁盘，不删除用户旧镜像；不保留静默转调旧系统的 fallback。
- [ ] 更新 `.gitignore`，证明构建不修改受跟踪文件；保留原有变更基线，不能用 git reset/checkout 清理验证现场。

### P5：最终验收与审查包

- [ ] 在独立干净 checkout 完成第 13 节矩阵，记录命令、退出码、日志、产物哈希及实际运行环境。
- [ ] 运行 requesting-code-review（可用时请求独立审查），修复 P0/P1/P2 问题并重跑受影响检查；未解决项明确列出，不自称验收通过。
- [ ] 输出第 15 节交付物，交回原 Agent 审查，不擅自合并或推送。

## 13. 强制验收矩阵

| 编号 | 场景 | 通过条件 |
| --- | --- | --- |
| A01 | 干净 clone、只有声明的依赖 | fetch 后所有生产目标可构建，无旧 build/产物依赖 |
| A02 | 重复 make kernel/installer | 不执行编译、链接、打包；产物及生成头 mtime 不变；允许轻量内容检查 |
| A03 | 改一个普通 C 文件 | 仅相关对象及真正下游目标更新 |
| A04 | 改公用头/链接脚本 | 正确更新全部消费者，无漏编 |
| A05 | 改编译参数、工具身份、profile | 必要目标失效，其他 profile 不受污染 |
| A06 | 删除源文件/资源/禁用组件 | 链接及 rootfs 不残留旧成员 |
| A07 | 删除多输出规则中的一个文件 | 自动重建完整输出，不被 stamp 误判 |
| A08 | -j1 与 -j8，重复三次 | 无竞态/缺文件；可比的解包内容一致 |
| A09 | 两配置并行、同配置双进程 | 不串用输出；同配置行为符合互斥契约 |
| A10 | 中断、磁盘写失败、子进程失败 | 非零退出，旧有效产物保留或无伪成功产物，重跑恢复 |
| A11 | 错误 URL/摘要、断网 | fetch 明确失败；缓存完整时离线全量成功 |
| A12 | 恶意/异常路径与清理 | 不越界写入/删除；拒绝危险 O、重复清单目标 |
| A13 | 同输入、固定 epoch、两个干净 O | 可控产物哈希一致；不可控字段有逐项证据及内容比较 |
| A14 | C 工具双编译器及 sanitizer | GCC/Clang 零新增警告，ASan/UBSan 无错误 |
| A15 | SDK、APK、镜像与安装升级 | 最小程序运行、包归属/签名正确、三类镜像可启动、升级保留用户环境 |
| A16 | 无 Python/Ninja 生产调用 | 配置、fetch、SDK 使用及完整镜像构建的 exec 跟踪无违规 |
| A17 | 旧入口删除与源码整洁 | CI/文档无活跃旧入口；构建前后受跟踪文件无新增变化 |

核心测试 harness 使用 POSIX Shell/C。A10 用测试 fixture 或容量受限的临时文件系统，不破坏真实工作区。A13 不允许通过删除生成时间/UUID 字段以外的任意差异来伪造一致性。CI 资源不足的长测试必须列为未运行，不能以 skip 状态宣传全通过。

## 14. 技能使用与质量门禁

1. 执行前阅读适用 AGENTS.md。本文件已确定总体方案，brainstorming 用于真正新出现的架构分歧，不重复询问已决定的语言/构建引擎。
2. 用 executing-plans 跟踪 P0–P5。若要多 Agent 并行，先用 dispatching-parallel-agents/subagent-driven-development 明确文件所有权；共享 Make 总入口由一个人整合，不并发修改。
3. 用 test-driven-development 开发复杂 C 工具和增量规则；先运行失败案例，禁止先实现后补一个只检查文件存在的测试。
4. 遇到失败用 systematic-debugging，记录第一个真实失败点；不要连续堆兼容开关、sleep 或强制全量重编译。
5. 按 verification-before-completion 保留实际命令结果，验证过的源码/配置/产物要能对应；改代码后只重跑受影响检查，但最终全套生产构建必须覆盖最终版本。
6. 用 requesting-code-review 检查依赖正确性、安全清理、缓存完整性、功能等价和旧代码删除；用 receiving-code-review 逐项验证并处理反馈。
7. 禁止把“测试启动了”“编译了工具”“ISO 文件存在”当成完整完成。不得为了绿灯降低断言、忽略失败或默默删测试。

## 15. 交给原 Agent 的最终审查包

必须提供：

- 基线提交、最终提交或完整 diff、任务前已有修改列表；按阶段组织的提交/改动说明。
- `docs/build/migration-inventory.md`：旧→新目标/脚本/行为映射，每项有验收证据。
- `docs/build/legacy-removal.md`：删除项及剩余 Python/Ninja 引用的职责说明。
- `docs/build/verification.md`：A01–A17 逐项结果、完整命令、退出码、日志路径、耗时、未执行理由。
- 新构建使用文档：依赖安装、fetch、配置、常用目标、错误诊断、清理、SDK 和输出路径。
- 最终镜像/SDK/包仓库的路径及哈希，说明实际启动过哪个哈希；不得交付旧版本镜像冒充新构建结果。
- 性能记录：同宿主、同配置的 clean/kernel incremental/no-op/installer 构建耗时和峰值内存；无可比基线就只报告观察，不承诺提升倍数。
- 已知限制、未完成需求、第三方构建适配及许可变化。

原 Agent 审查重点：Make 是否真正掌握依赖；参数变化及源删除是否正确；无 Python/Ninja 是否贯穿 SDK 和第三方；rootfs 是否残留；无操作构建是否稳定；清理是否安全；是否误删用户数据或原有功能。通过前不得宣布迁移完成。
