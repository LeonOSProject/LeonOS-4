# ReliefOS / ReliefNT 全项目品牌迁移实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 subagent-driven-development 或 executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）跟踪进度。本仓 `AGENT.md` 禁止未经用户明确要求自行创建分支或 commit，因此每个任务以可审查的 diff 和测试记录收束；只有用户另行授权时才按任务提交。

**目标：** 把系统规范名迁为 ReliefOS、内核规范名迁为 ReliefNT，并保持旧安装、旧源码和旧已编译应用在过渡期可用。

**架构：** 先固定新旧命名契约和扫描门禁，再处理独立内核/UAPI、主仓构建与运行库，随后迁移来宾路径、启动/安装、包和发布，最后覆盖界面、文档与外部服务。旧名仅在兼容、历史和真实旧地址中保留。

**技术栈：** GNU Make 4.3+、C/C++/汇编、POSIX Shell、Kconfig、musl/ELF、OpenRC、APK、GRUB/UEFI、QEMU/VMware、静态站点与 Git 子模块。

**规格：** `docs/superpowers/specs/2026-09-27-reliefos-reliefnt-rename-design.md`。执行者需同时阅读该规格与根 `AGENT.md`，并在每项工作前检查真实源码、构建图和 `git diff`。

## 全局约束

- 正式系统显示名严格为 `ReliefOS`，不拼 `4`；内核显示名为 `ReliefNT`。版本字段继续保留现有数值语义。
- 全面迁移规范技术标识；旧路径、头、ELF 符号、包与安装数据在过渡期兼容，不得覆盖或删除用户配置。
- 子仓先产生可引用提交，主仓再更新 gitlink；`kernel.sys`、`loader.elf`、驱动 `.drv`、syscall 编号、UAPI 布局、`LEONACL.SYS` 不随品牌改名。
- 生产构建继续由根 Makefile 驱动，只编译 C/汇编；不引入 Python、Meson、Ninja 生产调度器。`make fetch` 仍是唯一显式下载阶段。
- 用户未跟踪的主仓和子仓 `max_size = 50G/`、现有 `out/`/缓存/镜像、第三方子仓都不可清理或批量替换。
- 执行以 `docs/superpowers/specs/2026-09-27-reliefos-reliefnt-rename-inventory.tsv` 的 1,142 条逐文件记录为覆盖基线；每个旧名命中必须在最终复审中归入新规范、兼容、历史/归属或真实旧 URL，不以某一类文件的抽样替代全量复核。
- 外部 GitHub/Pages/SourceHut 地址及 CI Secret 只有在实际新目标准备好后切换；未准备好时保留旧 URL 并把它列为兼容残留。
- 每个修改任务的末尾运行 `git diff --check`，记录“源码、编译、打包、虚拟机运行”四层中实际完成的层级；不得把前一层当后一层。
- 运行 `tests/build/test-submodule-contract.sh` 等会在子仓写探针的测试，必须先在干净隔离克隆验证。当前真实子仓有用户未跟踪内容，不能让测试假设其 clean。

## 文件结构与职责

| 范围 | 主要文件 | 负责的契约 |
| --- | --- | --- |
| 内核源与 UAPI | `kernel/ntclks/Makefile`、`kernel/ntclks/kernel/ntclks/`、`kernel/ntclks/include/ntclks/`、`kernel/ntclks/include/uapi/leonos/` | ReliefNT 工程名、私有 API、系统信息和新旧 UAPI 头 |
| 子模块与主仓适配器 | `.gitmodules`、`mk/kernel.mk`、`mk/headers.mk`、`tools/build/ntclks-release-guard.sh`、`tests/build/test-kernel-adapter.sh`、`tests/build/test-submodule-contract.sh` | gitlink、子构建、头导出、clean/release 守卫 |
| 构建配置与工具 | `Makefile`、`Kconfig`、`Kconfig.components`、`configs/{components.toml,default.conf,profiles/default.conf}`、`tools/host/manifest/leonos-components.c`、`mk/*.mk` | 新变量/配置符号与旧配置导入、产物依赖图 |
| 公开 API / 运行库 / SDK | `include/leonos/`、`userland/runtime/include/leonos/`、`userland/runtime/src/`、`configs/header-export.list`、`mk/{runtime,sdk}.mk`、`tools/build/musl-sdk.sh` | 新头/库、旧源码与旧 ELF 兼容 |
| 来宾布局与服务 | `include/leonos/layout.h`、`tools/host/manifest/leonos-layout.c`、`tools/build/rootfs-stage.sh`、`system/rootfs/`、`userland/storage/` | 新旧目录、OpenRC 服务、用户数据迁移 |
| 启动与安装 | `boot/grub/`、`mk/images.mk`、`tools/build/{efi-stage,installer-stage,iso,disk}.sh`、`userland/apps/installer/`、`userland/storage/leonos-{grub-installer,kernel-update}` | GRUB、ESP、ISO/VMDK、安装与回滚 |
| APK / RPR | `configs/apk-ownership.json`、`mk/{apk,rpr}.mk`、`tools/build/{apk-stage,rpr-apps,site,verify-pages}.sh`、`tests/integration/test-apk-upgrade.sh` | 包名、依赖、签名、仓库与升级事务 |
| 可见品牌与发布 | `system/rootfs/etc/os-release`、`README.md`、`NOTICE`、`AGENT.md`、`logo.png`、`configs/nls/po/`、`userland/apps/`、`resources/pages/`、`.github/workflows/`、`docs/` | 文案、图像、翻译、网站、CI 和使用说明 |

