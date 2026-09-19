# 旧构建规则语义对照审计

日期：2026-09-19。按用户要求停止追加全量测试和镜像重建，先做源码对照。本报告不是新的测试通过声明。

对照基线：Git `648ca17` 的旧构建实现与当前未提交的 Make/C 实现。已删除文件用 `git show 648ca17:<path>` 阅读，未恢复旧生产入口。下列旧文件行号指该版本；新文件行号指审计时工作区。

## 结论

目标已经接入 Make 不代表旧规则语义全部迁移。当前仍有明确遗漏，不能宣称全量迁移验收完成。此次用户的 binutils 冲突就是打包前清理规则遗漏，不是需要忽略的 APK 返回码。

## 审计时确认未迁移的规则

| 优先级 | 规则与旧证据 | 新入口与缺口 | 影响及处理方向 |
| --- | --- | --- | --- |
| P1 | `build.py:739–743`：glxgears 入镜像时强制携带 PortableGL 共享库 | `tools/build/rootfs-stage.sh:112` 只看 portablegl 自身 IMAGE；组件依赖解析只提升 BUILD | glxgears IMAGE=y、portablegl IMAGE=n 时可能生成缺少运行库的镜像。恢复运行时依赖传播。默认配置不一定触发。 |
| P2 | `build.py:2269–2281`：为 vim、busybox、file、lua、cmd、less、sl 等入镜像工具生成运行时 manifest | `tools/build/rootfs-stage.sh:66–79` 仅处理 kind 以 `-app` 结尾的组件；上述组件为 tool | 可执行文件存在但缺失原有应用注册信息。应按实际安装路径生成工具 manifest，不能只放宽 kind 后沿用 app 的 ELF 路径。 |
| P2 | `tools/apk_distribution.py:312`：仅删除解析到 BusyBox 的 `/bin/bash` 旧别名 | `tools/build/apk-stage.sh` 没有对应清理 | 重打包带旧别名的根目录时可能重新声明 Bash 路径，引入与真实 bash 包的所有权冲突。当前新鲜 rootfs 是否有该别名不能由此推定。 |
| P2 | `tools/apk_distribution.py:296–302`：拒绝 run/tmp 符号链接、清空运行时数据、重新应用根布局 | `tools/build/apk-stage.sh:36–43` 仅清除 APK 数据库、缓存及仓库等 | 重打包边界缺少旧有运行时目录清理与校验。普通 rootfs 阶段仍建立布局，不等于打包入口保留了同等保证。 |
| P2 | `tools/apk_distribution.py:320` 和 `:360`：写 apk-tools 与 leonos-openrc 的 SOURCE.json | 新 `tools/build/` 与 `mk/` 无对应 SOURCE.json 生成步骤 | 原有上游来源、版本和摘要记录丢失。应使用锁定的上游元数据生成，不应手写一份与实际下载脱节的记录。 |
| P2 | `tools/make_installer_root.py:81`、`:239`：对安装器运行时与内嵌安装根的相同文件进行硬链接去重 | `tools/build/installer-stage.sh` 复制两套根后直接发布，未去重 | 重复载荷增大安装器根文件系统、I/O 和打包开销。恢复等内容文件去重时须同时保留权限等语义。未测量具体空间差额。 |
| P2 | `build.py:709–714`：RPR URL 去尾斜杠、要求 HTTPS、拒绝空白和引号等 | `tools/build/rootfs-stage.sh:124` 使用 awk `-F=` 直接取第二字段，无等价校验 | 配置校验遗漏；包含等号的值也会截断。应完整解析配置字符串，再执行旧有校验。 |
| P2 | `tools/package_devtools.py:462,474,517–554`：导出 SQLite、PortableGL、musl、zlib、libpng、libmagic、Lua、StardustUI 版本记录 | `mk/sdk.mk`、`tools/build/musl-sdk.sh`、`developer-sdk.sh` 未生成这些 THIRD_PARTY/*-VERSION 文件 | SDK 版本溯源输出未等价迁移。已有许可证及部分 BUILD.json 不等于这些版本记录。 |
| P2 | `buildsystem/components.py:238–260`：API 依赖闭包及校验 | `tools/host/manifest/leonos-components.c:120` 明确拒绝非空 api_requires；输出固定空数组 | 组件清单支持的能力退化。当前清单未使用该字段，因此不列为默认构建阻塞；但不能视为完整迁移。 |

## 已修复源码、需要与上述遗漏区分的项目

- 安装器专用 installer.elf：此前正常 rootfs 因 stage=false 不含该程序，新安装器又没有补入。已补构建依赖和运行时安装步骤；此前生成的 ISO 已做文件存在性检查，但这不是 VMware GUI 验证。
- BusyBox 的 usr/bin/ar、usr/bin/strings：旧打包器排除 BusyBox 后备链接，新打包器遗漏，导致与已安装的 binutils 冲突。当前源码已补排除。新规则目前删除任意同名符号链接，旧规则仅排除目标以 /busybox 结尾者，后续应收窄到原规则语义。
- 已将默认包版本代际提升到 2，避免修复后内容哈希变小导致无法升级。但该代际的最终镜像重建已按用户要求停止，不能称其 ISO 已交付。

## 已核对、没有作为遗漏列出的差异

- required 组件忽略 IMAGE/ENTRY/SDK/API 的遗留配置覆盖：旧 resolver 同样如此，不能当作新缺陷。
- installer 对 desktop/settings 的无条件策略覆盖：当前清单二者均 required=true、stage=true，因此目前不构成自定义 Kconfig 关闭失败。旧实现按清单筛选，更通用；修改清单时仍须重新审视。
- 根目录布局：新 C 工具仍使用共享的 UAPI 布局定义，没有证据支持“布局整体未迁移”。
- 新 staging 从新目录发布，可以替代部分旧的过期文件删除操作；不能机械地把所有旧 unlink 当作遗漏。

## 范围和后续次序

本轮重点核对 APK 预处理、安装器两套根、组件选择、运行时注册和 SDK 导出，未宣称逐条穷尽旧构建器全部规则。没有启动新一轮全量测试，也没有新增测试用例。

应先修补影响镜像内容和包所有权的规则，再补来源记录及 SDK 输出；每项先检查静态输入、输出和触发条件。完成一批实质修复后只运行与之相关的定向检查，避免在仍有已知规则遗漏时反复重建整套镜像。

## 本轮补齐状态

上述九项已补入源码，未运行全量测试或重建 ISO：

- rootfs 恢复 glxgears → PortableGL 的镜像条件，以及七个工具的注册清单；清单旁的 ELF 链接指向其现有命令路径，BusyBox 指向 `/bin/busybox`。
- APK 使用共享 C 布局生成器清理 run/tmp、恢复目录与标准链接；拒绝冲突的布局链接。排除 BusyBox 的 ar/strings 后备链接，移除已知 BusyBox bash 别名，保留真实 Bash 和其他同名链接。
- APK 的两个 SOURCE.json 保存依赖锁记录；apk-tools 额外保存实际 bootstrap 二进制 SHA-256。该记录格式明确改为锁文件溯源，不伪造旧格式或重复维护版本常量。
- 安装器使用 `leonos-dedup` 对 usr/bin/sbin/opt 及内嵌安装根相应路径去重。哈希只筛选候选，实际字节、模式、UID/GID、设备号一致才建立硬链接；符号链接不处理，替换先建临时硬链接再 rename。
- RPR 配置独立解析完整值，保留等号、去尾斜杠并恢复 HTTPS/字符校验。
- SDK 从构建依赖锁导出八类 VERSION 文件和完整锁副本，按 SDK 组件选择输出可选记录。
- 组件解析支持 api_requires，检查未知依赖及循环，传播 API 和 BUILD 依赖；保留 api 能力与用户选择的区别。
- 新脚本、锁文件、组件元数据、布局和去重工具均接入对应 Make 前置依赖；已有安装器夹具使用实际 C 去重工具。

定向验证：两个 C 工具通过项目严格警告编译；URL、布局清理、同内容/不同模式文件与符号链接去重、当前组件清单、API 依赖闭包/循环拒绝、SDK 版本输出、安装器夹具通过。rootfs 使用临时计划捕获器检查 PortableGL 条件和工具清单，不复制整套系统；`make -n O=out/full-migration ... installer` 返回 0，仅检查构建图展开。

这些证据不等同于实际签名 APK 事务、完整镜像打包或 VMware 来宾验收。本轮没有提交、推送或发布新 ISO。
