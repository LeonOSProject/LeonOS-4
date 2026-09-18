# 旧实现删除台账

对应计划第 15 节要求的 `docs/build/legacy-removal.md`：**删除项及剩余 Python/Ninja 引用的职责说明**。
旧→新的映射在 `migration-inventory.md`，验收结果在 `verification.md`。

## 1. 当前状态

**本分支尚未删除任何一行旧实现。** 这是有意的：新链目前覆盖 `kernel`、host 工具、配置、
依赖锁与 musl sysroot，而 `userland/runtime/sdk/rootfs/apk-repo/三类镜像/installer` 仍是
`scripts/not-migrated.sh` 的 `exit 2`。在这些目标迁完之前删掉 `build.py` 会让产品失去
可用的镜像生产能力——计划第 3 节禁止"通过禁用失败组件让构建变绿"，同样禁止用删除
退路的方式逼迫进度。

截至本轮的仓库事实（`git ls-files` / `find` 实测）：

| 项 | 数量 | 说明 |
| --- | --- | --- |
| `build.py` | 4958 行，仍在 | 旧调度入口，功能完好 |
| `buildsystem/**/*.py` | 48 个文件 | 旧执行器/图/缓存层 |
| `tools/*.py` | 198 个（递归 200） | 旧 action 脚本与测试 |
| `.github/workflows/*` | 2 个工作流调用 `python3 ./build.py` | 见第 4 节 |

## 2. 已被新实现取代、但文件仍在的项

这些已经有等价的新实现和证据，删除只差"确认没有别处引用"：

| 旧文件 | 新的替代 | 证据 |
| --- | --- | --- |
| `tools/generate_boot_logo.py` | `tools/host/logo/leonos-boot-logo`（`make tools`） | 输出与旧脚本**逐字节一致**（`verification.md` 4.4） |
| `tools/kconfig_sync.py` | `tools/host/config/leonos-config` + `mk/config.mk` | `test-bootstrap.sh` 与 `make kernel` 的 A02 无操作证明 |
| 版本戳写入（`build.py` 直接改 `include/generated/build_info.h`） | `tools/host/version/leonos-version` 写到 `$(O)/include/generated` | A13 两棵树哈希一致；A17 受跟踪文件零改动 |
| 配置与内核编译调度（`build.py` 的 `musl`/`kernel` action） | `Makefile` + `mk/{host,toolchain,config,kernel}.mk` | A02–A07、A13、A16 |
| `buildsystem/**` 的 musl sysroot 驱动 | `tools/build/musl-sysroot.sh` + `mk/third-party.mk` | 238/239 文件逐字节一致（`verification.md` 6.3） |

## 3. 剩余的 Python / Meson / Ninja 引用，按职责归属

计划第 2 节的要求是**生产构建**不执行 Python/Meson/Ninja。下表是"谁负责让它消失"：