表中旧路径表示**执行时要定位的当前路径**，不能从表中推断最终会保留旧文件名。每一组 `git mv` 都要同步所有生产端、消费端和测试引用。

---

### 任务 1：固定基线、词表和旧名审计门禁

**文件：** 创建 `tests/build/test-brand-identity.sh`；修改 `mk/tests.mk`（确保纳入 `test-build`）；创建 `docs/branding-compatibility.md`。

- [x] **步骤 1：在 `docs/branding-compatibility.md` 固定迁移词表。** 明确 `LeonOS 4 → ReliefOS`、`LeonOS → ReliefOS`、`NTCLKS/ntclks → ReliefNT/reliefnt` 的展示规则；列出 `LeonMMcoset`、`LEONACL.SYS`、旧 URL、历史材料和旧 ABI 作为例外。写出新旧路径、库、包、配置符号矩阵，并声明新旧契约的单一规范来源。
- [x] **步骤 2：先编写会失败的品牌测试。** 在 `tests/build/test-brand-identity.sh` 使用下面的起始断言；同时查路径名、`os-release` 和 GRUB 菜单，记录每项失败。

  ```sh
  #!/bin/sh
  set -eu
  root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
  grep -qx 'NAME="ReliefOS"' "$root/system/rootfs/etc/os-release"
  grep -qx 'ID=reliefos' "$root/system/rootfs/etc/os-release"
  grep -q 'menuentry "ReliefOS"' "$root/boot/grub/grub.cfg"
  test -f "$root/include/reliefos/layout.h"
  ```

- [x] **步骤 3：确认红灯。** 运行 `sh tests/build/test-brand-identity.sh`，预期至少 `NAME` 断言失败；现阶段不要通过弱化断言取得绿灯。
- [x] **步骤 4：把品牌测试分层扩充。** 外观断言、路径/服务断言、头/库断言、产物名断言分别调用脚本自身的子函数；旧名命中按 `compatibility`, `history`, `external-url` 的逐文件允许清单核对，而不是一条全局 `grep -q`。任意新增非允许旧名必须失败。
- [x] **步骤 5：保存基线证据。** 记录 `git status --short --branch`、主仓 `git grep -I -i -n -E 'leonos|ntclks'`、子仓对应搜索、`git ls-files` 文件名搜索和 `git diff --check`。此时品牌测试保持红灯是预期；后续任务逐项变绿。

### 任务 2：ReliefNT 子仓身份、私有命名与 UAPI 双路径

**文件：** 修改子仓 `README.md`、`NOTICE`、`Makefile`、`.github/workflows/build.yml`、`kernel/ntclks/version.c`、`kernel/ntclks/uts.c`；迁移子仓 `kernel/ntclks/`、`include/ntclks/`、`include/uapi/leonos/`；新增兼容头 `include/ntclks/` 与 `include/uapi/leonos/`；更新子仓构建/导出测试。

- [x] **步骤 1：先在子仓隔离环境跑原有 `make help`、`make test`、`make headers_install`，记录通过/失败和产品 manifest。** 子仓的真实未跟踪目录不清理；验证可以使用独立克隆与独立 `O`，不可把子仓当前状态假称干净。
- [x] **步骤 2：先写失败的 UAPI 兼容测试。** 测试同时包含 `<reliefos/system_abi.h>` 和 `<leonos/system_abi.h>`；用 `_Static_assert` 比较关键结构 `sizeof`/`offsetof`，比较 syscall/ioctl 常量；测试新 `<reliefnt/version.h>` 与旧 `<ntclks/version.h>` 同时可编译。

  ```c
  #include <stddef.h>
  #include <reliefos/system_abi.h>
  #include <leonos/system_abi.h>
  #include <reliefnt/version.h>
  #include <ntclks/version.h>
  _Static_assert(sizeof(struct reliefos_system_info) ==
                 sizeof(struct leonos_system_info), "system info ABI changed");
  ```

