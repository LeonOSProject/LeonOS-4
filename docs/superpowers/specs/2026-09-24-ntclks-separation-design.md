# ntclks 独立仓库与用户态分离设计

日期：2026-09-24。状态：**设计草案，供审批；尚未实施、编译或进行 QEMU 验证。**

调研基线：LeonOS-4 `main`，`7123bff3ebc5b9042b2d43270c5978d21c2604f7`。本次开始时工作树干净。后续执行者必须重新检查 HEAD 和已有修改。此前 VT 改造已合并，不能沿用旧交接文档的未完成状态。

## 1. 目标与范围

将 ntclks 变成可独立获取、配置、编译、测试的新 Git 仓库，主仓库通过固定提交的 submodule 集成。分离源码所有权、构建依赖和运行时策略；同时整理文件职责。

完成标准：内核 checkout 不需要 LeonOS-4 源码即可构建；用户态只消费安装后的 UAPI 和用户态库；主仓库只通过公开构建入口取得内核制品；正常系统、Desktop、Installer 的既有行为保持可验证。

本次不升级 Linux ABI 基线、不改 syscall 编号及结构布局、不更换 libc、不重写 GUI/文件系统，不新增私有 TTY ABI。大型源码内部重写不与机械移动混在一起。本文的目录、构建接口和清单格式均为**拟议接口**。

## 2. 已核实的依赖

| 位置 | 当前事实 | 拆分影响 |
| --- | --- | --- |
| `mk/kernel.mk` | 同时编译 `kernel/ntclks`、`kernel/ostui`、`drivers/bootstrap`；storage 子文件被 facade 文本包含 | 只移动 ntclks 不构成独立内核；须保留 storage 编译方式，防止重复链接 |
| `mk/boot.mk` | 编译 loader、五个 `.drv` 和 kerneldebug；loader integrity 头依赖 kernel.sys | loader、模块与内核制品需要统一版本和依赖图 |
| `boot/loader/main.c` | 引用 `leonos/boot_handoff.h`、`leonos/psf_font.h` | 引导协议与早期显示依赖不能遗留在用户态目录 |
| `kernel/ntclks/user/userland.c` | 包含 ELF/进程启动、Desktop 等身份判断、autospawn 测试策略 | `user/` 是 Ring-0 实现；不能按名字整体搬入用户态 |
| `include/uapi/README.md` | 当前声明 Linux v6.12 native x86-64 wire ABI | 原样保持，不把头文件存在当作行为已兼容 |
| `include/leonos/` | 混合 runtime API、wire 类型、boot/driver 协议、布局与字体辅助实现 | 逐个声明分类，不能整目录公开或搬进内核 |
| `mk/runtime.mk`、`mk/sdk.mk` | runtime 直接读根 include；SDK 收集多处头文件 | 需要唯一源与安装导出过程，防止旧模板覆盖新头文件 |
| `tests/build/test-incremental.sh`、`tools/test_*.py` | 硬编码内核、驱动、UAPI 源路径 | 必须按测试职责迁移，保留增量测试的真实语义 |
| `docs/KERNEL_USERSPACE_BOUNDARIES.md` | 记录 kernel+loader 成对升级和 handoff 版本校验 | 保持 RPR 配对更新及失败回滚，不发布 kernel-only 更新 |

文档中的旧 `build.py` 命令不能直接当成当前生产入口。当前 `make test` 只聚合 test-tools/test-build，不代表所有 ABI 与 QEMU 测试通过。

## 3. 方案比较与推荐

| 方案 | 优点 | 问题 | 结论 |
| --- | --- | --- | --- |
| A：只把 `kernel/ntclks` 变成 submodule | 改动少 | 仍依赖主仓库驱动、头文件、配置和工具 | 不满足独立性 |
| B：内核仓库包含 Ring-0、loader、UAPI、独立构建；主仓库负责用户态和系统集成 | 边界完整，loader 配对自然，两个仓库即可维护 | 要整理混合头文件和构建接口 | **推荐** |
| C：内核、UAPI、loader、驱动分别建仓 | 发布粒度细 | 多仓版本矩阵、循环依赖和维护成本高 | 当前不采用 |

推荐新仓库逻辑名称 `ntclks`，主仓库挂载点 `kernel/ntclks/`。此名称只是建议；远端 URL、可见性、权限和历史保留方式由用户确认，不假定某个 GitHub 仓库已经存在。

## 4. 源码所有权与目录

### 4.1 主仓库目标

