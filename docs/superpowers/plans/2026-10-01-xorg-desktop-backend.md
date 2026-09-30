# Xorg 桌面后端实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 subagent-driven-development（推荐）或 executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 为 ReliefOS 增加一个默认保持原生桌面的 Kconfig 后端选择，并接入 Alpine 官方 x86_64 APK 提供的 Xorg + `xinit` + `xterm` 最小 X11 会话。

**架构：** 顶层 Kconfig 产生互斥的 `DESKTOP_BACKEND_RELIEFOS`/`DESKTOP_BACKEND_XORG` 选择；rootfs staging 将选择编译为不可执行的 `desktop-backend` 标记和对应服务/文件集合。原生桌面继续由现有 `windowd`、`desktop.elf`、`sessiond` 提供，Xorg 模式由 tty1 会话包装脚本启动 Xorg、fbdev、evdev 和 xterm，安装器 runtime 强制使用原生后端。

**技术栈：** Kconfig/kconfig-frontends、POSIX `/bin/sh`、GNU Make、Alpine APK/musl x86_64 二进制、现有 Linux fbdev/evdev ABI、C shell contract tests、QEMU/UEFI smoke test。

**规格：** `docs/superpowers/specs/2026-10-01-xorg-desktop-backend-design.md`

## 全局约束

- 默认选择必须是 `DESKTOP_BACKEND_RELIEFOS=y`，未修改配置时现有 ReliefOS Desktop 行为不得改变。
- 用户可见的原生后端名称为 `ReliefOS Desktop + desktopd`，实现映射为现有 `windowd`、`desktop.elf`、`sessiond`，不新增 `desktopd` 守护进程。
- Xorg 后端只提供最小 X11 会话，不迁移现有 ReliefOS 原生 GUI 应用。
- Xorg 运行时包必须来自 Alpine v3.24 x86_64/musl 官方 APK，并逐包锁定版本、URL、SHA-256、签名和许可证信息。
- 直接 Xorg 包版本固定为：`xorg-server=21.1.24-r0`、`xorg-server-common=21.1.24-r0`、`xinit=1.4.4-r0`、`xterm=410-r0`、`xf86-video-fbdev=0.5.0-r6`、`xf86-input-evdev=2.11.0-r0`、`xkeyboard-config=2.47-r0`、`font-cursor-misc=1.0.4-r1`、`font-misc-misc=1.1.3-r1`；完整传递依赖必须按同一 APKINDEX 逐一锁定。
- 只有显式 `make fetch` 可以联网；普通构建消费已验证缓存，缺包时必须报告具体依赖和修复命令。
- Xorg 使用标准 `/dev/fb0`、`/dev/input/event0`、`/dev/input/event1` 和 Linux fbdev/evdev/POSIX 接口，不新增 Xorg 专用 syscall 或 ReliefOS 私有 ioctl。
- Xorg 后端不得启动 `reliefos-windowd` 或 `reliefos-session`；安装器 runtime 始终恢复原生桌面服务。
- 每次源码修改后运行 `git diff --check`；不把构建、打包或镜像生成误称为 QEMU 运行成功。

## 审查重点（Review Focus）

- 安装器 runtime 与安装后 rootfs 的后端标记混淆：安装器应始终进入原生桌面，安装后 Xorg 配置仍应保留。
- Xorg 模式遗留 `reliefos-windowd`/`reliefos-session` default runlevel：期望不会出现两个图形 server 同时争用 VT、framebuffer 或输入设备。
- 仅锁定顶层 APK 而遗漏传递依赖：期望 APK 数据库、动态链接器和所有 SONAME 在离线 rootfs 中完整可用。
- 后端标记被当作 shell 代码或接受未知值：期望只接受精确的 `reliefos`/`xorg`，未知值进入文本登录并记录错误。
- VT、fbdev 或 evdev 打开失败后的恢复：期望 Xorg 会话记录明确错误、退出到文本登录且不快速无限重启。

## 文件结构与职责

**修改：**

