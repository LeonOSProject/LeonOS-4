# 统一 ext2/3/4 驱动（ext4 + 元数据日志）设计规格

日期：2026-09-27
状态：已经用户逐节确认
对应计划：`docs/superpowers/plans/2026-09-27-ext4-driver.md`

## 目标

为 NTCLKS 内核存储栈编写统一的 ext 族驱动，支持完整 ext4 磁盘布局与
JBD2 式元数据日志（含崩溃恢复），取代现有 ext2 驱动成为发布镜像与安装器
根分区的默认格式；同时保持 ext2/ext3 卷的可挂载兼容。

## 已确认的决策

| 决策点 | 结论 |
| --- | --- |
| 特性范围 | 完整 ext4 含日志（JBD2 式读写 + 崩溃恢复） |
| 替代策略 | 统一驱动兼容 ext2/3/4；旧 ext2 专属路径被吸收，不保留双驱动 |
| 默认格式 | 发布镜像 / 安装器根分区默认 ext4 + 日志；开发/CI 镜像可继续 ext2 |
| 日志范围 | 仅元数据日志（inode 表、位图、目录块、组描述符、超级块）；数据块直写 |
| 实现架构 | 方案 A：演进式统一驱动（吸收现有 storage_ext2*.c），日志独立组件 |

## 架构总览

改动横跨两个仓库，沿既有协作模式（NTCLKS 侧提交 + 父仓 gitlink 更新）：

- 内核侧（NTCLKS 子仓 `kernel/ntclks/`）：存储驱动演进为统一 ext 族驱动。
- 父仓（LeonOS-4）：镜像工具链默认切 ext4、安装器分区格式、文档、集成测试。

挂载时序：`storage_mount` 读超级块 → 特性识别层判定 ext2/3/4 →
若 `has_journal` 先做日志恢复（回放有效事务尾部、截断无效事务）→
按 inode 形态选 extent 或间接块映射 → 进入统一读写路径。

## 组件（内核侧）

| 文件 | 动作 | 职责 |
| --- | --- | --- |
| `drivers/bootstrap/storage/storage_ext4_super.c` | 新增 | 特性识别：解析超级块特性标志，判定 ext2/3/4 与可用路径；未知 incompat 位拒绝 rw 挂载（安全阀） |
| `drivers/bootstrap/storage/storage_ext4_extent.c` | 新增 | extent 树映射：4 层 extent 树（depth 0–3）逻辑块→物理块；写路径分配/追加/分裂 |
| `drivers/bootstrap/storage/storage_journal.c` | 新增 | 元数据日志（JBD2 磁盘格式子集）：挂载恢复、事务 begin / 影子写 / commit / checkpoint、日志满强制 checkpoint |
| `drivers/bootstrap/storage/storage_ext2.c` 等 | 扩展 | 现有读/写/缓存路径保留为共用元数据核心（inode、目录、位图、extent 与间接块共用同一 VFS 出入口）；文件名不动，避免 churn |
| `drivers/bootstrap/storage/storage_mount.c`、`storage_vfs.c` | 小改 | 挂载分派接特性识别层；日志卷先恢复再放行 rw |

## 特性支持范围

- incompat 认：`filetype`、`extent`、`64bit`、`flex_bg`；其余（encrypt、
  casefold、inline_data、verity 等）拒绝挂载并报明确错误。
- ro_compat 认：`sparse_super`、`large_file`、`huge_file`、`dir_nlink`、
  `extra_isize`、`metadata_csum`（读校验 + 写维护，CRC32c）。
- compat 位可忽略（挂载不受阻），`dir_index` 目录按线性扫描处理。
- 日志格式兼容 JBD2 on-disk（magic `0xC03B3998`，描述符/提交/撤销/超级块），
  宿主机 `mke2fs` 生成的卷内核直接可恢复，不自造格式。

## 数据流

- 写文件（rw ext4 卷）：VFS write → 块分配（位图）→ 开启事务，将修改的
  元数据块（位图 / inode 表 / 目录块 / 组描述符 / 超级块）记入日志 →
  数据块直写盘 → commit（写提交块）→ 后台 checkpoint 把元数据刷回原位。
  commit 前崩溃 → 回放整体丢弃；commit 后崩溃 → 回放补全。
- ext2 卷（无日志）继续走现有直写路径，行为不变。
- 挂载：识别特性 → `has_journal` 则读日志超级块、扫描事务链、按事务序号
  判定有效尾部并回放（含 revoke 表）→ 建缓存 → 放行 rw。

## 错误处理

- 未知 incompat 特性 → mount 失败（明确报错），不半挂。
- 日志校验失败 → 截断到最后一个有效事务后继续；日志 inode 本身损坏 →
  整卷 ro 回退并告警。
- 介质错误 → EIO 透传，当前事务标记 abort，卷转 ro（防止在坏盘上继续写坏结构）。
- 日志满 → 触发强制 checkpoint 释放日志空间。

## 测试与验收

- 主机侧单测（沿用 `tools/tests/*_test.c` + `tools/test_*.py` harness）：
  extent 树遍历/分裂、日志回放/截断、事务提交/回绕、64 位组描述符与
  metadata_csum 校验；现有 `ext2_*` 测试全程绿作为回归底线。
- 镜像工具测试：`mke2fs -t ext4` 产出镜像过 `e2fsck -fn` 干净。
- QEMU 集成：安装器创建 ext4 + 日志根分区并启动成功；崩溃注入——写负载中
  强杀 QEMU 再冷启动，验证卷结构完好、已提交操作不丢。
- 验收定义：发布镜像默认 ext4 + 日志通过上述全链路；ext2 旧卷仍可挂载 rw。
