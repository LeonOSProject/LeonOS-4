# 审查包：GNU Make + C 构建重建（P0–P2c）

> 当前工作区全量迁移审查入口：[全量迁移记录](full-migration-2026-09-19.md)。包含未提交变更，审查时同时查看 `git diff` 和 `git ls-files --others --exclude-standard`，不能只查看旧提交范围。

> 接管后的修改和最新边界见 [接管记录](takeover-2026-09-19.md)。下文保留原交接历史；PAM、runtime、musl SDK 子集已经继续迁移，原“未开始”描述不再代表当前工作区。

计划：原构建重建计划（已清理，历史版本可从 Git 查看）
分支：`xiaobai/dev/buildsystem`　基线提交：`6be4c69`
完整 diff：`git diff 6be4c69..HEAD`（提交清单以 `git log --oneline 6be4c69..HEAD` 为准）
审查人：原 Agent（按用户要求，不替换审查者）。**未推送、未合并、未发布镜像。**

## 1. 交付范围：完成到 P2-d（认证链第一段），P2 的其余部分 / P3 / P4 / P5 未开始

这不是一次"整个计划做完"的交付。已完成的阶段都有可复跑的证明；未完成的阶段**保留为
`exit 2` 的显式拒绝**，没有任何目标被伪装成绿色。第 5 节列出剩余关键路径与其唯一阻塞点。

## 2. 按阶段的提交

| 提交 | 阶段 | 内容 |
| --- | --- | --- |
| `380688a` | P1-a | `tools/host/common/{buffer,io,process}` + `leonos-emit`，74 项 C 契约检查，GCC/Clang + ASan/UBSan |
| `78483fe` | P0-a/b/c | 迁移台账、旧系统基线（kernel/sdk/vmdk/iso/installer 的墙钟与峰值 RSS）、上游 Meson/Python 审计与 PAM 可行性 |
| `e43cca6` | P0-b | 补全 installer 基线与哈希（先前 rc=126 是我自己的 `measure.sh` 可执行位丢失，非构建失败） |
| `ba0d336` | P1-b | `Makefile` + `mk/{host,toolchain,config,kernel}.mk`、4 个 C 工具、内核闭环 |
| `8233d84` | P1-e | 任意 `O=` 可用 + 增量正确性（A02–A07 的 mtime/depfile 度量） |
| `6de6f1d` | P1-d | 验收证据归档，并**如实记录来宾启动验证受阻**（无回归证据、也无成功启动证据） |
| `351fe63` | P2-a | `configs/dependencies.lock.json`（41 条，唯一权威）、严格 JSON 读取器 + `leonos-deps`、`make fetch`、`musl-sysroot.sh`（239 文件中 238 与旧系统逐字节一致） |
| `3a1ad8c` | 自身缺陷修复 | A05 读的是从未写入的快照文件；`$(MAKEPID)` 在 POSIX 上为空导致签名 candidate 仍共享文件名 |
| `af8bec8` | P2-b | 同一 `O` 的双 make 明确拒绝（`scripts/build-lock.sh`），含变异检验 |
| `608323e` | P2-c | `make test-long`：A08（`-j1`/`-j8` 三轮）、A10 中断半、A16 execve 跟踪；`legacy-removal.md`；性能记录 |
| 最新提交 | P2-d | `tools/build/auth-upstream.sh` + `make leonos-auth`：Linux UAPI 头与 libxcrypt 由上游 configure 构建（计划第 9 节允许），`crypt.h` 与旧树逐字节等价、`.o` 跨 `O=` 路径字节稳定（`-fmacro-prefix-map`），23 项契约；顺带修掉 `O=` 泄漏进内核构建树、以及 `make clean` 认不出纯构建目标产物树两个缺陷（`verification.md` 第 11 节）|

**任务开始前工作区已有的改动**：仅一个未跟踪文件
原构建重建计划（已清理，历史版本可从 Git 查看）（计划本体，留给用户处置，未被本分支提交）。
本分支的改动没有覆盖、重置或批量格式化任何用户未提交内容；旧实现**一行未删**（diff 只有 1 行删除，
是 `.gitignore` 首行的锚定改写）。

## 3. 审查材料

| §15 要求 | 在哪 |
| --- | --- |
| 旧→新映射与每项验收证据 | `docs/build/migration-inventory.md` |
| 删除台账与剩余 Python/Ninja 职责 | `docs/build/legacy-removal.md` |
| A01–A17 逐项结果、退出码、未运行理由 | `docs/build/verification.md` 第 5 节矩阵 + 第 4–10 节 |
| 新构建使用文档 | `make help`（`scripts/help.sh`）；仓库内 `docs/BUILDSYSTEM.md` 仍描述旧链（属 P4，见 `legacy-removal.md` 第 4 节） |
| 镜像/SDK 路径与哈希、实际启动过哪个 | `verification.md` 第 10 节：**新链未产出任何 SDK/镜像，也没有启动过任何哈希** |
| 性能记录（同宿主同配置） | `verification.md` 第 10 节；无同法干净基线，因此不报倍数 |
| 已知限制与未完成需求 | `verification.md` 第 9 节 + `migration-inventory.md` 第 6 节 |