- `Kconfig`：新增互斥桌面后端 choice。
- `configs/default.conf`：保存原生桌面默认选择。
- `configs/dependencies.lock.json`：加入 Xorg 直接包和完整依赖闭包，标记可选 Xorg 特性。
- `tools/host/manifest/reliefos-deps.c`：让锁文件查询工具识别可选依赖特性。
- `tests/build/test-deps.sh`：覆盖特性字段的锁文件语义。
- `tools/build/rootfs-stage.sh`：生成后端标记、条件 staging Xorg 文件、处理安装后 runlevel。
- `tools/build/apk-stage.sh`：按 raw root 的后端标记过滤可选 Xorg APK，并把选中的包纳入 APK 数据库。
- `mk/apk.mk`：向 APK staging 传递锁文件和依赖查询工具。
- `tools/build/installer-stage.sh`：为 installer runtime 恢复原生后端和原生 default runlevel。
- `system/rootfs/usr/lib/reliefos/console-session`：根据后端标记选择原生图形会话或 Xorg 会话。
- `tools/test_console_boot_policy.py`：扩展启动分支、未知值和 installer 优先级测试。
- `tests/build/test-installer-stage.sh`：适配新的 APK staging 参数并覆盖 installer runtime 恢复策略。
- `docs/ABI.md`：记录 Xorg 所消费的标准 fbdev/evdev 接口和当前限制。

**创建：**

- `system/xorg/xorg.conf`：Xorg fbdev/evdev 的固定配置源。
- `system/xorg/reliefos-xorg-session`：从 tty1 启动 `xinit`/Xorg 的 POSIX shell 包装器。
- `system/xorg/reliefos-xorg-client`：启动 `xterm` 和 `/bin/login` 的 X11 client 脚本。
- `tests/build/test-desktop-backend.sh`：Kconfig、rootfs policy 和启动脚本 contract test。
- `tests/build/test-apk-stage-selection.sh`：Xorg/base APK 选择和离线 staging contract test。
- `tools/tests/xorg_device_probe.c`：使用标准 fbdev/evdev UAPI 的来宾设备探针。
- `tools/test_xorg_qemu.py`：构建 Xorg 配置镜像并验证真实 QEMU 会话。
- `docs/XORG.md`：用户和维护者的配置、fetch、运行与验证说明。

---

### 任务 1：增加桌面后端 Kconfig choice

**文件：**
- 修改：`Kconfig`
- 修改：`configs/default.conf`
- 创建：`tests/build/test-desktop-backend.sh`

- [ ] **步骤 1：编写失败的配置 contract test**

在 `tests/build/test-desktop-backend.sh` 中固定验证：

```sh
grep -q '^config DESKTOP_BACKEND_RELIEFOS$' Kconfig
grep -q '^config DESKTOP_BACKEND_XORG$' Kconfig
grep -q '^CONFIG_DESKTOP_BACKEND_RELIEFOS=y$' configs/default.conf
! grep -q '^CONFIG_DESKTOP_BACKEND_XORG=y$' configs/default.conf
```

测试还要使用 `make O="$work/out" defconfig` 检查生成 `.config` 默认选择原生后端，并使用一个只把 `CONFIG_DESKTOP_BACKEND_XORG=y` 写入 `.config` 的 fixture 运行 `olddefconfig`，确认最终配置不会同时启用两个符号。

- [ ] **步骤 2：运行测试确认失败**

运行：`sh tests/build/test-desktop-backend.sh`

预期：FAIL，报告缺少 `DESKTOP_BACKEND_RELIEFOS` 或 `DESKTOP_BACKEND_XORG`。

- [ ] **步骤 3：实现 Kconfig choice**

在 `Kconfig` 的 Build/桌面相关区域加入一个 `choice`，符号名固定为 `DESKTOP_BACKEND_RELIEFOS` 和 `DESKTOP_BACKEND_XORG`，默认 `DESKTOP_BACKEND_RELIEFOS`；在 `configs/default.conf` 写入原生后端为 `y`、Xorg 后端未设置。Kconfig help 必须说明用户可见的 `desktopd` 名称与实际 `windowd`/`desktop.elf`/`sessiond` 实现映射。

