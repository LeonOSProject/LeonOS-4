# ReliefOS / ReliefNT 品牌迁移基线证据（任务 1 步骤 5）

日期：2026-09-27
状态：迁移开始前基线；品牌门禁 `tests/build/test-brand-identity.sh` 保持红灯为预期。

## 工作区状态

- 主仓分支 `feature/rename`，HEAD `d1a16c592a81cf78e856ab8a9cfe87197bf526f5`。
- 内核子仓 `kernel/ntclks` gitlink `5bd8425d10d6879cc578342c0173a92af9468b62`
  （detached HEAD 与 gitlink 一致）。
- 用户未跟踪内容：主仓与子仓的 `max_size = 50G/`（误创建目录，不参与迁移，
  不清理）；`out/`、缓存、第三方子模块内容不作为改名输入。

## 旧名命中基线（git grep -I -i -E 'leonos|ntclks'）

| 范围 | 文本命中文件 | 文本命中行 | 旧名路径 |
| --- | --- | --- | --- |
| 主仓 | 831 | 18,434 | 146 |
| 内核子仓 | 283 | 5,805 | 130 |

与设计规格记录一致（规格中 leonos 783 文件/17,106 行 + ntclks 217 文件/1,474 行，
并集 831 文件；行有交集故并集行数 18,434）。逐文件清单见
`2026-09-27-reliefos-reliefnt-rename-inventory.tsv`（1,142 条）。

## 门禁与允许清单

- `tests/build/test-brand-identity.sh`：分层断言 + 逐文件旧名审计。
  起始断言 15 项红灯（新契约未落地），VERSION_ID=4 语义断言已绿。
- `tests/build/brand-allowlist.tsv`：1,148 条逐文件允许清单
  （1,121 migration + 17 history + 5 compatibility/attribution/external-url +
  3 份迁移文档 history + 1 条目类别数）。审计已验证：
  移出清单条目立即红灯（FAIL - non-allowlisted old name）。
- `git diff --check`：无空白错误。

## 命令记录

```sh
git status --short --branch
git rev-parse HEAD                        # d1a16c592a81cf78e856ab8a9cfe87197bf526f5
git -C kernel/ntclks rev-parse HEAD       # 5bd8425d10d6879cc578342c0173a92af9468b62
git grep -I -i -n -E 'leonos|ntclks' | wc -l
git -C kernel/ntclks grep -I -i -n -E 'leonos|ntclks' | wc -l
git ls-files | grep -Ei 'leonos|ntclks' | wc -l
git diff --check                          # clean
sh tests/build/test-brand-identity.sh     # 15 FAIL（预期红灯）+ 1 todo
```
