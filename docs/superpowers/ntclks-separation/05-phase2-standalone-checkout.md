# 阶段 2：独立内核 checkout 证据

日期：2026-09-25。产物：`/home/leon/build/ntclks-sep/ntclks/`（无 .git 的独立
checkout，6.5 MB 源码树），输出 O=`/home/leon/build/ntclks-sep/ntclks-out*`，
构建日志 `logs/ntclks-*.log`。树布局 = 迁移清单 §7（boot/loader 仅 3 个文件、
include/leonos 仅 10 个内核依赖头、include/uapi 全量 65 头、mk 七片段+裁剪版
resources.mk、unifont 闭包、kconfig-frontends、puff.c 门禁、repo-local cache）。

## 入口与验证（逐项实测）

| 验证 | 结果 |
| --- | --- |
| `make O=… all -j` 六制品 | ✓（独立复跑 BUILD_OK） |
| 二次增量构建 | ✓ 零动作；touch 单源 → 恰 4 动作（CC/LD/IMAGE×2） |
| `headers_install` | ✓ 65 头 == 白名单，C/C++ 自包含 |
| `install DESTDIR=…` | ✓ 9 制品 + manifest（format_version/arch/toolchain/config+uapi sha/handoff 7/逐制品 sha256，`sha256sum -c` 9/9） |
| `make test` | ✓ host 测试 + ABI 金值 + 导出边界（独立复跑通过） |
| `make clean` | ✓ 保 config 与 cache |
| **无父源码构建** | ✓ `unshare -rm` 将 `/home/leon/projects` 整体遮蔽后完整构建成功（独立复跑通过） |
| `PROFILE=debug` 全链 | ✓ |
| 产品对齐父基线 | 9/9 逐字节一致（需显式 SOURCE_DATE_EPOCH=提交时间；无 git 时回退 0 只影响 build_info 时间块 17 字节） |

## 清单订正（已回写 02-migration-manifest.md）

- `Kconfig.components` 是 Kconfig source 的必需输入。
- `boot/loader/string.c`（本分支 9eff3c3）使 loader 源为 3。
- 父仓 `scripts/clean.sh` 产品表缺 `kernel-export`/`kernel-install`（已修复）。
- `abi_layout_golden.json` 曾内嵌父仓绝对路径（已改为位置无关归一）。

## 遗留

- checkout 内 `third_party/kconfig-frontends` 为拷贝树，阶段 5 需转真实 submodule。
- `configs/dependencies.lock.json` 已裁剪为 unifont 单条目（内核所需全部）。