```text
LeonOS-4/
├── kernel/ntclks/          # gitlink：新内核仓库
├── userland/
│   ├── apps/              # 按组件保留，应用入口与私有文件同目录
│   ├── runtime/           # 原 userland/libc：LeonOS 扩展库，不是 musl
│   │   ├── include/leonos/# 用户态公开 API 的唯一手写源
│   │   └── src/
│   └── <ports>/           # musl/PAM/第三方适配，初次迁移保留名称
├── system/                # 服务、会话、rootfs 配置和发行版资源
├── boot/grub/             # 菜单、启动参数、介质策略
├── devtools/              # SDK 脚本、示例、文档；不维护 UAPI 镜像
├── configs/               # 发行版配置、组件清单、选用的内核配置
├── mk/                    # 主构建图及内核适配器
├── tools/{host,build}/    # 系统集成宿主工具及短脚本
├── tests/                 # 构建、用户态和整机测试
├── third_party/           # 用户态上游依赖
└── docs/                  # OS 架构、集成、用户态及执行文档
```

根 `Makefile` 仍是**完整 LeonOS 产品**的生产入口。内核仓库有自己的独立生产入口；同步修订 AGENT.md 对两个作用域的说明。

### 4.2 新内核仓库目标

```text
ntclks/
├── Makefile, Kconfig, configs/, mk/
├── arch/x86_64/            # 启动、体系结构、链接脚本
├── kernel/                # 核心、sched、exec、syscall、同步和信号
├── mm/                    # 内存管理、页缓存
├── fs/                    # VFS、文件系统；先保持 storage facade 边界
├── net/
├── drivers/               # 内建与可加载驱动，console/TTY/显示后端
├── debug/                 # kerneldebug
├── lib/                   # freestanding 内部辅助代码
├── boot/loader/           # loader 与完整性生成规则
├── include/
│   ├── ntclks/            # 内核私有，禁止用户态引用
│   ├── uapi/{linux,leonos}/ # 用户可见 wire ABI
│   ├── boot/              # loader ↔ kernel 协议
│   └── module/            # Ring-0 模块 API，独立于 UAPI
├── resources/             # 启动必需的最小字体及许可
├── tools/                 # 内核所需的 C 工具与短 shell 脚本
├── tests/                 # 内核宿主测试与最小 guest 探针
└── docs/                  # 内核内部、UAPI、boot/module 合约
```

这是最终职责布局，不要求一次移动所有文件。先建立自包含边界并通过构建，再逐组重排；保持可比较的移动清单。`kernel/ostui` 先归内核 console 内部实现，不因名字带 UI 而移至 Desktop。`kerneldebug` 和全部现有 Ring-0 驱动归内核。GRUB 二进制资源及镜像打包仍归主仓库。

### 4.3 移动映射与约束

| 当前 | 目标所有者/处理 |
| --- | --- |
| `kernel/ntclks/arch`、`mm`、`sched`、`user` | 内核 arch/mm/kernel；user 中机制放 exec，策略另拆 |
| `drivers/bootstrap/storage*` | 内核 fs 与存储驱动；第一阶段整体保留 facade，后续按真实依赖拆分 |
| `drivers/{mouse,serial,e1000,ac97,es1371}` | 内核 drivers，继续生成现有 `.drv` |
| `kernel/ostui`、`kernel/kerneldebug` | 内核 console/debug |
| `boot/loader`、`tools/build/loader-integrity.sh` | 内核 boot/tools；不得向上查找主仓库工具 |
| `include/uapi` | 内核唯一 UAPI 源，主仓库使用安装副本 |
| `include/leonos`、`include/linux`、libc/devtools 镜像 | 建立逐文件分类表；消除手工重复源 |
| `userland/libc` | 主仓库 `userland/runtime`，与 upstream musl 明确区分 |
| 工具、测试、配置、文档 | 随职责归属移动；整机镜像/QEMU 编排仍在主仓库 |

先用 `git ls-files` 区分跟踪源码、历史文件和本地生成物；不要把 `include/generated`、`out`、旧 `build` 缓存带进新仓库。

## 5. 头文件与 ABI 边界