## 4. 复跑方法（比留存日志更强：证据可再生）

```
make test          # C 单测（普通 + ASan/UBSan）+ 6 个 shell 契约套件，实测 rc=0，2:05
make test-long     # A08/A10 中断/A16 execve 跟踪，实测 rc=0，约 6 分
make doctor        # 宿主/目标工具链/锁文件一致性
make fetch         # 唯一联网入口；暖缓存时不联网
```

`make test` 的计数与退出码取自本轮实测：`test-tools` 4 次运行（`host/common` 74 项、`host/json`
66 项，各跑普通与 ASan+UBSan 两份）、`test-bootstrap` 22/0、
`test-concurrency` 17/0、`test-deps` 44/0、`test-incremental` 21/0、`test-reproducible` 11/0、
`test-third-party` 29/0；`make test-long`：`test-execchain` 20/0、`test-jobs` 19/0。
临时日志（`/tmp/leonos-*.log`）随进程目录消失，不作为证据引用——需要日志时重跑上面两条命令。

## 5. 请重点审的六处判断

1. **同 O 互斥为什么不用 `flock`。** GNU Make 4.4 不执行 `.EXIT`/`.STATUS`（正常退出与
   SIGINT/SIGTERM 三种情况实测均不运行），所以没有可靠的释放钩子可用；实现改成"owner 记录 +
   进程存活性 + 全序 `(启动时间, pid)` 裁决"。请审：pid 回收检测是否足够、被 OOM 杀掉后
   下一条 `make` 是否会被陈旧记录卡住（测试断言了不会），以及"豁免 `make -n`"是否可接受。
2. **签名（command signature）机制是否真的把增量交给了 Make。** 每条动作类一个 `.sig`，
   解析期 `$(file >)` 写 candidate、FORCE 规则 `cmp -s` 后 `mv` 提升，内容不变则不动 mtime。
   A05/A08 的度量是对象 mtime 集合，不是日志行数。请审这条链路在 `-j`、中断、以及
   "同一 O 被两个不同 flag 的进程共享"三种情况下的行为。
3. **sysroot 等价性只有 238/239。** 唯一差异是 `config.mak` 里的安装前缀路径；请确认这属于
   路径差异而非内容差异，以及是否接受"整锁摘要进签名"（改任何一条无关依赖会多重建一次 sysroot，
   换来的是不需要在 `leonos-deps` 建好之前解析 JSON）。
4. **libxcrypt 的一处有意偏离。** 上游 configure argv 与旧驱动逐条一致，只多了
   `-fmacro-prefix-map`：libxcrypt 把 `__FILE__` 编进 `.rodata`，不映射的话产物字节随
   `O=` 路径变化（旧系统就是这样），映射后两个不同深度的树构建出逐字节相同的库。
   请确认这是可接受的偏离而不是"改了第三方构建参数"。
5. **本轮我自己毁过一份基线产物。** 用 `llvm-objcopy -j .text --dump-section` 比较时漏了输出
   文件参数，就地重写了新旧两份 `libcrypt.so.2.0.0`；已用旧系统自己的 action 重建恢复，
   并把被它顺带跳号的两个受跟踪文件还原。过程与教训写在 `verification.md` 11.5。
6. **P1-d 仍是受阻而不是通过。** 新旧内核在 SeaBIOS 与 OVMF 下行为一致（都停在 GRUB 横幅），
   所以**没有回归证据**，但也**没有成功启动证据**。这是旧链的 ISO/引导问题，重建构建系统不会
   顺带修它；把它算作本分支的完成度是不诚实的，因此没有。

## 6. 剩余关键路径（下一步该怎么走）

**认证链是唯一的阻塞点。** `runtime` 的 `libleonos.so.2` 链接命令把 `libpam.so.0` 与
`libcrypt.so.2` 放在 `-lc` 之前（`build.py:1467-1471`），而 `userland/auth/*.c` 需要
`security/pam_appl.h`；SDK 又依赖 `archive:libc`；镜像依赖 SDK。因此：

```
P2-a3 Linux-PAM + libxcrypt + libbsd/libmd + util-linux/sudo/shadow 的 Makefile 移植
  └─> P2-a2 runtime（libleonos.so.2 / libleonos.a / installer 变体）
        └─> P2-a2 userland（62 个组件）  └─> P2 SDK（含 leonos-musl-cc 的 C 移植）
              └─> P3 资源/APK/rootfs/三类镜像 └─> P4 旧实现删除与 CI 切换 └─> P5 安装升级
```

Linux-PAM 1.7.2 上游只有 Meson（autotools 已在 1.6.0 移除），而用户裁定不开 Meson/Ninja 例外、
不降级、不删认证——所以"手写 Makefile 移植"是一次数小时的独立工程（`config.h` 的 HAVE_* 集合、
libpam/libpam_misc/libpamc 与模块、`faillock.c`/`opasswd.c`/`bigcrypt.c` 同名对象的多产物路径）。
它不适合塞进本审查包的尾巴上做半份实现，因此这里停住，等一次单独的阶段授权。