- [ ] **步骤 4：运行配置测试确认通过**

运行：`sh tests/build/test-desktop-backend.sh`

预期：所有 Kconfig/default/choice 检查 PASS，并且 `make defconfig`/`olddefconfig` 的 `.config` 只保留一个后端。

- [ ] **步骤 5：Commit**

```bash
git add Kconfig configs/default.conf tests/build/test-desktop-backend.sh
git commit -m "feat: add desktop backend choice"
```

**接口：** 后续任务读取 `CONFIG_DESKTOP_BACKEND_RELIEFOS`/`CONFIG_DESKTOP_BACKEND_XORG`；不得另造同义符号。

### 任务 2：让 rootfs 产生后端 policy 与条件服务集合

**文件：**
- 修改：`tools/build/rootfs-stage.sh`
- 修改：`tests/build/test-desktop-backend.sh`
- 修改：`tests/build/test-installer-stage.sh`

- [ ] **步骤 1：先增加失败的 rootfs policy 断言**

扩展 `tests/build/test-desktop-backend.sh`，验证 `rootfs-stage.sh` 的计划输出在原生/Xorg 两个 config fixture 下分别满足：

```sh
grep -qx 'reliefos' "$reliefos_root/etc/reliefos/desktop-backend"
grep -qx 'xorg' "$xorg_root/etc/reliefos/desktop-backend"
test -L "$reliefos_root/etc/runlevels/default/reliefos-windowd"
test -L "$reliefos_root/etc/runlevels/default/reliefos-session"
test ! -e "$xorg_root/etc/runlevels/default/reliefos-windowd"
test ! -e "$xorg_root/etc/runlevels/default/reliefos-session"
```

测试 fixture 必须通过真实 `rootfs-stage.sh` 计划/`reliefos-stage` 发布路径检查，而不是只复制源码目录。

- [ ] **步骤 2：运行测试确认失败**

运行：`sh tests/build/test-desktop-backend.sh`

预期：FAIL，缺少 `desktop-backend` 生成文件，且 Xorg runlevel 仍包含原生图形服务。

- [ ] **步骤 3：实现 staging policy**

在 `tools/build/rootfs-stage.sh` 中从 Kconfig 精确生成 `/etc/reliefos/desktop-backend`，只允许 `reliefos`/`xorg`；Xorg 模式通过现有 plan deletion 机制删除安装后 rootfs 的 `default/reliefos-windowd` 和 `default/reliefos-session` 链接，保留服务脚本和原生 ELF。Xorg 源文件仅在 Xorg 模式通过 `file` 记录进入 rootfs。未知配置必须让 staging 失败，而不是默认选择某个后端。

- [ ] **步骤 4：验证安装器 runtime fixture**

在 `tests/build/test-installer-stage.sh` 的 fixture rootfs 中加入 Xorg backend marker 和两个原生 runlevel 链接，扩展断言使 installer runtime root 仍恢复 `reliefos` marker 和两个原生服务链接，而安装后 root 保持 Xorg 选择。

运行：`sh tests/build/test-installer-stage.sh`

预期：PASS；installer runtime 与安装后 root 的后端 policy 不互相污染。

- [ ] **步骤 5：运行回归测试确认通过**

运行：`sh tests/build/test-desktop-backend.sh && sh tests/build/test-installer-stage.sh`

预期：PASS；原生/Xorg rootfs 的标记和 runlevel 集合互斥，installer runtime 保持原生。

- [ ] **步骤 6：Commit**

```bash
git add tools/build/rootfs-stage.sh tests/build/test-desktop-backend.sh tests/build/test-installer-stage.sh
git commit -m "build: stage desktop backend policy"
```

**接口：** `desktop-backend` 是非脚本单行文件；安装后 Xorg root 删除两个 default runlevel 链接，installer runtime 在进入 APK staging 前恢复原生 policy。