1. **UAPI**：syscall 编号、ioctl、用户/内核共享结构、flags。内核维护、通过拟议 `headers_install` 导出。保留 Linux/LeonOS 命名空间、数字、宽度、对齐和布局。
2. **用户态 API**：`leonos_*()` 库函数、Desktop IPC 客户端和应用便利接口，唯一源在 runtime。需要 wire 类型时包含已安装 UAPI。
3. **内核私有**：调度器、存储内部对象、指针、实现函数只在私有 include。
4. **boot 与 module API**：分别导出给 loader/驱动开发工具，不能混进普通用户 SDK 的公共 API。保留当前 handoff 和驱动语义，暂不承诺跨版本稳定模块 ABI。
5. `auth.h`、`fs.h`、`system.h`、`device.h` 等混合头，按**声明**拆分。保留用户态旧包含名的薄转发头，但不得通过相对路径访问内核私有树。
6. `psf_font.h` 目前包含字体数据且依赖 `layout.h`；内核采用自身最小内置字体及解析器，用户态字体加载归 runtime。字体资产只保留一个权威来源，必要时由显式资源安装目标提供只读副本；不能伪装成 UAPI。保留资源许可证。
7. 通用 UTF-8 等 header-only 算法属于实现，不属于 UAPI。首次可将经过许可核对的实现分成各自负责的内部副本，记录来源与测试；避免为几段辅助代码再建共享 ABI 仓库。
8. SDK 从安装目录装配；删除模板中的重复 ABI 定义。对 headers_install 结果做白名单与自包含编译检查，防止宿主头或私有头泄漏。

## 6. 运行时职责分离

源码独立不要求内核脱离一切用户态 ABI；要求内核不依赖具体 Desktop 源码和组件构建图。

- 保留内核中的 ELF 装载、地址空间、exec/fork、凭据、fd、session、TTY、信号与设备访问检查。
- 内核只提供启动首个用户进程的机制及必要默认 init 路径；服务排序、Desktop/Installer 会话、测试 autospawn 移至现有 `gptinit`、`sessiond` 或相应测试服务。
- `layout.h` 中产品安装路径归主仓库；内核真正需要的 init/模块搜索默认值必须有独立内核默认配置，主仓库可通过有限且受校验的配置参数覆盖，不能导入整个产品头文件。
- `userland.c` 的 Desktop/windowd 身份及特权逻辑先建立调用者和权限矩阵。将设备操作权限与进程凭据/受控资源句柄关联；优先复用已经实现的 POSIX/Linux 机制。不能删除路径判断后默认放行，也不能新增“桌面专用特权 syscall”代替它。
- 若现有接口不足以安全替代某个身份判定，单列设计审查项与失败测试，保留现有检查直到替代验证完成；这种过渡状态不得宣称“运行时彻底解耦完成”。
- 当前 VT 架构必须保持：tty1～tty6、独立串口、前台进程组、图形会话退出恢复、系统日志隔离及 Installer 多 VT。

## 7. 构建、配置和制品合约

拟议内核入口：`make ARCH=x86_64 PROFILE=release O=<绝对路径> all`，以及 `headers_install`、`install`、`test`、`clean`。输出只写 O 和显式 DESTDIR；仅编译内核及 loader/驱动，不隐式构建完整 OS。独立测试不得要求主仓库 checkout；完整启动测试由主仓库提供 rootfs。

- 延续 GNU Make、clang/LLVM 和现有工具变量，HOSTCC 与目标 CC 分离；生产链不引入 Python/Meson/Ninja/Rust。现存 Python 回归可继续作为非生产测试入口。
- 内核拥有所需配置 schema、默认值、版本生成器与数据转换工具；不读取父目录 `configs/components.toml` 或父仓库 autoconf。
- 主仓库保留 BUILD/IMAGE/ENTRY/SDK/API 组件配置；把必要选项显式映射为独立内核配置，未知/冲突选项报错。installer/runtime 配置不能污染内核生成头。
- 父构建用 `$(MAKE) -C kernel/ntclks ...` 保留 jobserver。主仓库 `mk/kernel.mk` 变为适配器，不枚举/编译子仓 C 文件；`mk/boot.mk` 只消费安装制品。
- 子构建每次请求都可检查依赖图，内部按签名与 depfile 判断增量；不能仅以 gitlink SHA 为 stamp，否则漏掉子仓本地修改。子构建检查完成后父构建再读取导出清单，避免 parse-time 捕获旧文件列表。
- UAPI 安装与内核完整链接分开，runtime 不必等待 kernel.sys；实际 ABI 变化会重编译消费者，kernel 私有实现变化不应全量重编译用户态。
- 安装至 `O/kernel-export`，临时目录完成后发布清单/完成标记；清理已删除头和驱动，失败不留下可消费的半成品。
- 制品保留 `kernel.sys`、`kernel.debug`、`loader.elf`、五个 `.drv`、`kerneldebug.sys`；主仓库暂时维持既有镜像内安装路径和更新格式。
- 拟议 manifest 记录格式版本、arch、内核 Git SHA/dirty、工具链身份、配置摘要、UAPI 摘要、handoff 版本和每个制品 hash；分别记录主仓库与内核版本，不用父 SHA 冒充内核 SHA。
- SOURCE_DATE_EPOCH 显式传递；独立构建默认内核提交时间。构建不依赖当前时钟、绝对 checkout 路径或隐式联网；fetch 必须显式。
- RPR 继续校验并原子切换 kernel+loader 配对；模块按本次内核构建配套安装。测试更新失败与回滚路径。