| 位置 | 现状 | 归属阶段 |
| --- | --- | --- |
| `tools/leonos_musl_cc.py`（40 行，被安装成 SDK 的 `bin/leonos-musl-cc`） | 违反约束：SDK 用户每次编译都会跑 Python | P2（SDK）：按 argv 改写逐条移植为 C，`userland/musl-gcc/launcher.c` 是可参考的现成模板 |
| `tools/package_musl_sdk.py:48-51` | 把上面那个 Python 包装器 `copy2` 进 SDK | P2（SDK） |
| `tools/package_devtools.py:407` | 第二条 SDK 路径，同样塞进 `leonos-musl-cc` | P2（SDK） |
| `build.py:1546` | 生产链接步骤直接 `exec` 该包装器 | P2（SDK） |
| `devtools/Makefile:6,9` | 已提交的 `CC := python3 "$(SDK_ROOT)/bin/leonos-musl-cc"` | P2（SDK），随包装器一起换 |
| `tools/rebuild_archive.py` | `ar` 重建到临时文件再改名；同时 `build.py:1426/1436/2114` 还在用裸 `ar rcs` 追加 | P2（runtime）：改成 `rm -f tmp && ar rcsD tmp && mv`，不需要新工具 |
| `tools/build_auth_upstream.py`（Linux-PAM/libxcrypt/util-linux/sudo/shadow） | 调 Meson/Ninja | **部分取代**：`linux-headers` 与 `libxcrypt` 已由 `tools/build/auth-upstream.sh` 接管（上游 configure，计划第 9 节允许），23 项契约见 `verification.md` 第 11 节；其余包（含 Linux-PAM，上游仅 Meson，需手写 Makefile 移植）仍在这个 Python 驱动里。用户裁定不变：不降级、不开 Meson/Ninja 例外、不去掉认证 |
| `tools/fetch_auth_upstream.py` | 旧的下载/解包 | 已被 `configs/dependencies.lock.json` + `tools/build/fetch.sh` 取代（P2-a 已交付），待删 |
| `tools/generate_gbk_table.py`、`tools/make_*.py`（图标/字体/清单） | 资源生成 | P3/P4：计划第 7 节要求可重现的 C 生成器 |
| `tools/apk_distribution.py:421`、`tools/make_image.py:114,128` | 包版本用 `time.time_ns()`、分区 UUID 用 `uuid.uuid4()` | P3：不先改成可派生值，A13 对镜像永远不可能成立 |
| `tools/make_ext2_root.py:20` | `fakeroot -- sys.executable <self>` 自举提权 | P3 |
| `build.py:2553-2660` `staging-prune` | 差集依赖上一次状态 | P3：成员清单 + 从空 staging 起步 + 原子发布 |
| `tools/*.py` 的自测脚本（如 `tools/test_component_config.py`、`tools/test_auth_sdk.py`） | CI 仍在跑 | 保留到其被测对象被替换，再随对象一起删 |

## 4. CI 为什么现在还不能切

`.github/workflows/build-installer.yml:145,187-201` 与 `.github/workflows/publish-rpr.yml:68-69`
显式执行 `python3 ./build.py run defconfig|image-vmdk|rpr-pages`。在这些工作流改成 `make`
目标之前删除旧入口，会让 CI 与发布管道直接失效。A17 的"CI/文档无活跃旧入口"因此**只能与
工作流改写同批交付**，不能靠先删后补。

`docs/BUILDSYSTEM.md` 仍是旧系统的说明文档；仓库根的 `AGENT.md`（33 处）与 `README.md`（23 处）
也在指导用户使用 `python3 build.py ...`。计划第 15 节要求的新使用文档已交付为 `make help`
（`scripts/help.sh`），但这些 Markdown 的替换属 P4，且必须与旧入口的删除同批评审——在那之前
它们描述的仍然是**唯一能出镜像的**那条链。

## 5. 删除顺序（P4 的可执行清单）

1. 新链能产出与旧基线**可比的**三类镜像与 SDK（A15 通过），且 A16 的 exec 跟踪覆盖到镜像链。
2. 逐条替换 `tools/*.py`：每替换一条就把该文件与它的调用点一起删，单独提交。
3. 换 CI：工作流改 `make`，观察一轮绿色后再删 `build.py`/`buildsystem/`。
4. 最后删 `build.py` 与 `buildsystem/**`，并把 `buildsystem/cache/` 的处置写进提交说明。

## 6. 明确禁止的"删除"

- **不得为了让 `grep -r python` 归零而删上游文件的构建脚本。** `buildsystem/cache/apk/**` 的
  APK `manifest.json` 里出现 `meson`/`ninja` 是上游包元数据；`build/linux-6.12/**`、
  `third_party/**` 里的 `meson.build` 是上游源码。计划第 3 节把这些排除在"零命中"之外。
  A16 的正确度量是 **execve 跟踪**（已实现：`tests/long/test-execchain.sh`），不是字符串搜索。
- 不得删除还承载未迁移功能的旧 action 来让新链"看起来完整"。
- 不得把旧入口包一层 `make` 壳留在仓库里冒充迁移完成。