- [x] **步骤 3：运行新测试确认失败。** 预期缺 `<reliefos/system_abi.h>` 或 `<reliefnt/version.h>`，而不是测试本身语法错误。
- [x] **步骤 4：迁移内核私有源与头目录、`NTCLKS_*` 私有宏、`ntclks_*` 私有函数和内部 include。** 更新 Make 源清单与所有引用；旧 `<ntclks/...>` 头仅转发到新头，不复制实现。对公开 UAPI 的旧结构/常量建立别名；不改数字值或字段布局。
- [x] **步骤 5：把子仓展示信息改为 ReliefNT，内核编译生成信息区分内核名与系统名。** 内核私有日志前缀改为 `[reliefnt]`；外部系统日志解析脚本和测试同步更新。`kernel.sys`、`loader.elf` 等产品清单不变。
- [x] **步骤 6：验证。** 在子仓执行 `make test`、`make -j8 all`、`make headers_install`；用 `find <O>/kernel-export/include -type f` 确认新旧 UAPI 均在，运行新兼容测试。检查 `git -C kernel/ntclks diff --check`。形成供主仓 gitlink 引用的子仓提交由用户授权的提交步骤完成；不得让主仓指向未提交对象。

### 任务 3：主仓内核适配器、头导出与 gitlink 迁移

**文件：** 修改 `.gitmodules`、`mk/kernel.mk`、`mk/headers.mk`、`mk/rpr.mk`、`tools/build/ntclks-release-guard.sh`（改名为 `reliefnt-release-guard.sh`）、`configs/header-export.list`、`tests/build/test-{kernel-adapter,header-boundary,ntclks-fetch,submodule-contract,submodule-rollback-build}.sh`、`tools/test_header_export.py`。

- [x] **步骤 1：先把现有子仓 gitlink、子仓 HEAD 与用户未跟踪内容记入测试证据。** 不在真实工作区直接 `git mv kernel/ntclks kernel/reliefnt`；先用本地隔离克隆验证 gitlink 路径变更、旧路径消失、新路径初始化和用户目录不被触碰。
- [x] **步骤 2：先改/新增适配器契约测试为红灯。** 测试新 `RELIEFNT_DIR` / `RELIEFNT_O` 覆盖，旧 `NTCLKS_DIR` / `NTCLKS_O` 仍可传入；同传时新名优先；无子仓时报 `kernel/reliefnt` 引导信息；产品 manifest 仍是九件原名；release guard 拒绝脏子仓及 SHA 不匹配。

  ```sh
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' EXIT
  make O="$tmp/out" defconfig
  make -n O="$tmp/out" RELIEFNT_DIR="$PWD/kernel/reliefnt" \
    NTCLKS_DIR="$tmp/no-such-kernel" kernel > "$tmp/new.log"
  make -n O="$tmp/out" NTCLKS_DIR="$PWD/kernel/reliefnt" \
    kernel > "$tmp/old.log"
  grep -Fq "$PWD/kernel/reliefnt" "$tmp/new.log"
  grep -Fq "$PWD/kernel/reliefnt" "$tmp/old.log"
  ```

- [x] **步骤 3：更新 `mk/kernel.mk` 与 `mk/headers.mk` 的规范变量、错误文字、调用路径和导出白名单。** 旧环境变量仅为输入兼容，不再出现在新输出 manifest。对子仓 `headers_install` 同时导出 `reliefos/` 与兼容 `leonos/`。
- [x] **步骤 4：迁移 gitlink 路径。** 在隔离验证通过、真实子仓未跟踪内容得到保全且子仓提交可 fetch 后，更新 `.gitmodules` 的 path；新远端未存在则保留现有可用 URL。`git submodule sync` 和递归 clone 验证后再把路径更改带回真实分支。只 stage gitlink，不把子仓内部源码当主仓普通文件。
- [x] **步骤 5：运行定向测试。** `sh tests/build/test-kernel-adapter.sh`、`sh tests/build/test-header-boundary.sh`、`python3 tools/test_header_export.py` 和 `make kernel`。涉及写探针/克隆的子模块测试只在隔离干净克隆运行，随后核对 `git diff --check` 与 `git status --short`。

  > 完成证据（2026-09-28）：旧 SHA `5cc9621` 上先运行递归子模块初始化与 `make fetch`，隔离回滚构建产出九个原名内核产品；`rpr-pages` 在 gitlink 与 HEAD 不一致时拒绝发布，之后恢复到子仓提交 `296acc8611fabf76b2ae7bb1169396ce8c17735b` 且干净。隔离 `test-submodule-contract.sh` 11 项通过。主仓 `test-kernel-adapter.sh` 35 项、`test-header-boundary.sh` 5 项、`test_header_export.py`（82 个头）、`test-ntclks-fetch.sh` 12 项全部通过；主仓 `make kernel` 通过并安装九个原名产品。递归克隆已验证新路径 `kernel/reliefnt` 和嵌套 kconfig 子模块，`.gitmodules` 保留原 URL。头边界验证发现并修复了旧认证 UAPI 头的传递包含兼容；子仓修复提交为 `296acc8611fabf76b2ae7bb1169396ce8c17735b`。