### 任务 3：扩展依赖锁并固定 Alpine Xorg APK 闭包

**文件：**
- 修改：`tools/host/manifest/reliefos-deps.c`
- 修改：`tests/build/test-deps.sh`
- 修改：`configs/dependencies.lock.json`
- 测试：`tests/build/test-deps.sh`

- [ ] **步骤 1：编写失败的 lock feature 测试**

在 `tests/build/test-deps.sh` 加入 fixture：

```sh
expect_output_is 'an xorg APK reports its optional feature' xorg \
    "$deps" --lock "$lock" --id alpine-xorg-server --print feature
expect_output_is 'an unmarked dependency defaults to the base feature' base \
    "$deps" --lock "$lock" --id alpine-openrc --print feature
```

同时要求带有非法 `feature` 值的 fixture 被 `--check` 拒绝，并保留未知字段拒绝测试。

- [ ] **步骤 2：运行测试确认失败**

运行：`make -s O=out/xorg-plan-test test-build`

预期：FAIL，`reliefos-deps` 不认识 `feature` 查询或 Xorg lock entry 尚不存在。

- [ ] **步骤 3：实现 lock feature 查询**

在 `tools/host/manifest/reliefos-deps.c` 增加可选 `feature` 字段，允许值只有 `base` 和 `xorg`；未填写时 `--print feature` 返回 `base`，其他缺失字段的查询行为不变。更新校验和帮助文本。

- [ ] **步骤 4：加入直接 APK 与完整传递闭包**

在 `configs/dependencies.lock.json` 加入直接包及其官方 APKINDEX 解析出的所有传递包：

```text
alpine-xorg-server       21.1.24-r0
alpine-xorg-server-common 21.1.24-r0
alpine-xinit             1.4.4-r0
alpine-xterm             410-r0
alpine-xf86-video-fbdev  0.5.0-r6
alpine-xf86-input-evdev  2.11.0-r0
alpine-xkeyboard-config  2.47-r0
alpine-font-cursor-misc  1.0.4-r1
alpine-font-misc-misc    1.1.3-r1
```

每个新 APK entry 设置 `kind=apk`、`script=tools/build/upstream-apk.sh`、官方 v3.24 x86_64 URL、实际 SHA-256、APK 顶层目录和 `feature=xorg`；通过 `D:` 依赖和 `so:` provider 闭包检查，不保留未解析的虚拟依赖。

- [ ] **步骤 5：运行锁文件测试确认通过**

运行：`make -s O=out/xorg-plan-test test-build`

预期：`reliefos-deps --check`、feature 查询、下载列表和现有依赖 contract tests 全部 PASS。

- [ ] **步骤 6：Commit**

```bash
git add tools/host/manifest/reliefos-deps.c tests/build/test-deps.sh configs/dependencies.lock.json
git commit -m "build: lock Alpine Xorg packages"
```

**接口：** APK 条目通过 `reliefos-deps --id alpine-<pkgname> --print feature` 查询；未标记包等价于 `base`，标记为 `xorg` 的包只在 Xorg rootfs 进入最终 APK transaction。

### 任务 4：让 APK staging 按后端选择安装包

**文件：**
- 修改：`tools/build/apk-stage.sh`
- 修改：`mk/apk.mk`
- 修改：`tools/build/installer-stage.sh`
- 创建：`tests/build/test-apk-stage-selection.sh`
- 修改：`tests/build/test-installer-stage.sh`

- [ ] **步骤 1：编写失败的选择性 staging 测试**

在 `tests/build/test-apk-stage-selection.sh` 使用最小 fake APK/`.PKGINFO`、fake `reliefos-deps` 和 fake `apk` 记录调用，验证：

```sh
run_stage reliefos
! grep -q 'xorg-server' "$log"
grep -q 'openrc' "$log"
run_stage xorg
grep -q 'xorg-server' "$log"
grep -q 'xterm' "$log"
```

同时验证被过滤的 Xorg archive 不会被复制到最终 repository，也不会参与 raw overlay 删除或 ownership/APK database transaction。