## 8. Git 与发布流程

优先保留历史：在一次性 clone 中按批准的路径清单提取相关历史，再作归一化提交。先拆混合头再导出，确认 license/作者信息完整。只在隔离副本做历史过滤，绝不重写 LeonOS-4 历史。若选择新仓库初始快照，需用户明确选择，并保留来源提交和许可证。

挂载点和旧 `kernel/ntclks` 同名，不能直接在非空已跟踪目录上执行 submodule add。先在独立临时工作目录验证新仓库，再在主仓库的受控变更中移除旧跟踪文件、建立 gitlink 和 `.gitmodules`；禁止以删除整个工作目录的方式处理已有修改。

发布顺序：子仓验证 → 用户授权的子仓 commit/push → 确认 SHA 可从远端获取 → 父仓 gitlink 更新 → 用户授权的父仓 commit/push → 全新递归 clone 验证。不得提交本机绝对路径 URL、不可达 SHA 或仅存在于本机的对象。开发构建可使用 dirty 子仓，发布构建必须拒绝 dirty/未初始化状态。

**本轮仅交付设计文档，不创建新仓库、不建分支、不提交或推送。** 实施前批准设计；发布前提供实际远端。遵守当时的 AGENT.md 和明确用户授权。

## 9. 分阶段迁移与验证

| 阶段 | 交付物 | 必须通过的关卡 |
| --- | --- | --- |
| 0 基线 | 来源→目标→消费者→测试清单、当前构建/运行证据 | 明确原有失败；不把历史文档当新测试结果 |
| 1 ABI 边界 | 逐声明分类、唯一 UAPI 源与导出测试 | UAPI/musl ABI 布局、用户 SDK 自包含编译、无私有头泄漏 |
| 2 独立内核 | Ring-0、loader、工具、配置、资源完整 checkout | 无父目录环境下独立构建、host 测试、制品完整性 |
| 3 父构建集成 | Make 适配器、sysroot/SDK、镜像/更新路径 | 完整 all、增量/并行/旧 O、制品删除、离线与可重现性 |
| 4 目录与策略 | runtime 命名整理、内核职责目录、用户态启动策略 | 每组机械移动单测；权限负例；autospawn/会话回归 |
| 5 真 submodule | 可达远端 SHA、gitlink、递归 CI、文档 | 全新 clone --recurse-submodules 完整构建、缺子仓友好报错 |
| 6 验收 | 整机证据与 review 记录 | live/installed/installer QEMU；VT/Desktop/日志/退出恢复；更新回滚 |

阶段 2 可以在本地独立目录验证；在远端未确定时明确停在阶段 5 的发布动作，不能声称完成真实 submodule 交付。

重点回归入口：`tools/test_uapi.py`、`tools/test_musl_abi.py`、`tools/test_console_boot_policy.py`、`tools/test_installer_input.py`、`tools/test_vt_qemu.py`、`tools/test_vt_installed_qemu.py`、`tools/test_vt_installer_qemu.py`。执行前读实际参数与 fixture 要求，不能假定无参数可运行。内核单位测试迁入子仓；上述整机测试留父仓并消费明确的产物路径。

最终验收还要覆盖：修改/删除内核源、私有头、UAPI、linker script、配置与工具链；确认正确重建范围；用户态仅用导出 sysroot 编译；新旧 ABI 探针对比；真实 Ctrl+Alt+F1～F6、TTY stdin/out/err、setsid/TIOCSCTTY/tcsetpgrp、无 Desktop 和 Installer 退出恢复。QEMU 使用独立测试磁盘，不改用户镜像。

## 10. 待确认与设计自检

建议默认：方案 B；挂载点 `kernel/ntclks`；loader 和 Ring-0 驱动随内核；保留历史；内核与 OS 的独立版本；分阶段保留当前外部行为。需确认新仓库 URL/可见性及历史策略。细粒度权限替代需要实现前专项审查，不能凭目录设计推断安全性。

自检：已覆盖反向源码依赖、混合头与字体资源、配置生成、内核/loader 配对、SDK 镜像、权限策略、增量构建、同名路径转 gitlink、历史与许可证、递归 CI、VT/Installer 回归。此自检只评估设计完整性，不代表代码通过验证。

执行提示词与阶段清单见 [配套文档](../plans/2026-09-24-ntclks-separation-agent-prompt.md)。