### 任务 4：Kconfig、主机工具与构建命名

**文件：** 新增 `tools/build/reliefos-config-migrate.sh`；修改 `Makefile`、`Kconfig`、`configs/components.toml`、`configs/default.conf`、`configs/profiles/default.conf`、`tools/host/manifest/reliefos-components.c`、`tools/host/config/reliefos-config.c`、`mk/{host,config,tests}.mk` 及所有引用这些路径的 `mk/*.mk`；重生成 `Kconfig.components`；更新 `tests/build/test-{components,components-metadata,config-first-build,config-groups,incremental}.sh`。

- [x] **步骤 1：先写旧 `.config` 导入 fixture 与新配置生成测试。** 同一个组件旧 `CONFIG_LEON_COMPONENT_APP_HELLO_BUILD=y` 应转换为 `CONFIG_RELIEFOS_COMPONENT_APP_HELLO_BUILD=y`；新配置中不得生成双份互相冲突的选择。已存在的 `O/config/.config` 仅在显式迁移命令下改写，并保留备份。

  ```sh
  tmp=$(mktemp -d)
  trap 'rm -rf "$tmp"' EXIT
  printf '%s\n' 'CONFIG_LEON_COMPONENT_APP_HELLO_BUILD=y' > "$tmp/old.config"
  sh tools/build/reliefos-config-migrate.sh "$tmp/old.config" "$tmp/new.config"
  grep -qx 'CONFIG_RELIEFOS_COMPONENT_APP_HELLO_BUILD=y' "$tmp/new.config"
  ```

- [x] **步骤 2：确认 fixture 红灯。** 预期缺迁移脚本或新 Kconfig 符号；保持旧配置 fixture 只读。
- [x] **步骤 3：实现迁移脚本和生成器规则。** `configs/components.toml` 为唯一组件来源；运行现有组件生成入口重生成 `Kconfig.components`，同步 `configs/default.conf` 和 profile；检查 `select/depends on` 对应关系无悬空旧符号。
- [x] **步骤 4：迁移主仓私有 `LEONOS_*`/`leonos-*` 变量、host 工具文件名和输出 marker。** 对用户可设置的 `LEONOS_*` 环境变量做输入别名，并测试新名优先；`clean/distclean` 必须识别旧/新所有权 marker 且拒绝未知目录、symlink、源码根。
- [ ] **步骤 5：验证 `make help`、`make doctor`、`make defconfig`、`make menuconfig`、`make test` 与 `make -n userland`，用不同 `O` 对旧配置迁移和新配置增量构建各跑一次。** `make -n/-q` 不应写配置或抢锁；检查 `git diff --check`。

> 执行记录（2026-09-28）：旧 `.config` 迁移 fixture、组件解析器/metadata、first-build、config-groups、userland graph、SDK driver、镜像适配、版本 ABI、Pages/RPR 与 clean/lock 测试均通过；`make regen-component-kconfig` 输出与已跟踪 `Kconfig.components` 一致。`make help`、`make doctor`、`make defconfig`、`make menuconfig`、新 `O` 的 `make -n/-q userland` 检查均已运行；不同 `O` 上的 `make tools` 连续两次运行，第二次无构建动作。完整 `make test` 的 host 与 ASan/UBSan 测试、auth stage、bootstrap（22 项）通过，随后 `test-brand-identity.sh` 在任务 5/6/7 尚未实施的公开头、来宾路径/服务、库、镜像与 SDK 命名断言处失败；门禁原样保留，步骤 5 待所有后续任务完成后全量重跑。

### 任务 5：公开 API、运行库与 SDK 的源码/二进制兼容

**文件：** 新增 `include/reliefos/`、`userland/runtime/include/reliefos/`、`tests/integration/brand-abi.sh`、`tests/fixtures/brand-abi/{old_api,new_api}.c`；修改原 `include/leonos/` 与 `userland/runtime/include/leonos/` 为兼容层；修改 `userland/runtime/src/`、`mk/{runtime,sdk}.mk`、`tools/build/musl-sdk.sh`、`configs/header-export.list`、`tests/integration/sdk-link.sh`、`tests/build/test-sdk-{driver,stage}.sh`。