- [ ] **步骤 2：运行测试确认失败**

运行：`sh tests/build/test-apk-stage-selection.sh`

预期：FAIL，现有 `apk-stage.sh` 会把所有 `alpine-*` archive 无条件加入 transaction。

- [ ] **步骤 3：扩展 `apk-stage.sh` 接口并实现过滤**

将 `apk-stage.sh` 参数扩展为 `SRC RAW_ROOT OUTPUT_ROOT WORK APK UPSTREAM DEPS LOCK POLICY OWN_TOOL KEY VERSION`。从 `RAW_ROOT/etc/reliefos/desktop-backend` 精确读取后端，按 `feature` 过滤 APK archive；过滤必须发生在冲突清理、repository copy、ownership 扫描、index 和最终 `apk add` 之前。`base` 包始终保留，`xorg` 包只在 backend 为 `xorg` 时保留，未知 backend 立即失败。

- [ ] **步骤 4：更新所有真实调用者**

在 `mk/apk.mk` 和 `tools/build/installer-stage.sh` 传入 `RELIEFOS_DEPS_TOOL` 与 `RELIEFOS_LOCK`。更新 installer fixture adapter，使其接受扩展后的参数；确保 installer runtime 已由任务 2 设置为 `reliefos`，因此不会安装 Xorg runtime。

- [ ] **步骤 5：运行 APK/安装器测试确认通过**

运行：`sh tests/build/test-apk-stage-selection.sh && sh tests/build/test-installer-stage.sh && make -s O=out/xorg-plan-test test-apk`

预期：base/Xorg 选择正确，APK ownership、签名 repository、依赖数据库和 installer runtime 回归全部 PASS。

- [ ] **步骤 6：Commit**

```bash
git add tools/build/apk-stage.sh mk/apk.mk tools/build/installer-stage.sh tests/build/test-apk-stage-selection.sh tests/build/test-installer-stage.sh
git commit -m "build: select Xorg APKs by desktop backend"
```

### 任务 5：加入 Xorg 配置、会话包装器和启动分支

**文件：**
- 创建：`system/xorg/xorg.conf`
- 创建：`system/xorg/reliefos-xorg-session`
- 创建：`system/xorg/reliefos-xorg-client`
- 修改：`system/rootfs/usr/lib/reliefos/console-session`
- 修改：`tools/build/rootfs-stage.sh`
- 修改：`tools/test_console_boot_policy.py`
- 创建：`tests/build/test-xorg-session.sh`

- [ ] **步骤 1：编写失败的会话 contract test**

在 `tests/build/test-xorg-session.sh` 固定断言：

```sh
grep -q 'Driver[[:space:]]*"fbdev"' system/xorg/xorg.conf
grep -q '/dev/fb0' system/xorg/xorg.conf
grep -q '/dev/input/event0' system/xorg/xorg.conf
grep -q '/dev/input/event1' system/xorg/xorg.conf
grep -q 'xinit' system/xorg/reliefos-xorg-session
grep -q 'xterm' system/xorg/reliefos-xorg-client
grep -q '/bin/login' system/xorg/reliefos-xorg-client
```

并检查 `console-session` 的分支顺序是 installer marker → backend marker → text getty，未知 backend 不执行 Xorg。

- [ ] **步骤 2：运行测试确认失败**

运行：`sh tests/build/test-xorg-session.sh`

预期：FAIL，Xorg 源文件和 backend 分支尚不存在。

- [ ] **步骤 3：实现固定 fbdev/evdev 配置**

创建 `system/xorg/xorg.conf`，使用 `fbdev` `/dev/fb0`、`evdev` `/dev/input/event0` 键盘和 `/dev/input/event1` 鼠标，固定 tty1，禁止依赖 udev 自动枚举、DRM/KMS 和 libinput。由 `rootfs-stage.sh` 仅在 Xorg backend 记录到 `/etc/X11/xorg.conf`。

- [ ] **步骤 4：实现 POSIX Xorg session/client**

