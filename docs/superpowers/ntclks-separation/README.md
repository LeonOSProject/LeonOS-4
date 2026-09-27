# ntclks 分离执行产物

本目录是 [ntclks 独立仓库与用户态分离](../specs/2026-09-24-ntclks-separation-design.md)
按 [阶段计划](../plans/2026-09-24-ntclks-separation-agent-prompt.md) 执行时产生的
工作清单与证据记录。它们描述执行时点的事实，不是产品文档。

| 文件 | 内容 |
| --- | --- |
| `00-baseline-and-evidence.md` | 阶段 0：基线构建/测试证据、测试入口盘点、环境事实 |
| `01-header-classification.md` | 阶段 0/1：头文件逐声明分类（UAPI/runtime/boot/module/private/resource） |
| `02-migration-manifest.md` | 阶段 0/2：源文件迁移映射（当前路径 → 目标所有者 → 消费者） |
| `03-permission-matrix.md` | 阶段 0/4：Desktop/windowd 身份与权限判定调用矩阵、负例测试挂钩点 |
| `04-reproducibility-two-path.md` | 阶段 3：双路径可重现性比较结果与建议修复 |
| `05-phase2-standalone-checkout.md` | 阶段 2：独立内核 checkout 的验证证据 |
| `06-handover-2026-09-25.md` | **交接文档**（授权边界、进度、证据、下一步、风险） |
| `07-phase4-runtime-policy.md` | 阶段 4：权限负例 + autospawn/会话策略迁移执行记录（含内核侧留痕） |

原始命令输出与日志保存在任务工作目录
`/home/leon/build/ntclks-sep/logs/`（仓库外），报告中给出相对引用。