- [x] **步骤 1：先把旧 SDK 的导出函数与 `libleonos.so.2` 的 `DT_SONAME`/符号表作为 ABI 基线保存到测试 fixture。** 新测试用旧头/旧 SDK 构建一个小 ELF，再用新镜像的兼容库加载；另用 `<reliefos/api.h>` 和 `reliefos-musl-cc` 构建新 ELF，确保 `DT_NEEDED` 为 `libreliefos.so.2`。

  ```sh
  # 以下断言写入 tests/integration/brand-abi.sh；work 是该脚本创建的临时目录。
  old_elf="$work/old.elf"
  new_elf="$work/new.elf"
  compat_lib="$work/root/usr/lib/leonos/libleonos.so.2"
  new_lib="$work/root/usr/lib/reliefos/libreliefos.so.2"
  readelf -d "$old_elf" | grep -q 'libleonos.so.2'
  readelf -d "$new_elf" | grep -q 'libreliefos.so.2'
  nm -D "$compat_lib" | grep -q ' leonos_'
  nm -D "$new_lib" | grep -q ' reliefos_'
  ```

- [x] **步骤 2：运行测试确认新头/新编译驱动缺失而失败。** 不用仅检查文件存在的测试代替实际编译链接。
- [x] **步骤 3：以新头为唯一声明源，为旧头生成/维护转发兼容层。** 一组旧与新结构/宏的 `_Static_assert(sizeof/offsetof/number)` 测试覆盖公开 ABI；针对 `leonos_*` 导出函数建立显式对应表。旧结构 tag 可在兼容头通过宏别名映射到同布局的新结构，不能形成两份独立布局定义。

  ```c
  /* 仅示意 include/leonos/system.h 的兼容映射模式。 */
  #include <reliefos/system.h>
  #define leonos_system_info reliefos_system_info
  #define LEONOS_SYSTEM_NAME_LEN RELIEFOS_SYSTEM_NAME_LEN
  ```

- [x] **步骤 4：更新 `userland/runtime/src/` 中公开函数的规范名字，并在相同翻译单元为每个已发布旧导出提供真实 ELF alias 或等价包装。** 用任务 5 步骤 1 的符号基线逐项比对，不遗漏数据导出、弱符号和版本；然后把 `userland/apps/`、主仓 `tools/`、测试程序、内核子仓消费者逐组切换到新头和新调用名。`third_party/` 上游源码不批量替换，只改本仓适配层。每组完成后先编译对应应用，最后用旧 SDK fixture 验证仍能链接。
- [x] **步骤 5：更新共享/静态运行库和 SDK 装配。** 从同一对象集分别链接 `libreliefos.so.2` 与 `libleonos.so.2`，显式设置各自 SONAME；新 ELF 链到 `libreliefos.so.2`，旧 ELF 继续加载旧库。旧 ABI 库和新 ABI 库的依赖、安装路径、许可证、归档内容同步纳入 SDK 与 rootfs manifest。
- [x] **步骤 6：验证 `make runtime sdk`、`sh tests/integration/sdk-link.sh`、`sh tests/build/test-sdk-driver.sh`、`sh tests/build/test-sdk-stage.sh`、`python3 tools/test_header_export.py`；用 `readelf/nm` 比较新旧符号与 SONAME。** 旧 ELF 的最终加载还需在任务 11 的 QEMU 中证明；检查 `git diff --check`。

> 执行记录（2026-09-28）：旧 SDK ABI fixture 的初次编译链接测试先因新头缺失红灯；记录了 `libleonos.so.2` 的旧 SONAME、1,466 项动态符号/类型表与 416 个旧 SDK 导出。实现后 `make runtime sdk`、`make userland`、`make tools`、`make test-tools`、旧/新 ABI ELF 与 PortableGL 双名导出检查、SDK 动态/静态链接、SDK driver/stage、82 头文件导出、rootfs/安装器 stage 和 APK 所有权测试均通过。主系统及安装器的旧兼容库均经 `readelf/nm` 验证 SONAME 与 1,466 项旧动态符号/类型表一致。构建发现并修复了 curses 结构 tag 不一致、PortableGL 源路径/旧符号 alias 缺失，以及适配未修改 Doom 上游宏的编译条件。`test-brand-identity.sh` 的主仓/子仓旧名命中审计通过；尚余断言对应任务 6/7/9 的来宾路径、可见产品名和镜像名，另有 1,121 条 `migration` 清单待任务 11 最终复审。`git diff --check` 与已暂存 diff 检查通过；旧 ELF 在 QEMU 的实际加载按计划留到任务 11 验证。

### 任务 6：来宾目录、OpenRC 服务与已有配置迁移

**文件：** 新增 `include/reliefos/layout.h`、`system/rootfs/usr/lib/reliefos/reliefos-migrate`；修改 `include/leonos/layout.h`、`tools/host/manifest/leonos-layout.c`、`tools/build/{rootfs-stage,standalone-root,installer-stage}.sh`、`configs/apk-ownership.json`、`system/rootfs/etc/leonos/`、`system/rootfs/etc/init.d/leonos-*`、`system/rootfs/etc/runlevels/*/leonos-*`、`userland/runtime/src/` 和使用旧路径的 `userland/apps/`；增加 `tests/build/test-reliefos-migration.sh`。