创建 `reliefos-xorg-session`，以 tty1 控制终端执行：

```text
/usr/bin/xinit /usr/lib/reliefos/reliefos-xorg-client -- /usr/bin/Xorg :0 -config /etc/X11/xorg.conf -vt 1 -keeptty -novtswitch
```

创建 `reliefos-xorg-client`，使用 `/usr/bin/xterm` 在其伪终端内执行 `/bin/login`，不执行 root shell，不使用 Bash 专属语法。脚本必须保留退出状态并让 `console-session` 回到文本 getty。

- [ ] **步骤 5：接入 console-session 分支**

修改 `console-session`：installer runtime 无条件调用现有原生图形会话；普通系统按 `desktop-backend` 精确选择原生 `login.elf --graphical-session` 或 `reliefos-xorg-session`；未知/缺失值写错误到日志并跳过图形启动，继续文本登录。

- [ ] **步骤 6：运行启动测试确认通过**

运行：`sh tests/build/test-xorg-session.sh && python3 tools/test_console_boot_policy.py`

预期：Xorg 配置、脚本调用链、installer 优先级、原生回归和未知值错误路径全部 PASS。

- [ ] **步骤 7：Commit**

```bash
git add system/xorg system/rootfs/usr/lib/reliefos/console-session tools/build/rootfs-stage.sh tools/test_console_boot_policy.py tests/build/test-xorg-session.sh
git commit -m "feat: add Xorg graphical session"
```

**接口：** 任务 2 提供 `/etc/reliefos/desktop-backend`；本任务只消费 `reliefos`/`xorg` 两个精确值，并提供 `/usr/lib/reliefos/reliefos-xorg-session` 给 `console-session`。

### 任务 6：覆盖标准 fbdev/evdev ABI

**文件：**
- 创建：`tools/tests/xorg_device_probe.c`
- 创建：`tools/test_xorg_abi.py`
- 修改：`docs/ABI.md`

- [ ] **步骤 1：编写失败的标准设备探针**

在 `tools/tests/xorg_device_probe.c` 使用公开 Linux UAPI 打开 `/dev/fb0`、`/dev/input/event0`、`/dev/input/event1`，执行 `FBIOGET_VSCREENINFO`、`FBIOGET_FSCREENINFO`、`EVIOCGNAME`、`EVIOCGBIT`，并以有限超时验证 evdev `poll`/`read` 不破坏 `struct input_event` 布局。不得调用 `RELIEFOS_FBIOBLIT` 或 `RELIEFOS_EVIOCSVT`。

在 `tools/test_xorg_abi.py` 验证源代码只使用标准 Linux/POSIX 路径，并将探针的静态链接、来宾运行和失败 errno 记录为独立结果。

- [ ] **步骤 2：运行 ABI 测试确认当前缺口或通过**

运行：`python3 tools/test_xorg_abi.py --source-only`

预期：在实现探针前 FAIL；若现有 fbdev/evdev 兼容已经足够，来宾阶段应记录 PASS，不修改私有 ABI。

- [ ] **步骤 3：实现最小标准 ABI probe**

使用现有 ReliefOS musl SDK 编译探针，所有 ioctl 常量、结构和错误检查来自已导出的 Linux UAPI；不复制手写结构，不新增 syscall/ioctl。若测试暴露标准 Linux 语义缺口，只允许在现有对应内核实现中补齐，并同步 ABI contract；不得通过 Xorg 包或启动脚本绕过。

- [ ] **步骤 4：更新 ABI 文档**

在 `docs/ABI.md` 增加 Xorg 依赖的 fbdev/evdev 章节，明确当前支持的 ioctl、设备路径、活动 VT 权限和不支持 DRM/KMS/libinput 的范围。

- [ ] **步骤 5：运行 ABI 验证确认通过**

运行：`python3 tools/test_xorg_abi.py --source-only`，并在任务 7 的 QEMU 镜像中运行来宾探针。

预期：探针只使用标准 ABI，fbdev/evdev 打开和查询行为可诊断；没有新增 ReliefOS 私有接口。