- [ ] **步骤 1：先写迁移 fixture。** 建立旧 `/etc/leonos/display.conf`、旧 `/var/lib/leonos/users.db` 和一个已存在的新配置；运行迁移后要求旧数据保留、缺失的新文件得到拷贝、已有新文件原样不动、再运行一次结果相同；模拟不可写目标时要求非零返回与明确日志。

  ```sh
  fixture=$(mktemp -d)
  trap 'rm -rf "$fixture"' EXIT
  mkdir -p "$fixture/etc/leonos" "$fixture/etc/reliefos"
  printf 'theme=win95\n' > "$fixture/etc/leonos/display.conf"
  printf 'theme=metro\n' > "$fixture/etc/reliefos/display.conf"
  sh system/rootfs/usr/lib/reliefos/reliefos-migrate "$fixture"
  grep -qx 'theme=metro' "$fixture/etc/reliefos/display.conf"
  grep -qx 'theme=win95' "$fixture/etc/leonos/display.conf"
  ```

- [ ] **步骤 2：运行 fixture 确认缺迁移器失败。** 再实现有边界的复制/校验/日志逻辑；迁移只针对明确列出的配置与数据文件，遇到冲突记录而不覆盖。注意短读短写、权限、目录所有者及重复执行。
- [ ] **步骤 3：切换布局常量和 staging。** 新系统只把新目录作为规范写入；旧目录提供兼容读取/入口。更新服务文件名、runlevel 链接、脚本调用及 `pam.d` 名；同一服务只能启动一次，安装器/普通系统两个 root 都要检查。
- [ ] **步骤 4：验证 `sh tests/build/test-rootfs-stage.sh`、`sh tests/build/test-installer-stage.sh`、`sh tests/build/test-reliefos-migration.sh`、相关 OpenRC Python 回归与 `make rootfs`。** 检查 stage manifest 的新旧路径及文件所有权；旧数据库完整性用哈希比对；检查 `git diff --check`。

### 任务 7：启动介质、安装器与内核更新回滚

**文件：** 修改 `boot/grub/{grub,installer,live,installer_embedded}.cfg`、`boot/grub/theme/theme.txt`、`mk/images.mk`、`tools/build/{efi-stage,iso,disk,installer-stage,site}.sh`、`userland/apps/installer/{main.c,installer_setup.c}`、`userland/storage/leonos-{grub-installer,kernel-update}`、`tests/build/test-image-adapters.sh`、`tests/integration/test-kernel-update.sh`、`tools/test_installer_update_qemu.py`。

- [ ] **步骤 1：先写启动布局与旧盘升级测试。** 新 ESP 中 `/reliefos/kernel.sys` 与 GRUB 的 `module2` 参数同路径；旧盘 `/leonos/kernel.sys` 保持可启动；新安装 ISO marker、下载页名称和 installer 检测保持一致。模拟升级发布中断，确认旧 GRUB 项/载荷仍在。

  ```sh
  grep -q 'module2 /reliefos/kernel.sys' boot/grub/installer.cfg
  grep -q '/reliefos/kernel.sys' tools/build/efi-stage.sh
  test -s out/x86_64/release/images/reliefos-installer.iso
  ```

- [ ] **步骤 2：确认测试红灯。** 当前 GRUB 与镜像规则仍用旧路径/产物名，失败应明确指向这些约定。
- [ ] **步骤 3：同时切 `efi-stage.sh`、GRUB 三种入口、Installer staging、disk/ISO 目标和安装器/更新器。** 保留 `EFI/BOOT/BOOTX64.EFI`、`kernel.sys`、`loader.elf`；新旧载荷成套验证后才发布新 GRUB 配置。升级模式先读旧布局，写新布局并保留回滚入口。
- [ ] **步骤 4：检查卷标长度与工具限制。** FAT 卷标不能直接使用超限的 `RELIEFOS4ESP`；为 ESP 采用 `RELIEFOS`（8 字符），GPT 分区名采用 `RELIEFOS_ESP` / `RELIEFOS_ROOT`，并更新测试断言。已发布分区 UUID 保持不变。
- [ ] **步骤 5：验证 `sh tests/build/test-image-adapters.sh`、`sh tests/integration/test-kernel-update.sh`、`make image-vmdk iso installer`。** 以 `mtools`/ISO 清单逐项检查载荷与 GRUB 路径；QEMU 新安装和旧盘升级在任务 11 执行；检查 `git diff --check`。

### 任务 8：APK 包名、仓库、SDK 包和签名链

**文件：** 修改 `configs/apk-ownership.json`、`mk/{apk,rpr,sdk}.mk`、`tools/build/{apk-stage,rpr-apps,rpr-config,verify-pages}.sh`、`userland/storage/leonos-{apk-update,check-update,rpr-apkcheck,rpr-ping}`、`tests/build/test-{apk-ownership,rpr-packages,sdk-stage}.sh`、`tests/integration/test-apk-upgrade.sh`。

- [ ] **步骤 1：先在隔离 root fixture 安装旧 `leonos-*` 包，写升级测试断言：新 `reliefos-*` 包真实接管文件，旧包数据库状态一致，现有配置不丢，签名校验仍启用。** 用当前 `configs/apk-ownership.json` 的 `forbidden_shortcuts` 列表作为反作弊检查。
- [ ] **步骤 2：运行测试确认新包缺失。** `apk` 的真实数据库与依赖解算必须参与，不用自写假的 installed database。
- [ ] **步骤 3：迁移所有权组、包名、依赖、`provides/replaces` 的真实升级关系、命令入口和 SDK 包名。** 过渡包只在确实必要时保留，并明确版本排序；不得靠假 `provides`、关闭签名/TLS 或强制覆盖绕过冲突。旧 RPR 公钥的信任处理要在新的仓库配置中明确。
- [ ] **步骤 4：验证 `sh tests/build/test-apk-ownership.sh`、`sh tests/build/test-rpr-packages.sh`、`sh tests/integration/test-apk-upgrade.sh`、`make apk-repo rpr-pages sdk`。** 对 APKINFO/数据库/签名/清单逐项检查，核对新 SDK 归档名和内容，运行 `git diff --check`。

### 任务 9：系统可见品牌、翻译、帮助与图形资产

**文件：** 修改 `system/rootfs/etc/os-release`、`system/rootfs/etc/motd*`、`system/docs/leonos.hlp`、`system/docs/zh_CN/leonos.hlp`、`userland/apps/{installer,login,desktop,settings,osver,fastfetch}/`、`userland/fastfetch/leonos-ascii.txt`、`configs/nls/po/{leonos.pot,zh_CN.po}`、`boot/grub/theme/theme.txt`，并检查 `logo.png` 及相关 `resources/build-art/app-icons/` 的使用位置。

- [ ] **步骤 1：先扩展任务 1 的测试，检查 `os-release` 三字段、GRUB/安装器/登录/桌面/版本页、MOTD、Fastfetch 和中英文帮助的用户可见产品名均为 ReliefOS；内核信息使用 ReliefNT。** 不把 `VERSION_ID=4` 误判成显示后缀。
- [ ] **步骤 2：确认 UI 字符串测试红灯，再逐入口修改源文案与翻译模板。** 更改 `msgid` 时同步 `zh_CN.po`，处理格式参数与长度；旧历史语料不改事实。根 `logo.png` 目前是 740×740 无文字蓝色图形，继续作为 ReliefOS 图标；地球壁纸也沿用，不修改 NASA 来源署名。核验图标、GRUB、站点和镜像 staging 中没有烘焙的旧品牌文字。
- [ ] **步骤 3：构建受影响应用与图形镜像。** `make app-installer app-login app-desktop app-settings app-osver rootfs image-vmdk`；在 QEMU 分别查看 GRUB、安装器、登录和桌面，并截取证据。壁纸是 1280×720 BMP，遵守现有解码上限与来源署名；检查 `git diff --check`。

### 任务 10：仓库文档、站点、CI 与对外地址切换

**文件：** 修改 `README.md`、`AGENT.md`、`NOTICE`、`docs/{README,RPR,BUILDSYSTEM,BUILD_AND_INSTALLER,ABI,ROOTFS_LAYOUT_AND_MIGRATION}.md`、`resources/pages/css/leonos.css`、`tools/build/{home-page,download-page,site,verify-pages}.sh`、`.github/workflows/{build-installer,build-pages,code-count,sourcehut-sync}.yml`、`.github/ISSUE_TEMPLATE/Bug-Report.yml`、`.gitmodules`。