- [ ] **步骤 6：Commit**

```bash
git add tools/tests/xorg_device_probe.c tools/test_xorg_abi.py docs/ABI.md
git commit -m "test: cover Xorg Linux device ABI"
```

### 任务 7：构建 Xorg 镜像并做 QEMU 端到端验证

**文件：**
- 创建：`tools/test_xorg_qemu.py`
- 创建：`docs/XORG.md`

- [ ] **步骤 1：编写失败的 QEMU 验收脚本**

让 `tools/test_xorg_qemu.py` 接受 `--output`、`--cache` 和 `--qemu`，启动一个从 `.config` 选择 Xorg backend 的 VMDK/ISO，验证串口日志出现 Xorg 会话启动、Xorg 日志路径和 xterm client 路径；若 QMP/显示可用，再抓取屏幕并确认非黑屏 X11 输出。

必须额外验证：

```text
Xorg server started
xterm client started
/bin/login prompt reached
native windowd/sessiond not started
```

这些字符串可由新包装器写入独立日志，不修改内核日志协议。

- [ ] **步骤 2：运行测试确认失败**

运行：`python3 tools/test_xorg_qemu.py --output build/xorg-qemu`

预期：FAIL，Xorg 镜像、wrapper 或 QEMU 验收脚本尚不存在。

- [ ] **步骤 3：实现镜像构建和 QEMU 驱动**

脚本创建独立 `O=out/xorg-qemu`，运行 `make defconfig` 后只替换后端 choice 为 `CONFIG_DESKTOP_BACKEND_XORG=y`，再运行 `olddefconfig` 和目标镜像构建。构建前执行 `make fetch` 或对缓存执行 verify-only；QEMU 使用现有 UEFI/VGA/串口/QMP 约定，QMP socket 放在支持 Unix socket 的工作目录，不放 `/tmp` 或 DrvFs。

- [ ] **步骤 4：实现日志和图像验收**

让脚本等待 Xorg session 的有限时间，读取串口和 guest 日志，检查 Xorg server、xterm、login prompt、未启动原生 server、退出后文本 VT 五个状态；超时必须输出 QEMU 日志、Xorg 日志和最后一帧证据。

- [ ] **步骤 5：编写使用文档**

在 `docs/XORG.md` 说明 Kconfig/menuconfig 选择、`make fetch`、离线构建、Xorg 的 fbdev/evdev 限制、最小 xterm 登录流程、安装器仍使用原生桌面，以及 QEMU 验证命令和未验证的硬件范围。

- [ ] **步骤 6：运行完整验证**

运行：

```bash
git diff --check
make -s O=out/xorg-final test
make -s O=out/xorg-qemu fetch
python3 tools/test_xorg_qemu.py --output build/xorg-qemu
```

预期：配置、依赖、staging、ABI 和 QEMU X11 会话全部通过；默认 ReliefOS 配置和 Xorg 配置均可独立构建。

- [ ] **步骤 7：Commit**

```bash
git add tools/test_xorg_qemu.py docs/XORG.md
git commit -m "test: verify Xorg desktop backend in QEMU"
```

## 任务接口总览

- 任务 1 定义唯一 Kconfig 符号：`DESKTOP_BACKEND_RELIEFOS`、`DESKTOP_BACKEND_XORG`。
- 任务 2 将 choice 编译为 `/etc/reliefos/desktop-backend`，并定义安装后/Xorg 与 installer/native 的 rootfs 差异。
- 任务 3 定义锁文件的 `feature` 查询，并为 Xorg APK 固定官方版本和依赖闭包。
- 任务 4 消费任务 2 的 backend marker 和任务 3 的 feature 查询，生成最终 APK transaction。
- 任务 5 消费任务 2 的 backend marker，提供 Xorg session wrapper 给 `console-session`。
- 任务 6 只使用标准 Linux/POSIX ABI，不改变任务 5 的启动接口。
- 任务 7 通过真实镜像和 QEMU 验证所有前置接口，并产出用户文档。