- [ ] **步骤 1：先写站点与 CI 的新产物路径断言。** 首页标题为 ReliefOS，内核页标题为 ReliefNT，下载页指向真实 `reliefos-installer.iso`，`SHA256SUMS` 文件名和下载物匹配；CI 验证/上传同一个新路径。运行 `sh tests/build/test-pages.sh` 应先在旧布局上失败。
- [ ] **步骤 2：更新当前技术文档和仓库入口。** `README.md` 可用相对路径 `logo.png`，避免旧 GitHub 图片 URL；`NOTICE` 保留第三方归属与 Apache-2.0 根 `LICENSE` 原文。`BY_AUTHOR.md`、旧安全审计、旧设计规格按历史资料保留原名称，增加迁移索引即可。
- [ ] **步骤 3：更新站点模板、CSS 文件引用、CI artifact/断言与 bug 模板。** CI 的签名 Secret 名只有在仓库配置中建立新 Secret 后替换；切换期间 CI 应明确兼容读取旧名而不把密钥写入日志。
- [ ] **步骤 4：外部目标切换门禁。** 使用干净 clone 实测新 GitHub 主仓/子仓 URL 与 gitlink 可 fetch，实测 Pages/RPR 与 SourceHut 同步目标后才替换 `.gitmodules`、Kconfig 默认 URL、MOTD、网站链接和 CI 发布目标。若目标未就绪，此步骤保留可工作的旧 URL，并在 `docs/branding-compatibility.md` 逐项记录原因；不能写入推测的新地址。
- [ ] **步骤 5：验证 `sh tests/build/test-pages.sh`、`make pages`、`sh tools/build/verify-pages.sh <pages-output>`、`git diff --check`。** 对生成站点和 CI YAML 做路径扫描，确保所有指向的产物存在。

### 任务 11：全链路回归、旧名复审与交付

**文件：** 修改 `docs/branding-compatibility.md`（记录最终兼容清单与验证证据）、本计划复选框；必要时修正前述任务中的具体源文件或测试。

- [ ] **步骤 1：从干净递归 clone 验证新子仓路径和外部远端。** `git submodule update --init --recursive` 后运行主仓与子仓的 `make fetch`；检查新旧 UAPI 导出、gitlink SHA、子仓 dirty 守卫。外部服务未就绪时把此项明确记为未验证，不声称 release 完成。
- [ ] **步骤 2：在隔离 `O` 跑 `make test`、`make test-long`、`make -j8 all`、`make release`。** 测试失败先判定是原有基线、计划问题还是实现问题，不通过跳过测试掩盖。运行 `tests/build/test-submodule-contract.sh` 只用干净测试克隆。
- [ ] **步骤 3：检查发行物。** `readelf`/`nm` 验证新旧运行库与旧/新 ELF，`tar -tzf` 验证 SDK，APK 工具验证包数据库与签名，`xorriso`/`mtools`/`qemu-img` 验证 ISO/ESP/VMDK、Pages/下载/校验和命名一致。
- [ ] **步骤 4：在 QEMU 分别验证新安装、旧安装升级、失败回滚、内核更新、GRUB/Installer/登录/桌面、旧配置和旧 ELF；VMware 专属可见路径在 VMware 单独验证。** 记录实际平台、镜像 SHA、步骤、日志与截图。未执行的环境单列限制。
- [ ] **步骤 5：复审全部旧名命中。** 对主仓与子仓运行下列命令，逐条归入兼容、历史、第三方归属或仍需修复；再用 `git status --short` 列出全部新建未跟踪源码并对这些文件单独 `rg`，避免 `git grep` 漏掉它们。文件名扫描与图像/产物视觉扫描另做。不能仅凭命中数下降宣布完成。

  ```sh
  git grep -I -i -n -E 'leonos|ntclks' || true
  git -C kernel/reliefnt grep -I -i -n -E 'leonos|ntclks' || true
  git ls-files | grep -Ei 'leonos|ntclks' || true
  git diff --check
  git status --short --branch
  ```

- [ ] **步骤 6：提交前审查。** 核对主仓与子仓 diff、所有已有用户改动仍在、只有本任务文件被 stage；按 `AGENT.md`，没有用户明确提交要求时停在可审查状态。交付报告逐项列源码、编译、打包、QEMU/VMware、外部 URL 四类证据和未完成门禁。

## 依赖与审查检查点

1. 任务 1 的命名/兼容矩阵先完成，任务 2 才能定义内核新 UAPI；任务 3 依赖可引用的子仓提交。
2. 任务 3、4、5 的构建与 ABI 闭环通过后，任务 6、7、8 才能迁移运行时路径、载荷和包。
3. 任务 9 的可见品牌可在内部迁移期间准备，但要在最终镜像中复测；任务 10 必须消费任务 7/8 的真实产物名。
4. 任务 11 是最终门禁；任何阶段发现旧安装被覆盖、旧 ELF 不能加载、子仓远端无法 fetch 或新发布物缺失，都回到对应任务修正后重跑定向验证。

## 计划自检对应关系

| 规格章节 | 计划任务 |
| --- | --- |
| 命名与扫描边界 | 1、11 |
| 内核与 UAPI | 2、3、5 |
| 主仓公开 API / SDK | 4、5 |
| 来宾路径、服务和数据 | 6、7 |
| 包/SDK/发布 | 5、8、10 |
| UI、网站、文档、外部目标 | 9、10 |
| 错误/回滚与全链路验收 | 3、6、7、8、11 |
