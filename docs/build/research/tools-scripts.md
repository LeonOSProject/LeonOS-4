# tools/ 生产脚本事实底稿（P0-a 迁移台账）

生成时间基线：工作区 `2026-09-19` 状态。盘点对象 = `tools/` 下所有 **非 `tools/test_*.py`** 的 `.py`（71 个）+ 非 Python 生产件（`oschinpt-apk-post-*`、`provision_rpr_signing_key.sh`）+ 子目录（`tools/tests/`、`tools/abi/`、`tools/browser_test_page/`、`tools/vscode/`）。

> 计数说明：任务书说 69 个生产脚本；实测 `find tools -name '*.py' ! -name 'test_*.py'` = **71**（含 `tools/vscode/` 下 2 个）。`test_*.py` = 129。本文按 71 个盘点。
> 行号引用格式：`文件:行`。`build.py` 行号取自当前工作区副本（238201 字节）。

## 0. 全库事实（适用于所有表格）

| 事实 | 证据 |
| --- | --- |
| 生产脚本 **零 Python 第三方依赖**，全部 stdlib（`tomllib`、`sqlite3`、`http.server`、`zipfile`、`tarfile`、`fcntl`、`struct`、`zlib`） | 逐文件 `import` 扫描；仅 `tools/test_apiapp_qemu.py`、`test_hyfetch_qemu.py`、`test_installer_responsiveness.py`、`test_musl_gcc_iso.py`、`test_network_qemu.py`、`test_installer_mode_switch_qemu.py` 用 `PIL` |
| `pyproject.toml` 的第三方依赖（PySide6/unicorn/pyelftools/capstone）只属于 `los2w`（Windows 运行器），与 `tools/` 无关 | `pyproject.toml:29-34` |
| 生产入口唯一：`python3 build.py` → `PYTHON = sys.executable`；build.py 以 argv 调用 `tools/*.py` | `build.py` 全文 `(PYTHON, "tools/...")` |
| build.py 还 **import 两个 tools 模块作为 Python 函数库**（不是子进程） | `build.py:64` `from tools import leonos_layout as layout`、`build.py:65` `from tools import musl_link`、`build.py:788` `from tools.storage_tools import ...`、`build.py:901` `from tools.package_fastfetch import CACHE` |
| 宿主外部程序权威清单（按 target 分组，等价于新 `doctor`） | `build.py:4155-4203` `task_tools()`：`clang ld.lld llvm-ar autoreconf autoconf automake libtoolize make gcc g++ gperf flex bison rustc grub-mkfont grub-mkstandalone grub-mkrescue truncate mkfs.fat mcopy mke2fs dd qemu-img qemu-system-x86_64 xorriso openssl` |
| CI 显式安装 `ninja-build` 与 `meson`（为 linux-pam），另装 `python3`/`python3-pil` | `.github/workflows/build-installer.yml:60-91` |
| CI 调用点：`build.py run defconfig` / `image-vmdk` / `image-iso` / `installer` / `info`，直接 `python3 tools/test_component_config.py`，`tools/count_code.py` | `build-installer.yml:145,187-203`；`code-count.yml:35`；`publish-rpr.yml:68-69` |
| 被 Python 生成并 **受 Git 跟踪** 的文件（§7 禁止"提交结果省略再生路径"的现有违例） | `include/generated/autoconf.h`、`autoconf-installer.h`、`build_info.h`、`rustcfg.args`、`include/uapi/linux/syscall.h`、`Kconfig.components`、`docs/ABI_MIGRATION.md` |

### 0.1 词典序 / /locale 相关产物清单（迁移到 C 时必须显式约定字节序 collation）

| 脚本:行 | 排序点 | 影响的产物 |
| --- | --- | --- |
| `apk_ownership.py:113` | `sorted(entries, key=path)` | `apk-ownership.json` 清单、APK 分组、缓存键 |
| `apk_distribution.py:72,515` | `sorted(root.rglob("*"))` 进 sha256 | `tree_digest()` / `repackage_tree.fingerprint()` → `buildsystem/cache/apk/distributions/<digest>`、`.repackage-cache/<digest>` 目录名 |
| `apk_distribution.py:200,202,434,480` | `sorted(depends)`、`sorted(provides)`、`sorted(groups)` | 写入 `.APKINDEX`/包体、`apk add` argv 顺序 → 安装数据库顺序 |
| `package_musl_gcc.py:63` / `package_python.py:72` | `sorted(tree.rglob("*"))` | `.leonos-package.json` 的 `upstream_files` 映射 |
| `kconfig_sync.py:153` | `sorted(values)` | `autoconf.h` / `autoconf-installer.h`；`kconfig_sync.py:137` 保留 defaults 文件出现顺序 → `.config` 顺序 |
| `generate_component_kconfig.py:43` | `sorted(categories)` | `Kconfig.components`（Git 跟踪） |
| `build_rpr_pages.py:60` | `json.dumps(sort_keys=True)` | RPR 索引/清单 |
| `package_devtools.py:16,45,59` | `ZIP_TIMESTAMP=(1980,1,1)` + 排序遍历 | SDK zip 字节可重现 |
| `make_oschinpt_index.py` | 按字典文件 **原始行序** 生成偏移 | 索引必须与二进制查询侧一致，不可排序 |
| Python `sorted()` 语义 = Unicode 码点序 | — | C 侧必须用 `memcmp` 字节序，不能用 `strcoll`/locale |

---

## 1. musl / 工具链 / SDK（计划 §2 重点）

### 1.1 sysroot 与链接顺序事实

**sysroot 布局**（`build.py:766` `musl_prefix = out/musl/sysroot`，产物清单 `build.py:768-775`）：

```
out/musl/sysroot/
  .leonos-musl.json          {"sources":{musl,mimalloc commit},"patches":[{path,sha256}],"target":"x86_64-linux-musl"}
  include/                   musl 头 + 手工补 pty.h/arpa/inet.h/... (build.py:792-797) + mimalloc.h
  lib/libc.so                真实 ELF = musl 动态加载器（PT_INTERP /lib/ld-musl-x86_64.so.1）
  lib/libc.a libm.a libdl.a libpthread.a libresolv.a librt.a libutil.a libxnet.a libcrypt.a
  lib/crt1.o Scrt1.o rcrt1.o crti.o crtn.o
  lib/mimalloc.o             静态 PIC 目标（-static 时直接进链接）
  lib/libmimalloc.so.3       ld.lld -shared -soname libmimalloc.so.3
  lib/libssp_nonshared.a     由 userland/musl-dev/stack_chk_fail_local.c 现编译
  share/licenses/{musl,mimalloc}/COPYRIGHT|LICENSE
```
`make_musl_checkpoint`/`musl_development_payload` 用 `lib/ld-musl-x86_64.so.1` 与 `include/crypt.h` 字节比对校验 sysroot 与镜像一致（`apk_distribution.py:225-229`）。

**链接顺序权威**（`tools/musl_link.py:6-30`，被 `build.py:65` import 后直接生成 argv）：

| 模式 | argv 顺序 |
| --- | --- |
| PIE 可执行 | `ld.lld --gc-sections -z max-page-size=0x1000 -pie --hash-style=both --dynamic-linker /lib/ld-musl-x86_64.so.1 -rpath /usr/lib/leonos:/lib:/usr/lib -o OUT Scrt1.o crti.o <objs> -L LIB -l:libmimalloc.so.3 --start-group <libs> -lc --end-group crtn.o` |
| 静态可执行 | `... -static --image-base=0x4000000 ... crt1.o crti.o <objs> -L LIB <lib>/mimalloc.o --start-group <libs> -lc --end-group crtn.o` |
| DSO | `ld.lld -shared --no-undefined --hash-style=both -z max-page-size=0x1000 -soname N <objs> -L LIB -l:libmimalloc.so.3 <libs> -lc`（无 crt1/crti/crtn，无私有 ABI note） |

`--start-group ... -lc --end-group` 与 `crti.o/crtn.o` 括号顺序是硬约束，禁止 Make 侧对库列表排序。

### 1.2 逐项表格

| 文件:行数 | 职责 | 调用者 | 输入 → 输出 | 外部程序 / 读 Git / 联网 | 迁移归属 + 理由 |
| --- | --- | --- | --- | --- | --- |
| `build_musl.py` (105) | 构建钉版 musl + 未改 mimalloc，产出 sysroot 与 `.leonos-musl.json` 戳 | `build.py:774,781`（target `musl`） | 读 `third_party/musl`、`third_party/mimalloc`、`patches/musl/0001-enforce-password-file-lock.patch`、`userland/musl-dev/stack_chk_fail_local.c` → 写 `out/musl/{build dir}`、`out/musl/sysroot/**`、`out/musl/sources/<patch-sha>/musl` | `git rev-parse HEAD`（校验两仓 commit，行 38-41）、`git archive`（行 51）、`patch -p1 --batch --forward`（行 56）、musl `./configure --target=x86_64-linux-musl --disable-gcc-wrapper --syslibdir`、`make -j`、`make install`、`clang`、`llvm-ar`、`ld.lld`；**不联网** | **(d)+(c)**：musl 本身是 autotools，Make 递归 `$(MAKE)` 即可；补丁应用（git archive → tar 解包 → `patch` → 摘要寻址缓存目录）用 `tools/build/musl-patch.sh` 或一个小 C 工具。**必须保留** `make clean`-当补丁摘要变化 的语义（行 66-67）与 stamp JSON（是下游 `auth-upstream`/`storage-upstream` 的输入校验）。不可用 shell 拼 `CC="clang --target=... -fuse-ld=lld"` 之外的隐式 host include（行 68-71 注释明确"不使用宿主 include/libc"） |
| `musl_link.py` (30) | 生成 musl CRT/mimalloc 链接 argv 的纯函数库 | `build.py:65` import；`build.py:1494,1509` 直接作为 command；被 `build_cmd/less/lua/nano/pleditor/portablegl/sl/sqlite/file/build_tcc/package_python` import（各 `:13/:21/:11/:18`） | 无 IO | 无 | **(b) 新 C 工具 或 Make 变量模板**。它是 30 行的顺序契约，§6.1 要求"链接顺序显式定义"。建议 `mk/toolchain.mk` 里用显式 `LINK.executable/LINK.shared` 模板 + 一个 C 辅助仅当需要 argv 转义；**不可删**：删掉等于让 Make 随手排序 `-lc/--start-group` |
| `leonos_musl_cc.py` (40) | SDK 编译器驱动包装器（可移动前缀）：`--target=x86_64-linux-musl -fuse-ld=lld -nostdlib -nostdinc -isystem <sdk>/include -isystem <clang resource>/include -D_GNU_SOURCE -DLEONOS_USE_MUSL -mno-avx -mno-avx2`；非编译模式追加 `Scrt1.o|crt1.o crti.o`、`-pie`/`--image-base`、`-Wl,--dynamic-linker=/lib/ld-musl-x86_64.so.1`、`-Wl,-rpath,/usr/lib/leonos:/lib:/usr/lib`、`-l:libmimalloc.so.3 -l:libleonos.so.2 -lc`、`crtn.o` | `build.py:1515`（inputs 声明）；**被复制成 SDK 可执行**：`package_musl_sdk.py:49-51` → `<sdk>/bin/leonos-musl-cc`，`package_devtools.py:407` → SDK zip `bin/leonos-musl-cc`；**生产构建链直接执行**：`build.py:1546` 用 `out/musl/sdk/bin/leonos-musl-cc` 作为 `musl-probe:*` 的 link command | 读 `argv`、`$LEONOS_CC`（默认 `clang`）、`clang -print-resource-dir` | `clang`（subprocess.call/check_output）；无 Git；无网络 | **(b) 必须重写为 C 或短 Shell —— 最高优先级违例**。计划 §2 "执行链不运行 Python，**包括生成的包装器**"：当前 SDK tarball、SDK zip、以及 `musl-probe` 链接步骤三处都跑 Python。§10 要求"安装前缀可移动，不能嵌入开发者绝对工作区路径"。现成范式：`userland/musl-gcc/launcher.c`（读 `/proc/self/exe` 推导根、按 argv[0] 派生命令、`-idirafter` 顺序）与 `userland/python/launcher.c`。注意保留行 29-30 注释的 `-x none` 修正（否则调用者的 `-x c` 会作用到 `crtn.o`），以及行 12-13 对 `--version/-dumpmachine/-dumpversion/-print-resource-dir` 的直传短路 |
| `make_musl_checkpoint.py` (63) | 在已装 root 镜像副本上追加 ABI probe / LTP 测试文件（诊断 checkpoint） | `build.py:3503,3505`（target `musl-installer-*` checkpoint root） | 读 `--base`(root.fat)/`--musl/tests/musl-abi-{dynamic,static}.elf`/`--ltp/*.elf` → 写 `<stage>/install/root.fat` | 偏移 1080 读 2 字节魔数 `53 ef` 判 ext2（行 32-34）；`debugfs -w -R mkdir/write`（ext2 分支）或 `mmd`/`mcopy -o -i`（FAT 分支） | **(d) 直接用 debugfs/mtools**：整脚本只是"按镜像类型选工具追加文件"，`tools/build/checkpoint-root.sh` 足够；不写 Python 版 FAT/ext2 逻辑（§3） |
| `package_musl_gcc.py` (109) | 解包并校验钉版 Dyne gcc-musl 15.1.0 静态工具链，产出 `musl-gcc/root` + `.leonos-package.json` | **无构建图调用者**。仅 `userland/musl-gcc/README.md:25` 手动命令 + `tools/prepare_gcc_probe.py:29` `from package_musl_gcc import COMMANDS` | 读 `buildsystem/deps/musl-gcc/dyne-gcc-musl-x86_64.tar.xz` → 写 `out/musl-gcc/root/{opt/dyne,usr/bin 别名,usr/share/licenses/musl-gcc,usr/share/examples/musl-gcc}` | `curl`（`ARCHIVE_URL`，行 38，联网）、`tarfile`、`readelf -l -d` 断言无 INTERP/NEEDED（行 65-67）、`x86_64-linux-musl-gcc` 编译 `userland/musl-gcc/launcher.c` | **(d)+(e)**：musl-gcc 已从生产镜像退役（`leonos_layout.RETIRED_TOOL_PATHS` 含 `opt/dyne`，`build.py:2557` 清理；`build.py:146-147` 仅剩 `gcc-probe-*` 名字残留）。下载/摘要/解包属 `make fetch`（§9）+ 一个 `unpack-verify` C 工具；`COMMANDS` 清单迁成 `configs/*.txt` 数据文件，避免为了 README 保留整个 py |
| `package_musl_sdk.py` (58) | 打包 tar.gz 版 musl SDK（include/lib/licenses + `libleonos.so.2/.a` + 可选 ncurses + zlib/libpng 头） | `build.py:1515,1520`（target `musl-sdk`） | 读 `sysroot/{include,lib,share/licenses}`、`include/uapi/**`、`include/leonos/**`、`userland/libc/include/leonos/**`、`third_party/{zlib,libpng}/*.h`、`--png-config`、`--runtime`(libleonos.so.2)、`--archive`(libleonos.a) → 写 `out/musl/sdk`、`out/musl/leonos-musl-sdk.tar.gz` | 无外部程序；**拷贝 `tools/leonos_musl_cc.py` 为 `bin/leonos-musl-cc`（行 49-51）**；拒绝 `stage` 与源码根/sysroot 相同（行 23-24） | **(a) Make 规则 + 一次 cp/tar**，唯一非平凡点是 §1.2 的包装器替换。头文件优先级契约（行 31 注释："UAPI 拥有 wire 定义，libc 拥有标准 C/POSIX 声明"）与 `--png-config` 必须写成显式清单，不能靠 rglob 顺序；`tarfile(dereference=True)` 会把 SDK 内符号链接实体化，迁移时要显式决定 |

### 1.3 该组最难的迁移点（事实依据）

1. `leonos_musl_cc.py` 的 Python 侵入面是 **三处**（SDK tarball、SDK zip、`musl-probe` 链接命令），且它内含与 `musl_link.py` 重复的 CRT/`--start-group` 顺序知识 → 两处必须同时改，否则签名/ABI 漂移。
2. `build_musl.py` 用 `git archive <rev>` + `patch` + "补丁 sha256 作目录名 + `verify_source_tree` 反向比对" 实现"上游树不被污染 + 补丁变更触发重建"（行 43-62、`fetch_auth_upstream.py:26-65`）。§6.2 要求"补丁是显式输入"，§9 要求"补丁在配置专属工作目录应用" → 需要一个新的 `patch-tree` C 工具或 Make 规则来复现摘要寻址，不能简单 `patch` 到共享源码树。
3. `musl_link.py` 是 **被 import 的函数**，其 argv 直接成为 Make 图里的 link 命令。迁移时若只搬 `str()` 拼接而丢掉顺序，增量正确性测试（A05/A06）会假绿。

---

## 2. APK / OpenRC / 存储包（§2、§10）

| 文件:行数 | 职责 | 调用者 | 输入 → 输出 | 外部程序 / 联网 | 迁移归属 + 理由 |
| --- | --- | --- | --- | --- | --- |
| `apk_distribution.py` (561) | 下载并 openssl 验签 `apk-tools-static`，用上游 `apk mkpkg/mkndx/add` 造签名本地仓库与"被 apk 真正管理"的 rootfs；同时造 `leonos-musl-dev` 包 | `build.py:3284,3290,3310,3341,3401`（target `apk-root`）；被 `build_rpr_apps.py:10`、`build_rpr_pages.py:16` import `bootstrap/make_package/signing_key`；被 `make_installer_root.py:236` import `repackage_tree`；`make_live_root` 经 `make_installer_root` 间接 | 读 `--source`(staging)、`configs/apk-ownership.json`、`configs/components.toml`、`configs/openrc-packages.json`、`system/rootfs/etc/apk/{repositories,keys/**,protected_paths.d/leonos.list}`、`userland/storage/{leonos-apk-update,busybox-binutils-links}`、`boot/grub/*` 等 → 写 `out/apk/root/**`、`out/apk/work/manifest.json`、`repository/packages.adb`+`*.apk`、缓存 `buildsystem/cache/apk/{distributions/<key>/root,manifest.json,downloads/<sha256>}`、`<stage>/../.repackage-cache/<fingerprint>/` | `unshare -Ur`（优先）否则 `fakeroot`（行 39-61）；`apk.static`；`openssl dgst -verify / genpkey RSA2048 / pkey -pubout`；`curl`（`URL`=清华镜像 apk-tools-static 3.0.8、`LICENSE_URLS`）；`readelf -dW` 抽 NEEDED/SONAME；`bin/busybox --list` 断言 applet | **(d) 保留 apk/openssl 为执行者 + (b) 少量 C 辅助**。§3 明确"不重写 APK 签名算法"，签名/索引/事务必须由 `apk mkpkg/mkndx/add --initdb` 完成。需要迁移的是"编排"：(i) `bootstrap()` 的下载+双摘要+tar 抽取+锁 → 归 `make fetch`；(ii) `tree_digest`/`repackage_tree.fingerprint` 与 `inventory` 排序 → 与 §2 的清单工具合并；(iii) ELF NEEDED/SONAME  provider 解析（行 385-400）→ 新 C 工具（复用仓库内 `readelf` 或已有 ELF 解析），**注意** "multiple ELF providers" 与 "unresolved real ELF dependency" 两条错误是包图正确性的守门人，不能空实现。**违例记录**：行 421 `version = f"0.{time.time_ns()}-r0"` 使包版本每次不同，直接违反 §10 的固定 epoch 要求；行 92 `use_cache = destination.is_relative_to(ROOT)` 把"是否在工作区内"当缓存开关，`O=` 在工作区外会静默丢缓存 |
| `apk_ownership.py` (134) | 归属策略加载 + 全树清单（file/symlink/size/sha256/mode/group/classification），明确声明"不是已安装 APK 记录" | `build.py:3260,3263,3285`（target `esp:apk-ownership`，产 `ownership_report`）；被 `apk_distribution.py:19` import `ROOT/inventory/load_policy` | 读 `configs/apk-ownership.json`、`configs/components.toml`(tomllib)、staging 树、`include/uapi/leonos/rootfs.h`（经 leonos_layout） → 写 `out/generated/apk-ownership.json`（`indent=2, sort_keys=True`） | 无外部程序；不联网；不读 Git | **(b) 新 C 工具**。核心是"路径前缀最长匹配 + 版本化 .so 回退 + guest 内符号链接词法解析（不跟随宿主符号链接）"（行 61-104），语义精确且已被 `test_apk_ownership.py`/`test_apk_layout.py` 钉住。策略校验（`version==1`、`apk_registration=="not-installed"`、component 集合全等、路径禁 `..`/绝对）必须在 C 里逐条保留（§10 "重复目标默认报错"，行 33-34 的 `ambiguous ownership` 就是那条守门规则）。JSON 解析需按 §5 用固定版本库，不许手写切片 |
| `openrc_packages.py` (39) | 下载+签名校验+清单一致性校验钉版 Alpine APK（openrc/openrc-user/libcap2/busybox-openrc/findutils），并收集其成员路径供归属避让 | 仅被 `apk_distribution.py:248` 调用；`build.py:3286` 作为 target input 列出 | 读 `configs/openrc-packages.json`、`system/rootfs/etc/apk/keys` → 写 `buildsystem/cache/apk/packages/<name>-<ver>.apk` | `apk verify`（真验签，非 allow-untrusted）、`tarfile` 读 `.PKGINFO`；`curl`（经 `download_verified`） | **(c) 短 shell + 复用 apk**：`apk verify` + `curl` + 归档成员/`.PKGINFO` 名字检查。冲突检测（行 35-37 "Conflicting Alpine payload"）与绝对路径/`..`/特殊成员拒绝（行 31-34）要保留，正是 §9 的解包防御要求。索引是可变内容 → §9 要求"不伪装可重现输入"，这里已经钉了逐包 sha256，属正面基线 |
| `storage_tools.py` (34) | 存储命令/库/兼容符号链接的 **常量表** + `stage_filesystems()`（缺任一 `mkfs/fsck.*` 即拒绝整包） | `build.py:788` import；`build.py:817,2809,3261` input；`apk_ownership.py:16` import | 读 storage package 树 → 写 staging（copytree，忽略 `.storage-package.json`） | 无外部程序（`mount`/`mkfs.*` 仅出现在 docstring 附近的字符串常量） | **(a) 数据表迁 configs，(c) 短 shell 做 stage**。表格（`FORMATTER_COMMANDS`/`UTIL_LINUX_COMMANDS`/`UTIL_LINUX_LIBRARIES`/`COMPAT_LINKS`）是 rootfs 清单（§10）的天然输入，应进 `configs/` JSON 或 `mk/*.mk` 列表；"齐全才落地"检查是几行 shell。**不要**在 C 里重造 4 个 fs 的名字列表 |
| `oschinpt-apk-post-install` (63, sh) | APK post-install 钩子：把 oschinpt 输入法 provider 注册进 `settings.ini`（幂等、按 owner/group 与 mode 0644/0755） | `build.py:3311`、`build_rpr_apps.py:141` 作为包脚本传入 | 读/写 guest `/usr/lib/leonos/apps/oschinpt/settings.ini` | 纯 sh（`grep`/`awk`/`install`） | **(f) 原样搬**：它不是构建产物而是 guest 生命周期脚本，只出现在 rootfs/APK payload 清单中。计划 §3 说"不把所有 Python 当旧残留"，同理不该重写已工作且被 `test_apk_*` 覆盖的 sh |
| `oschinpt-apk-post-deinstall` (72, sh) | 卸载时按 `providerN_id=oschinpt` 摘除条目并保留其余 provider | `build.py:3312`、`build_rpr_apps.py:142` | 同上 | 纯 sh（`awk` 重写、`$$` 临时文件 + `mv`） | **(f) 原样搬**（同上） |

---

## 3. 镜像与 rootfs 组装（§2、§10）

| 文件:行数 | 职责 | 调用者 | 输入 → 输出 | 外部程序 | 迁移归属 + 理由 |
| --- | --- | --- | --- | --- | --- |
| `make_image.py` (298) | 造 GPT 磁盘（保护 MBR + 主/备头与条目表 + CRC32）、ESP FAT32、ext2 root，最后 `qemu-img convert` 成 VMDK | `build.py:3367,3369,3381,3411,3526`（target `image-vmdk`） | 读 `--esp-tree`（staging）、`build/images/{esp.fat,root.ext2}` → 写 `<out>.raw`、`.vmdk`、`esp.fat`、`root.ext2`（全部临时文件 + `os.replace` 原子发布）；写 guest `etc/fstab`（行 159-170）；`image_lock` 用 `.<raw>.lock` flock 拒并发（行 65-82） | `truncate`、`mkfs.fat -F 32 -s 2 -n LEONOS4ESP`、`mcopy -s -i`、`dd bs=512 seek=N conv=notrunc`、`e2fsck -f -n`、`qemu-img convert -f raw -O vmdk`；**不联网** | 分两块：**(a) FAT/ext2/QEMU 部分交给既有工具**（mke2fs/mtools/xorriso/qemu-img，§3 禁重写格式工具、§10 明列这些工具）；**(b) GPT 写表部分需要决策**：当前 45 行 `struct.pack` + `zlib.crc32` 手写 GPT。§3 禁的是 ELF 链接器/APK 签名/FAT-ext2-ISO 工具，**GPT 不在禁改清单内**，而 `sfdisk`/`sgdisk` 是成熟外部程序（CI 已装 `gdisk`）。推荐用 `sfdisk --json` 脚本化（(d)），并保留 128 条目/2048 对齐/`LEONOS4_ESP`+`LEONOS4_ROOT` 名字与 fstab 的 `PARTUUID` 契约。若坚持自研则用 C 工具（现成 fixture：`tools/tests/rootfs_gpt_test.c`）。**违例记录**：行 114/128 `uuid.uuid4()` 每次生成新 disk/partition GUID → §10 要求固定 UUID/volume ID，A13 目前不可能通过；尺寸下限校验（行 234-235、256-257：ESP≥128MiB、root≥128MiB/262144 扇区）必须原样进 `mk/images.mk` 断言 |
| `make_ext2_root.py` (63) | 用 fakeroot 自举 + `mke2fs -d` 把目录树发布为 root 属主 ext2；容量/inode 自适应 | 被 `make_image.py:26`、`make_installer_root.py:12`、`make_live_root.py:10` import；`build.py:3367,3381,3411,3526` input；自身 `__main__` 是 fakeroot 内的第二次入口 | 读 staging 树 → 写 `<out>`（临时目录内造再 `replace`） | `fakeroot -- python3 make_ext2_root.py --populate ...`（行 20-22，**自调用 Python 一次**）、`mke2fs -q -t ext2 -F -b 4096 -I 128 -O none,filetype,sparse_super,large_file -m 0 -E root_owner=0:0 -N <inodes> -d <stage>`、`e2fsck -f -n` | **(c) 短 shell**：`fakeroot sh -c 'chown -R 0:0 <stage> && mke2fs ... -d <stage>'` + `e2fsck` 完全等价，且消掉"父 py 起子 py"这一 §2 违例。**必须保留**：`-O` 特性串、`-I 128`、`-b 4096`、`-m 0`、`root_owner=0:0`、inode 数 `max(8192, entries*2)`，以及行 41-46 的容量估算（对 hard link 去重后 `unique` 计 4K 块 + 512/条目 + 4096/目录 + 64MiB，向上取 32MiB 粒度）——这是镜像尺寸基线（§10"安装器尺寸必须与基线一致"）的一部分。`apply_test_home_ownership` 的 uid/gid 归属要落到 rootfs 清单（§10 uid/gid 字段） |
| `make_installer_root.py` (245) | 造安装器 live ext2 root：安装器专有程序（imd/windowd/desktop/installer/gptinit）+ 安装 payload（`install/root`、`install/esp`）+ 已装策略 runtime 覆盖 + APK 重打包 + 相同文件硬链接去重 | `build.py:3380,3408,3419,3525`（target `installer-root`） | 读 `--esp-tree`、`--userland-dir/*.elf`、`--gptinit`、`--installed-policy-dir/{desktop,settings}.elf`、`--policy-apps`、`--generated-icons-dir`、`docs/ADVANCED_INSTALL.txt` → 写 `--stage` 与 `--out`(root.fat，实为 ext2) | 经 `make_ext2_root`/`apk_distribution.repackage_tree`；无自身外部程序 | **(a)+(c) 混合**：payload 布局是清单驱动（§10 rootfs manifest：type/source/guest path/mode/uid/gid/owner component），应写成 `configs/rootfs-installer.json` + Make 规则。三处必须原样搬的语义：(i) `policy_apps` 白名单只允许 `desktop|settings`（行 220-222）；(ii) `install/root` 与 live root **同一命名空间、去掉 gptinit**、去掉 `license.conf`/`install.id`（行 228-229）；(iii) `share_identical_payload_files()`（行 81-105）按 (size,mode,uid,gid,sha256) 做**硬链接去重**，只对常规文件、绝不对符号链接去重 —— 这是镜像尺寸基线的主要来源，需要一个 C 工具（去重是数据转换，符合 §8），不能用 `cp -al` 近似 |
| `make_installer_iso.py` (160) | 造 UEFI(+可选 BIOS) 混合安装 ISO：`grub-mkstandalone`、FAT16 efiboot.img、`xorriso -as mkisofs` | `build.py:3387,3388,3456,3459,3510,3511`（target `installer-image`，同时用于 `musl-installer-*` ISO 与 live ISO） | 读 `--loader/--kernel/--middlelayer/--installer-root/--grub-font/--grub-config(boot/grub/installer.cfg)`、`boot/grub/installer_embedded.cfg`、`boot/grub/theme/theme.txt`、`/usr/lib/grub/{x86_64-efi,i386-pc}` → 写 `--out` iso、`--boot-image`(efiboot.img)、`--stage` | `grub-mkstandalone -O x86_64-efi --modules=<21 模块>`、`grub-mkimage -O i386-pc`、`cdboot.img+core.img` 手工拼 eltorito.img（行 136）、`truncate`、`mkfs.fat -F 16 -n LEONOSINST`、`mcopy -s -i`、`xorriso -as mkisofs -iso-level 3 -R -J -V LEONOS4INST [-b boot/grub/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot] -e boot/efiboot.img -no-emul-boot` | **(a) 纯 Make/shell**：这是外部程序薄封装，§3 禁重写 ISO 工具。需要保留的启动参数契约：`GRUB_MODULES` 全串（行 11-14，21 个模块，含 `multiboot2 gfxmenu gfxterm font search_fs_file`）、`.bios` 开关（可选安装器参数，§P4 要求 CI 不遗漏）、`LEONOSINST` 卷标、efiboot.img 尺寸 = payload+8MiB 且 ≥16MiB（行 75-79）、以及行 134-135 注释的理由（宿主 `grub-mkrescue` 的 EFI 与仓库验证过的模块可能不同，因此固定用 standalone image）。非确定字段：xorriso ISO 内嵌时间戳需 `--modification-date=`（当前未设 → §10/A13 缺口） |
| `make_live_root.py` (41) | 把正常桌面 payload 打成可引导 ext2 ramdisk（live ISO 的 root） | `build.py:3379,3383,3525,3528`（target `desktop-live-root`、`musl-desktop-vim-root`） | 读 `--tree`(apk_root 或 staging) → 写 `--out`；固定生成 `usr/lib/leonos/osmlayer.manifest`（`name=osmlayer abi=2 root=/ fs=ext2 gui=desktop.elf`） | 同上（复用 make_image/make_ext2_root/make_installer_root） | **(a)+(c)**：只有 15 行有效逻辑，真正价值是"live 与已装系统用完全同一棵树"（`make_root_tree`）+ manifest 常量。manifest 文本与 `build.py:2880` `esp:manifest` 的 `action_key="manifest-v6-ext2"` 字符串必须一起迁（三处硬编码：`build.py:2880`、`make_live_root.py:22`），建议在 `mk/rootfs.mk` 定义一次 |
| `populate_exfat.py` (487) | 用户态 exFAT 写入器（位图/FAT 链/目录集/NoFatChain/upcase 哈希），不挂载、不需 root | **无生产调用者**。唯一引用是 `tools/test_make_image.py:25` `from populate_exfat import ExfatVolume`。生产 root 已是 ext2（`make_image.py:218` `--root-fs choices=("ext2",)`） | 读 mkfs.exfat 产物 + staging 目录 → 就地写镜像 | 无（纯 `struct`+`os.pread/pwrite`） | **(f) 可删除（连同 `test_make_image.py` 的 exFAT 用例）**，或 **(e) 保留在 test-legacy** —— 二选一并写进 `legacy-removal.md`。理由：§3 明确"不重写 FAT/ext2/ISO 格式工具"，而这个脚本正是自研 FAT 写入器，与既定方向相反；且无生产调用者。若要保留 exFAT 根的能力，应改用 `mkfs.exfat` + `mtools`/guest 侧写，而非维护 487 行宿主格式实现 |

### 3.1 相关支撑件

| 文件:行数 | 职责 | 调用者 | 迁移归属 + 理由 |
| --- | --- | --- | --- |
| `leonos_layout.py` (344) | guest rootfs 布局唯一权威：目录/符号链接表从 **`include/uapi/leonos/rootfs.h` 的 `X("path",0mode)` 宏用正则反解**（行 87-94），加路径常量、`tool_payload_paths`、`builtin_command_links`、`RETIRED_TOOL_PATHS` | `build.py:64` import；`build.py:2654,2702,3252,3260,3368` input；`apk_distribution/apk_ownership/make_image/make_installer_root/make_live_root` import | **(a) 生成 Make/C 可直接消费的清单，删除正则反解**。它是 §10 rootfs manifest 的半成品（有 guest path+mode+link，缺 source/uid/gid/owner 逐条目）。当前"用正则读 C 头"既脆弱又是唯一入口，违反 §5"禁止用正则冒充完整解析器"。正确做法：让 `rootfs.h` 由一个 C 生成器/`conf2manifest` 工具从同一数据源产生，Python 侧消失。文档注释（行 1-30）与 `docs/ROOTFS_LAYOUT_AND_MIGRATION.md` 是权威行为说明，必须同步迁移而非重写 |
| `image_test_accounts.py` (60) | 独立 ISO/VMDK 预置公共测试账号（`system/test-accounts/{passwd,shadow,group,gshadow}` + `etc/leonos/test-image` 标记）；**明确不用于安装器** | `make_image.py:27` `seed_test_accounts`；`make_ext2_root.py:10` `apply_test_home_ownership`；`build.py:3363,3410` input | **(a) Make 规则 + (e) test-legacy**。产物含明文口令散列与 uid/gid 归属，属 §10 rootfs 清单的 file+uid/gid 条目；不要做成"构建脚本内嵌凭据"。验收靠 `tools/test_image_accounts.py`（现有覆盖，需登记） |
| `make_musl_checkpoint.py` / `rebuild_archive.py` (23) | `ar` 前先删旧 archive 再 `rcs` 全量重建，保证"删掉的 .c 不再残留可链接成员" | `build.py:1475,1476,1497,1498`（target `archive:libc`、`archive:installer-libc`） | **(a)**：Make 里 `rm -f $@ && ar rcs $@ $(OBJS)` 即等价，且正是 §6.1"静态库重建到新临时文件，不对旧 archive 追加"的实现。可删（P4），前提是新规则用临时文件+rename 且 `.DELETE_ON_ERROR` |
| `qmp_terminal_smoke.py` (437) | 经 QEMU QMP unix socket 发 `human-monitor-command`，驱动来宾终端/编辑器/fastfetch/sl/less/cmd/vim/ISO9660/abitest，用 `screendump`+PPM 差分判定，`--abittest`/`--tcc` 等模式 | `build.py:3893`（`test-qmp-*` 的 smoke_command）、`build.py:4043-4088`（12 个 target inputs） | **(e) 保留并列入 `test-legacy`**（§2"已有独立 Python 回归测试可保留在显式 test-legacy 目标，并列出依赖"）。它属于测试/QEMU 体验而非生产构建链，不违反"执行链不跑 Python"。依赖：stdlib socket/json，外部 `qemu-system-x86_64`。注意它仍写 `build/images/*.ppm` 到固定路径（行 184-431）→ 迁移 `O=` 时需参数化，且 §6.3 要求端口/socket/临时镜像不跨配置共享（现在用 `/tmp/leonos4-qmp-<taskid>-*.sock`） |
| `run_gcc_probe_qemu.py` (98) | 以快照模式启动 `gcc-probe.vmdk` 并收串口日志 | **无 build.py 调用者**（仅 `docs/LINUX_ABI_PROGRESS_2026-09-08.md:254`）；`build.py:146-147` 只剩 `gcc-probe-image/runner` 的**名字残留**，无对应 target | **(f) 可删**（随 `prepare_gcc_probe.py`/`package_musl_gcc.py` 一起），或 **(e) test-legacy**。理由：其上游（musl-gcc 工具链镜像）已从生产退役；保留则必须登记为显式手工入口并修 build.py 的死名 |
| `prepare_gcc_probe.py` (67) | 造独立 ext2 启动盘，放入未改动的预编译 musl GCC + probe runner | **无 build.py 调用者**；仅 `userland/musl-gcc/README.md:28`；`import package_musl_gcc.COMMANDS` | 同 `run_gcc_probe_qemu.py`：**删除或 test-legacy**，二选一记入 `legacy-removal.md` |
| `tests/` (183 个 `.c` + 4 个非 c：`linux-reference-initramfs.list`、`ltp-installer-grub.cfg`、`musl-installer-grub.cfg`、`legacy_authd/`) | 来宾侧 ABI/存储/APK/sudo/PAM/QMP 探针与测试程序源码（不是 Python） | `build.py` 36 处引用（如 `:1531-1548` `musl-probes`、`:1559` `musl-ltp` inputs）；`tools/test_*.py` 133 处引用 | **(a) 原样保留为测试源码**，编译规则进 `mk/tests.mk`（用 `musl-sdk`/`leonos-musl-cc` 或 SDK 新 C 包装器编译）。**这是新系统的验收资产，绝不能当旧残留删除**：§8 要求 C 工具边界测试用 ASan/UBSan，§12 P1 要求 C 测试覆盖空/截断/溢出/只读/信号。`linux-reference-initramfs.list` 是 Linux 参考根的文件清单，也要保留 |
| `browser_test_page/` (5 静态文件) | browser.elf 手工验证页面（index/relative-page/root-path/folder/page.html/plain.txt） | **无任何调用者**（全仓 grep 零命中；`docs/BROWSER.md:87` 反而引用了已不存在的 `tools/gen_ninja.py`） | **(f) 可删或移出 tools/**（例如 `tests/fixtures/browser/`）。理由：与构建无关，无引用；若保留需顺带修 `docs/BROWSER.md` 的死引用 |
| `abi/linux-v6.12-syscall_64.tbl` | Linux v6.12 x86-64 syscall 表原始副本（钉版上游数据） | 唯一读者 `generate_linux_syscalls.py:11` | **(d) 保留为数据文件**（§7 生成资源需可追溯再生；§9 锁文件思路）。它同时被 `tools/test_uapi.py`/`test_linux_abi_contract.py` 用作 ABI 基线，属测试 fixture，不删 |
| `provision_rpr_signing_key.sh` (61) | 用 Alpine `abuild-keygen` 生成 RPR APK 签名密钥并写入 GitHub Actions Secrets | `docs/RPR.md:21`（人工运维入口） | **(f) 原样保留**（已是 POSIX shell，与构建执行链无关）。注意 §8"不拼接 system()"约束不适用于仓库既有 sh；新 C 工具才受约束 |
| `vscode/generate_compile_commands.py` (167) | 按区域（kernel/loader/libc/userland/devtools/all）生成 `build/vscode/compile_commands.json` | `.vscode/tasks.json:185,195,203,211,219,227`；`tools/vscode/README.md` | **(c)/(b)**：`compile_commands.json` 只能由新构建系统产出才可能长期正确（§4 要求 `V=1` 打印真实 argv）。最省事路径：`make compile-database V=1`/`make -n` 转换，或让 Make 规则旁路导出。当前脚本 **重复实现了一套编译 flag**，是"文档与构建漂移"的源头，必须在 P4 前解决而不是原样搬 |
| `vscode/run_clang_tidy.py` (49) | 用上面生成的 DB 跑 `clang-tidy --region` | `.vscode/tasks.json:235,244,253` | **(c) 短 shell**（`clang-tidy -p <db> $(files)`），保留 `--checks/--fix/--warnings-as-errors` 契约（README 已承诺） |

---

## 4. 资源生成与配置/版本（§7）

判定口径：新 C 生成器 = 纯数据转换（§8 允许）；已有非 Python 外部工具 = (d)；否则只能 (b)。`third_party` 可用性核查见 §4.3。

### 4.1 表格

| 文件:行数 | 职责 | 调用者 | 输入 → 输出 | 外部程序 | 可替代性判断 → 归属 |
| --- | --- | --- | --- | --- | --- |
| `gen_loader_integrity.py` (45) | 把 kernel.sys / middlelayer.sys 的 sha256 写成 C 头数组 | `build.py:1347,1351`（target 产 `loader_integrity.h`） | 读两个 ELF → 写 `out/generated/loader_integrity.h`（含 `LEONOS_LOADER_{KERNEL,MIDDLELAYER}_SHA256`） | 无 | **(b) 极简 C 工具**（或 `sha256sum`+printf shell）。54 行、无解析、输出确定性，属 §5 `tools/host/{manifest,assets}` 一类。**注意**：它是 loader 校验数据（§2 表格点名保留），且输出被 loader 编译消费 → 内容变化必须触发 loader 重链（§6.1 显式输入） |
| `generate_gbk_table.py` (81) | 从 `third_party/litehtml/src/encodings.cpp` 的 `gb18030_decoder::m_index[]` 提取前 23940 项，生成 GBK↔Unicode 双向表头 | `build.py:1404,1407` | 读上游 C++ 源（**正则抓 `{...}` 块**）→ 写 `out/generated/leonos_gbk_table.h`（write-if-changed，行 74-76） | 无 | **(b) C 生成器，但需先换数据源**。可重现性是够的（输入是版本固定的 submodule），但用正则从 C++ 源码里抠数组违反 §5"禁止用正则/临时字符串切片冒充解析器"的精神，且上游一改格式就静默错位。两条可选：(i) C 工具 + 一个 GB18030/GBK 的 C 表来源；(ii) 保留"从 litehtml 取表"但把提取器写成有明确断言（元素数 ≥23940、值域）的 C 工具并加 fixture 测试。截断到 `126*190` 的规则（行 25-28）是行为契约，必须带注释搬 |
| `generate_boot_logo.py` (169) | 把 `logo.png` 解码、缩放到 192×192，输出内联 C 位图头 | `build.py:1203,1205` | 读 `logo.png` → 写 `out/generated/boot_logo.h` | 无（**手写 PNG 解码**：`zlib.decompress` + 自己实现 `paeth`/filter/颜色类型/重采样，行 22-47） | **(d)→(b) 混合，有现成 C 库**：`third_party/libpng` + `third_party/zlib` 已在仓库且 build.py 已从源码编译（见 §4.3）。新做法：一个 host C 程序链 host 编译的 png/zlib，做 解码→缩放→emit 头。这比继续维护手写解码器安全，也满足 §7"资源必须选择可重现的 C 生成器或明确锁定的外部非 Python 工具"（PNG 尺寸/位深变化要报错而非猜测） |
| `make_app_icons.py` (507) | 用内嵌 3×5 位图字体与几何绘制，为每个 app 生成 32×32 32-bit BMP 图标 | `build.py:2339,2340`（target `app-icons`，`--apps` 来自组件选择） | 无输入文件（字形表内联）→ 写 `<out-dir>/<app>.bmp` | 无（`struct.pack("<2sIHHI"/"<IiiHHIIiiII")` 直写 BMP/DIB） | **(b) C 工具（近乎直译）**。这是纯确定性像素生成，无格式解析、无外部依赖，最适合 C；同时它也**不适合**交给 ImageMagick 之类外部工具（会引入浮点/版本漂移，破坏 A13）。注意 16→32 的最近邻缩放（`src = x*SIZE//BMP_SIZE`）必须逐像素复现，否则图标基线漂移。输出集合由 `--apps` 决定 → 与 §6.1"成员清单稳定排序"配合，新增/删除 app 要能触发重建 |
| `make_window_button_icons.py` (123) | 生成窗口按钮 16×16 BMP（最小/最大/关闭等，含 alpha） | `build.py:2348,2350`（target `window-button-icons`；名字列表在 `build.py` `WINDOW_BUTTON_ICONS` 与脚本 `ICONS` 两处） | 无输入 → 写 `<out-dir>/<name>` | 无 | **(b) C 工具**，同上。真正的迁移风险是"图标名清单两份"（脚本 `ICONS` vs `build.py:WINDOW_BUTTON_ICONS`）→ 必须合并成一份数据文件，否则新系统会漏图标且 Make 看不出来 |
| `make_minesweeper_assets.py` (80) | 生成扫雷 mine/flag 20×20 BMP | `build.py:2344,2345` | 无输入 → 写 2 个固定 BMP | 无 | **(b) C 工具**，或干脆把 BMP 作为二进制资源提交（尺寸固定、人工可审）。**不要**为它建泛化图像框架（§8） |
| `prepare_ui_font.py` (290) | 造两版 UI TTF：metro（若源是 `ttcf` 则重打包单字体）与 win95（把 PSF 8×16 点阵字形注入 TTF `glyf/loca/hmtx`，重算 `head` checkSumAdjustment 占位并重建表目录） | `build.py:2330,2332`（target `ui-font`） | 读 `system/fonts/Deng.ttf`、`system/fonts/system.psf` → 写 `out/generated/fonts/{leonos-metro,leonos-win95}.ttf` | 无 | **(b) C 工具（本组最难之一）**。这是真正的 TTF 表级改写器（`font_tables`/`bitmap_glyph`/`pixel_glyph`/`rebuild_font`），没有许可清晰的现成 C 库在仓库内（FreeType 不在 `third_party/`），`fontforge`/`ttx` 都是 Python 或重量级。§7 要求"可重现 C 生成器或明确锁定外部工具"→ 只有两条：用 C 重写（推荐，配 fixture 测试：空/截断/越界 glyph id/缺 hmetrics），或引入并锁定 FreeType。必须保留的行内断言：`PIXEL_FIRST=0x20/LAST=0x7E`、`pixel_advance=(asc-desc)//2`、"每个像素字形必须有独立水平度量否则报错"、`MAX_RUNTIME_FONT_SIZE=20MiB` 运行期尺寸闸（行 200+ 的 `ensure_runtime_font_size`）——它是来宾加载能力的守门条件 |
| `prepare_browser_font.py` (47) | 拷贝外部 Times New Roman 给 browser.elf | `build.py:2335,2336` | 读 `system/fonts/times.ttf` → 写 `out/generated/fonts/times-new-roman.ttf` | 无（`shutil.copyfile`） | **(a) Make `cp` 规则**。为一条 cp 造工具被 §5 明确禁止。真正要做的是把字体来源/许可证登记进 `configs/dependencies.lock.json`（§9）与许可清单 |
| `make_grub_font.py` (94) | PSF(8×16, 256 字形) → 固定宽 BDF 文本（0x20-0x7E） | `build.py:2519,2521` | 读 `system/fonts/system.psf` → 写 `out/generated/grub/leonos-unicode.bdf`；下一步由 `grub-mkfont` 转 pf2（`build.py:2526,2531`） | 无自身；下游 `grub-mkfont` | **(b) 短 C 工具（约 40 行）或 (c) shell/awk**。PSF 头校验（魔数 `0x36 0x04`、mode、`charheight==16`、最小长度）要在 C 里保留。注意 BDF 是文本、PF2 由外部工具产出 → 两段规则都可 Make 化，无需新增依赖 |
| `make_grub_theme.py` (160) | 用内嵌 8×8 字体画 GRUB 背景并输出未压缩 24bpp TGA | **无调用者**（全仓 grep 零命中）。目标路径 `boot/grub/themes/leonos98/background.tga` 既不存在也未被 `boot/grub/*/*.cfg` 或 `boot/grub/theme/theme.txt` 引用 | 无输入 → 写 **源码树** 常量路径 `OUT` | 无 | **(f) 可删**。理由：无调用者、产物未被跟踪、GRUB 主题实际用的是 `boot/grub/theme/theme.txt`。若产品仍想要该背景，正确做法是把 TGA 当资源提交（或改由 make_*icons 系列 C 生成器统一出图），而不是保留一个手写整屏绘图的孤儿脚本 |
| `build_info.py` (73) | 生成版本/时间 C 头并自增构建号 | `build.py:1067,1068,1071`（target `build-info`；`build.py:1230` 把它作为隐式输入挂进编译；`BUILD_NUMBER_EXEMPT_TARGETS` 白名单在 `build.py:136-...`） | 读 `buildsystem/state/build_number.txt` → **写源码树** `include/generated/build_info.h` + 状态文件 | 无 | **(b) 新 C 工具 + §7 强制改造**。现状三处违规：(i) 每次调用 `+1`（§7"普通 build 不递增受版本控制的数字"）；(ii) `datetime.now()` 直接嵌时间（§7 要求优先 `SOURCE_DATE_EPOCH`，回落到固定提交标识）；(iii) 输出写在源码树（§7"版本头移至 O/generated，通过 include 路径消费"）。新实现只读：release 版本 + `git` 固定提交标识 + 可选 `BUILD_ID`，同内容则不改 mtime（§8 `write_file_if_changed`）。注意 §2 已声明工作区有他人未提交的版本号改动，不得覆盖 |
| `generate_app_manifests.py` (86) | 从 `configs/components.toml` 为每个 app 生成 guest `manifest.ini`（id/name/version=system/category/exec/icon/entry/terminal/system/open_with/extensions/commands） | `build.py:2288,2310`（target，产 `out/generated/apps`） | 读 `configs/components.toml`、`userland/apps/<app>/<app>.app.ini`（只为取 `terminal`）→ 写 `<out-dir>/<相对目录>/<app>/manifest.ini`；先 `rmtree(out_dir)` | 无 | **(b) C 工具（复用同一 TOML 解析器）**。它 import `buildsystem/components.load_components`（行 13），所以必须与 §5 的组件清单一起迁：`configs/components.toml` 是组件元数据权威（§7），C 侧需要一个真 TOML 库（§5 禁自造解析器）。`version=system`、`icon=<app>.bmp if entry`、`shutil.rmtree(out_dir)` 全量重建语义都要保留，或改成清单驱动的 grouped target（§6.3）。`read_terminal()` 的 key=value 解析可与 `configs/` 合并掉 |
| `check_abi_migration.py` (142) | 扫描私有 ABI 符号使用；`--report <path>` 写"Generated LeonOS private ABI inventory"台账（按 `sorted(symbol)`、`sorted(path)` 输出），`--strict` 拒绝 app 源码新增私有硬件 API 并对已删除私有 ioctl 全树报错 | `build.py:3811,3813`（target `test-abi-migration`，产 `out/generated/abi-migration-report.txt`）。注意 `build.py:3811` 把 `docs/ABI_MIGRATION.md` 声明为 target input，但脚本本身不读它（只为"文档改动触发再生"）；`docs/ABI_PRIVATE_INVENTORY.md:2` 明确要求"由 tools/check_abi_migration.py 再生" | `git ls-files -z` 枚举受跟踪文件（行 55-60）→ 写 `--report`；排除 `docs/`、`tools/`、`los2w/` 与自身 | `git` | **(e) 保留但列入 test-legacy，或 (b) 重写成不含 Git 的 C 检查器**。当前实现**主动扫描 Git**（§8 明确"C 工具不扫描 Git 来推测构建依赖"），且产物是受跟踪文档。它属审查/报告类工具而非生产链，因此不必强行 C 化；但**不能悄悄丢弃**：`docs/ABI_PRIVATE_INVENTORY.md:2` 声明"由 `tools/check_abi_migration.py` 再生"，删除即断再生路径 |
| `check_unix_paths.py` (120) | 拒绝遗留盘符前缀路径与已删除的驱动 ABI 名（`git ls-files` 扫） | `build.py:3741`（target `test-unix-paths`） | 读受跟踪源码 → 只读检查，无产物 | `git` | **(e) test-legacy 静态检查**（或并入 `test-tools`）。同上：Git 扫描是审查行为，不应作为 Make 依赖；保留为显式检查目标 |
| `check_licenses.py` (449) | 第三方许可/署名打包策略检查：子模块、staging 镜像、ZIP；`--strict`、`--json`、`--self-test` | **无构建调用者**。仅 `docs/README.md:51-61` 与 `README.md:119` | 读 `third_party/**`（含未初始化子模块检测）、`--root/--image/--installer`、`--sdk *.zip`；`--self-test` 会在临时目录伪造 `.gitmodules` 与源树 | 无（`configparser`/`zipfile`）；读 Git 元数据 | **(e) test-legacy 保留 + 列入 `make test-build` 的许可检查**。§9 要求锁文件带许可路径，长期方向是把它的"策略"数据并入 `configs/dependencies.lock.json`，检查逻辑仍是有价值的独立审计器（`--image` 对 FAT/VMDK 明确报告"skipped"而不是假称完整，这种诚实性要保留）。不适合直译成 C（几百条策略分支），也不必删 |
| `count_code.py` (940) | 生成显式源文件清单后调用 `cloc`/`scc`，含历史统计、CoCoMo/LOCOMO 估算、SVG 图、markdown/json 输出 | `.github/workflows/code-count.yml:35`（`--format markdown --no-progress >> $GITHUB_STEP_SUMMARY`）；`docs/CODE_COUNT.md`；`tools/test_code_count.py`；配置 `tools/codecount.json` | 读 `git log`（行 674，含 `--first-parent/--no-merges`）、`.gitignore`/`.gitmodules`、`tools/codecount.json` → 写 `--output`（json/markdown/svg） | `git`、`cloc`、`scc` | **(f) 移出构建系统职责**。它是统计报表工具，与依赖图无关，且重度依赖 Git 历史（§8 禁止 C 工具这样做）。§P4 只要求"CI/文档无活跃旧入口"，`code-count.yml` 与 `tools/` 无耦合。建议：保留原样但在台账标注"不参与构建执行链，Python 允许"，或迁到 `scripts/`。（另：`docs/CODE_COUNT.md` 引用 `build/code-count.json`，`O=` 改名时需同步） |
| `generate_doxygen.py` (128) | 生成 Doxyfile 并跑 `doxygen`，输出到 `tools/dist` | **无调用者**（全仓零命中；仅 README 能力清单提到"Doxygen 文档"；`build.py` 无 `doxygen`） | 读源码树 → 写 `tools/dist/`，并**生成 Doxyfile 到临时/工作目录** | `doxygen` | **(f) 可删或降级为文档说明的 one-liner**。理由：无调用者；`doxygen` 本身就是成熟外部程序，不需要 128 行 Python 包装（真正必要的只是一份 `Doxyfile` 数据文件 + `doxygen Doxyfile`）。若仍要文档站点，应把配置作为受跟踪文件（§7 可追溯再生） |
| `generate_component_kconfig.py` (111) | 校验 `configs/components.toml` 并生成 `Kconfig.components` 菜单（每组件 `BUILD/IMAGE/ENTRY/SDK/API` 符号，`select` 表达依赖自动启用） | `build.py:950,970,3645,3665,3818,4330`；CI `python3 tools/test_component_config.py` 间接 | 读 `configs/components.toml`（经 `buildsystem/components.load_components`）、`validate_component_targets`（会检查 `userland/**` 是否有源文件）→ **写源码树 `Kconfig.components`**（Git 跟踪）、可选 `--json` | 无 | **(b) C 工具 + §7 布局修正**。职责本身必要（TOML 是元数据权威，Kconfig 必须随之生成），但违例两点：(i) 输出写进源码树且被提交 → 改到 `O/generated/Kconfig.components`，用 `include` 消费；(ii) 校验逻辑（正则 + 全量字段）与 §5 的 TOML 解析要求冲突 → C 侧必须用真 TOML 库。语义要点必须逐条保留：required 组件不进菜单（行 36-37）、只有 `api && api_stage_path` 才有 `API` 开关（行 64-73）、`select` 而非 `depends on`（行 56-58 的理由注释） |
| `kconfig_sync.py` (241) | `.config` 解析/校验/规范化 + 组件符号求值 + 生成 `autoconf.h`/`autoconf-installer.h`/`rustcfg.args`/`component-selection.json`，并回写 `.config` | `build.py:956,971,3656,3666,4333`（target `config-sync`、`menuconfig`）；`build.py:946` 注释把它当作"新增输出格式时要改的地方" | 读 `configs/default.conf`、`buildsystem/config/leonos.conf`、`configs/components.toml` → 写 **`.config`（回写）** + `include/generated/*`（Git 跟踪）+ `out/generated/component-selection.json` | 无 | **(b) C 工具（拆两块）**：(1) 配置规范化/预设展开/未知符号报错 —— 逻辑约 110 行、无外部依赖、直译即可，满足 §7"未知/失效符号有可读报告"；(2) 输出 emit 用 `write_file_if_changed`（现在是自实现的读-比-写，行 126-133，语义已正确）。**必须修的违例**：`BUILD_PRESET_VALUES` 在 Python 里硬编码了一份默认值（行 30-52），而 §7 明令"不要再复制一套 defaults 到 C 常量或 Make 中" → 预设应来自 `configs/` 数据；`retired` 符号白名单硬编码 `INIT/SERVICED`（行 118-120）同理。另外 `config_key_order` 依赖 defaults 文件出现顺序（行 137），C 侧要保持稳定且与 locale 无关 |
| `build_kconfig_frontends.py` (101) | 从 `third_party/kconfig-frontends`（C 前端，submodule）构建宿主 `kconfig-mconf` | `build.py:3622,3633`（target `kconfig-mconf`）、`build.py:3827`（`test-kconfig-frontends` 输入） | 读 submodule 全树 → 写 `out/host/kconfig-frontends-work`（副本）与 `out/host/kconfig-frontends/bin/kconfig-mconf` | `./bootstrap`（autoreconf/aclocal/autoconf/automake/libtool）、`./configure --enable-frontends=mconf --disable-utils --disable-L10n --disable-shared --enable-static --disable-werror`、`make -j$(cpu_count)`、`make install`；无 Git；无网络；**还有一处非 Git 的源码修补**：`patch_gperf_compatibility()`（行 32-49）把 `libs/parser/hconf.gperf` 的 `unsigned int len` 改 `size_t len` 以适配现代 gperf | **(a) 纯 Make**（递归 `$(MAKE)` + 继承 jobserver，§6.1）。这是 autotools 项目，**没有 Meson/Ninja/CMake**。gperf 修补要留在"配置专属工作目录"里（§9），用一条显式 `sed`/patch 规则并把修补后的摘要计入签名（§6.2），而不是藏在 Python 字符串替换里。**新系统能否只用 C 前端 + Make？—— 见 §4.2 的结论：可以覆盖 `menuconfig`，但不能覆盖 `defconfig/olddefconfig/autoconf.h`** |

### 4.2 Kconfig：新系统只用 `third_party/kconfig-frontends` + Make 是否足够？

事实（可直接引用）：

- `third_party/kconfig-frontends/frontends/` 里有 `conf`、`mconf`、`nconf`、`gconf`、`qconf`；当前只启用 `mconf`（`build_kconfig_frontends.py:80`），`conf`（非交互）尚未构建。加一个 `--enable-frontends=conf,mconf` 即可同时得到 `kconfig-conf`，用于 `defconfig/olddefconfig/allyesconfig` 类目标 —— 这是 §7"使用仓库固定的 C Kconfig 前端"的正解，零新增依赖。
- 但 `Kconfig` 只负责符号求值并输出 `.config`。LeonOS 额外需要、目前**全部在 Python 里**的语义有 4 类，`conf` 无法替代：
  1. 从 `configs/components.toml` 生成 `Kconfig.components`（`generate_component_kconfig.py`）；
  2. `configs/default.conf` + BUILD_PRESET 展开 + 组件符号回写（`kconfig_sync.normalize_values/write_config`）；
  3. `.config` → `autoconf.h`、`autoconf-installer.h`（两个 `LEONOS_LICENSE_REQUIRE` 常量不同）、`rustcfg.args`、`component-selection.json`；
  4. `validate_component_targets`（检查组件源目录是否真有 `.c/.S/.cpp`）与未知/失效符号可读报告。
- 结论：**可以只用 C 前端 + Make + 新的 C 转换工具**，不需要 Python。建议分工：`kconfig-conf`/`kconfig-mconf` 负责符号层；`tools/host/config/`（C）负责 TOML→Kconfig 生成与 `.config`→(Make include + C 头 + rustcfg + selection JSON) 转换；Make 负责 `O/config`、include 路径与 §6.2 参数签名。`configs/default.conf` 与 `configs/profiles/` 仍是权威（§7），C 工具只读取。
- 迁移检查点：`build.py:946` 的注释明确"新增配置输出格式时改 `tools/kconfig_sync.py` 并把脚本加入 inputs"，说明 `config-sync` 的输出集合是可增长的 —— 新 C 工具必须按 §7"内容稳定、相同输入不更新时间"与 §6.3 多输出（grouped target `&:`）设计，否则 autoconf.h 会引发自重启循环（§7 明文禁止）。

### 4.3 `third_party/libpng` / `zlib` / `minimp3` 可用性核查（用户特别要求）

| 库 | 状态 | 证据 | 对资源生成的可用性 |
| --- | --- | --- | --- |
| `third_party/zlib` | 可用（真 C 源码，submodule）。build.py **已从源码逐文件编译**为 target 静态库 | `build.py:872`、`ZLIB_SOURCES`、`:1420-1428` `archive:zlib`，用 `-DZ_SOLO -include stddef.h` | 可直接为 host C 工具提供 inflate/deflate（`generate_boot_logo` 的手写 PNG IDAT 解压、`package_*` 的 gz 处理）。注意 `Z_SOLO` 是 target 侧裁剪配置，host 工具需要自己的最小配置（可同样逐文件编译，不跑它的 configure） |
| `third_party/libpng` | 可用（真 C 源码，submodule），已被编译进 target；**关键：配置头走 prebuilt，不依赖 Lua** | `build.py:873-874` 使用 `scripts/pnglibconf.h.prebuilt`，`:979-1000` 只做文本级 marker 替换以关掉浮点支持（`PNG_FLOATING_ARITHMETIC_SUPPORTED`/`PNG_FLOATING_POINT_SUPPORTED`/`PNG_READ_FLOAT_SUPPORTED`，注释理由："普通 LeonOS 用户进程刻意避开 x87/SSE 状态"）；`:1431-1436` `archive:libpng` + `-DLEONOS_LIBPNG_FIXED_POINT=3` | **是 `generate_boot_logo.py` 与任何 PNG 读/写的正当替代品**，且已解决 libpng 生成 `pnglibconf.h` 需要 Lua 的经典坑（用 prebuilt 绕开）。host 工具若需定点路径要同样声明 fixed-point 配置，不能默认浮点（会与 target 行为不一致） |
| `third_party/minimp3` | 只有 `minimp3.h`（单头文件解码器），**只在 target 侧使用** | `build.py:1153` `-Ithird_party/minimp3` | **与资源生成无关**（MP3 音频解码，非图片）。不能作为 §7 的替代路径，也不需在 host 工具里引入 |
| 其他已存在 C 库 | `mimalloc`、`musl`、`sqlite`、`file`、`lua`、`ncurses`、`nano`、`less`、`sl`、`cmd`、`tinycc`、`portablegl`、`busybox`、`litehtml`、`stardustui`、`mbedtls`、`doomgeneric`、`rime-pinyin-simp`、`kconfig-frontends` | `ls third_party/`、`.gitmodules` | 与本节仅两点相关：`litehtml` 是 GBK 表当前数据源（§4.1）；`kconfig-frontends` 是配置层唯一 C 前端（§4.2） |

补充（许可与"不新增依赖"）：`docs/THIRD_PARTY.md`、`configs/apk-ownership.json`、`check_licenses.py` 是现有许可登记面；§9 要求新增 JSON/TOML 库必须"固定版本并加入许可清单"，§5 禁止正则假解析 → 建议锁定一个单文件 C JSON 与一个 TOML 实现（如 MIT/Public-domain 级），并把许可路径写进 `dependencies.lock.json`。

---

## 5. 上游组件构建脚本（§2 `tools/build_*`）

### 5.1 共同模式（事实）

1. **来源**：绝大多数读 `third_party/<pkg>` submodule，先 `git rev-parse HEAD` 与脚本内钉的 commit 常量比对，不符即退出（`build_cmd.py:36`、`build_less.py:43`、`build_lua.py:52`、`build_nano.py:40`、`build_pleditor.py:36`、`build_sl.py:35`、`build_sqlite.py:61`、`build_tcc.py:62`、`build_terminal_packages.py:26`、`build_file_magic.py:63`、`build_busybox.py:127`、`build_musl.py:39`）。`build_busybox.py:120` 还取 `%ct` 提交时间戳参与缓存键（行 158-166）。
2. **"上游树保持原样"**：多数脚本把源复制进 `--work-dir` 再编译，并显式声明不 patch（如 `build_lua.py` 文档"upstream source tree is not patched"、`build_tcc.py`"upstream tree stays pristine"）。真正的补丁只有一处 per-package：musl（`patches/musl/`，`build_musl.py:56`）与 linux-pam（`patches/linux-pam/*.patch`，`build_auth_upstream.py:122-143`）。
3. **适配层是"port 目录 + 显式源清单"**，不是 patch：`--port` 指向 `userland/*/port`，C 文件清单在脚本内硬编码（`build_file.py:LIBMAGIC_SOURCES`、`build_less.py:LESS_SOURCES`、`build_nano.py:NANO_SOURCES`、`build_cmd.py:EXCLUDED_SOURCES`、`build_pleditor.py:EDITOR_SOURCES`）。
4. **链接**统一走 `musl_link.executable/shared()`（§1.1）。
5. **`--stamp`**：每个脚本写一个 JSON/stamp 文件供 Make 侧依赖与元数据追溯。

### 5.2 逐项（构建体系与归属）

| 脚本:行数 | 上游与版本锚 | 上游构建体系 | Meson/Ninja/CMake | 打补丁 | 调用者 | 归属 + 理由 |
| --- | --- | --- | --- | --- | --- | --- |
| `build_busybox.py` (912) | `third_party/busybox` + commit；可选 `--official-source` 官方 tarball（行 187 `official_source()`） | BusyBox 自有 kbuild **Make**（`make -C src O=out allnoconfig` → `oldconfig` → `make CC=clang ARCH=x86_64`，行 834/843/873） | 无 | 无（配置在 Python 里逐项写 `.config`；`--compile-flag/--linker-flag/--leonos-*` 注入） | `build.py:1770,1780`（target `busybox`） | **(c) shell + Make 递归**，配置片段变数据文件。BusyBox 原生就是 Make，新系统直接 `$(MAKE) -C ... O=...` 并继承 jobserver（§6.1；当前脚本自己 `-j`，需去掉硬编码并行）。注意它还负责 PID1/init 相关配置（`tools/test_busybox_pid1.py` 覆盖），配置项清单必须逐条对照，不能"allnoconfig 后再猜" |
| `build_cmd.py` (523) | `third_party/cmd` commit | **无上游构建体系**：Python 手写逐文件 `clang` 编译循环 + `musl_link` | 无 | 无，用 `EXCLUDED_SOURCES` 排除 `lexec/llinenoise/lpath/lreadline/lsysport` 并换 LeonOS port 实现 | `build.py:1987,1997` | **(a) Make 规则**（`mk/components/cmd.mk` 声明源清单 + 排除清单）。这正是 §6.1"源文件优先由组件清单声明"的既有范例，脚本只是编译器的命令行编排；迁移后应删除 |
| `build_less.py` (171) | `LESS_COMMIT=b8bbf429...` | 无上游 make：Python 编译循环，**但调用上游 `mkhelp.py` 用 `python3`**（行 76 `run(["python3", source/"mkhelp.py"])`） | 无 | 无（port 层） | `build.py:1904,1914` | **(b) 小 C 生成器 或 (c) shell**。§9 明文"上游 configure 允许生成 Shell/Make 文件，但**禁止默认路径中调用 Meson/Ninja/Python**"，而 less 的原生构建正是靠 `mkhelp.py` 把 help 文本编成 `help.h` —— 这是 P0-c 上游审计必须记录的第三个 Python 依赖点（前两个是 linux-pam 的 Meson、见 §5.3）。可用 C 工具复刻 `mkhelp.py`（输入是纯文本表，无上游修改） |
| `build_nano.py` (160) | `third_party/nano` commit | Python 编译循环（显式 `NANO_SOURCES`）+ `musl_link`；`--dynamic` 开关 | 无 | 无（port + `--leonos-libc-include`） | `build.py:1804,1814` | **(a) Make 规则**，同 `build_cmd` |
| `build_sl.py` (133) | `SL_COMMIT=923e7d7e...` | Python 编译循环 | 无 | 无 | `build.py:1863,1873` | **(a)** |
| `build_pleditor.py` (193) | `third_party/pl_editor` commit，从**pristine worktree** 复制 | Python 编译循环（`EDITOR_SOURCES = main.c pleditor.c syntax.c`），`--generated-include` | 无 | 无 | `build.py:2026,2036` | **(a)** |
| `build_lua.py` (194) | `third_party/lua` commit | Python 编译循环 + `llvm-ar` 造静态库 + `musl_link.shared` 出 `liblua.so.5` | 无 | 无；文档明确用 **C89 可移植配置**，故意排除 POSIX-only/readline/C 模块加载，并说明 Lua 是 `-mgeneral-regs-only` 的**例外**（`double` 必须走 SSE ABI） | `build.py:1942,1952` | **(a)**，但 §6.2 要求"参数签名"记录该 ABI 例外；`-mgeneral-regs-only` 例外若丢，Lua 数值会破坏（A05 类问题）。文档注释要迁进 `mk/components/lua.mk` 头部 |
| `build_file.py` (143) | `third_party/file` | Python 编译循环，显式 `LIBMAGIC_SOURCES`（含自带 asprintf/getline/strcasestr 兼容实现）；产 `libmagic.so.1` + 静态库 | 无 | 无（port/config.h） | `build.py:1583,1597` | **(a)** |
| `build_file_magic.py` (75) | 同一 file 源 | **autotools**：`autoreconf -fi` → `./configure`（一堆 `--disable-{bzlib,zlib,zstdlib,lzlib,lrziplib,landlock,libseccomp,shared}`）→ `make all` | 无 | 无（在临时构建副本操作） | `build.py:1565,1577` | **(c) 短 shell + 递归 make**，产出 `magic.mgc`。§9 允许 configure 生成文件；这里的关键是"上游源码不被污染"（脚本复制后再 autoreconf），迁移时同样要在工作目录做 |
| `build_sqlite.py` (128) | `SQLITE_VERSION=3.46.1` + commit | **混合**：`make -f Makefile.linux-gcc sqlite3.c sqlite3.h` 生成 amalgamation，再由 Python 编译循环 + `llvm-ar` + `musl_link` | 无 | 修改 `Makefile.linux-gcc` 文本后使用（行 80-84，写进 `generated/`） | `build.py:1632,1652` | **(c) shell 生成 amalgamation + (a) Make 编目标**。注意要求 `third_party/sqlite/main.mk`、`Makefile.linux-gcc`、`port/leonos_sqlite_vfs.c` 三者存在（行 58），这些是显式输入，要进签名 |
| `build_portablegl.py` (120) | `PORTABLEGL_COMMIT=7cf39dc1...` | 单头文件库：Python 编译循环 + `--autoconf`（上游 `portablegl.h` 需要生成 config）+ `musl_link.shared/static` | 无 | 无 | `build.py:1681,1693` | **(a)** |
| `build_tcc.py` (513) | `third_party/tinycc` commit | Python 编译循环 + `llvm-ar`；安装自建 "LeonOS target definition layer"（行 105 生成 config 头） | 无 | 不改上游，改的是 build 私有源副本 | **无任何调用者**（`build.py` 里 `tcc` 出现次数为 **0**；`leonos_layout.RETIRED_TOOL_PATHS` 已含 `opt/tcc`；仅 `qmp_terminal_smoke.py:87 --tcc` 遗留分支） | **(f) 可删除**（连同 `qmp_terminal_smoke.py` 的 `--tcc` 分支与 `RETIRED_TOOL_PATHS` 中 `opt/tcc` 的过渡清理）。理由：生产图已不引用它 513 行；§3 说不把所有 py 当旧残留 —— 但这个是真的退役残留。若产品仍承诺 TCC，则必须先恢复 target 再谈迁移（当前是"死代码但被 README/测试名暗示存活"） |
| `build_terminal_packages.py` (126) | `REVISIONS = {ncurses:0096bd40..., vim:af9a7a04...}` | **autotools**：ncurses `./configure --with-normal --without-ada --with-termbin ...`；vim `./configure --with-features=normal --disable-{nls,perlinterp,pythoninterp,rubyinterp,tclinterp,luainterp} ...` → `make` → `make install DESTDIR=` | 无 | 无 | `build.py:837,842`（按包参数化，target 名即包名） | **(c) 短 shell + 递归 make**，包参数进 `mk/components/*.mk`。这是它最难迁移的点：脚本按 `package` 参数被多次调用（每个终端包一个 target），新 Make 需要模式规则；同时 vim 的 `--disable-pythoninterp` 是 §2"不运行 Python"的**正面基线**，必须保留并在 `doctor`/A16 里可验证 |
| `build_storage_upstream.py` (106) | `configs/storage-upstream.json`：e2fsprogs 1.47.3、dosfstools 4.2、exfatprogs 1.4.3（含 sha256 + 上游校验和来源） | **autotools**：`./configure --host=x86_64-linux-musl --prefix=/usr ...` → `make` → `make install DESTDIR=`；e2fsprogs 用 `--disable-*/--enable-static`，dosfstools `--enable-compat-symlinks`，exfatprogs `--disable-shared --enable-static` | 无 | 无 | `build.py:816,820` | **(d) 这些正是镜像制作要复用的外部工具本身**。构建走 `tools/build/*.sh` + Make；产出 `mkfs.{ext2,fat,exfat}`/`fsck.*` 交给 `storage_tools` 清单（§2）。它 import `build_auth_upstream.run` 与 `build_musl.REVISIONS`、`fetch_auth_upstream.fetch` → 迁移时先抽出公共 fetch/log 层，避免 shell 之间互相 import Python |
| `build_musl_ltp.py` (87) | `SOURCES`：ltp `3a64d78f...`（codeload tarball）、linux-6.12 | **autotools + LTP 自有 make**：`make autotools` → `./configure --host=x86_64-linux-musl --build=x86_64-pc-linux-gnu --without-libcap --without-numa --without-openssl --without-tirpc` → `make include-all lib-all` → 逐目录 make；另 `make ARCH=x86_64 headers_install` | 无 | 无 | `build.py:1559,1561`（target `musl-ltp`，依赖 `musl-sdk`）；`REVISIONS/SOURCES` 被 `build_auth_upstream.py:22` import | **(c)/(e)**：属诊断/测试制品（用 SDK 包装器编 LTP，再由 `make_musl_checkpoint --ltp` 塞进 checkpoint root）。可归 `mk/tests.mk` + shell。**注意它同时是"数据源"**：`SOURCES` 定义了 linux-6.12 的 URL/sha256，auth/storage 都复用它 → 该表必须迁进 `configs/dependencies.lock.json`（§9），不能留在 Python 常量里 |
| `build_api.py` (369) | 无上游；实现 LeonOS `.api` 包格式（`API_FORMAT="leonos-api"`，512 字节块，32MiB 上限，成员路径/输入法 ID/设置校验） | — | 无 | 无 | **无生产调用者**：仅 `tools/test_apiapp_qemu.py:12,110` import `build_api_file`；`devtools/README.md:125`、`devtools/docs/PACKAGING.md:28,77` 指导第三方使用。`build.py:2566-2572` 反而在**删除** `api/*.api` 旧产物 | **(e) 保留并列入 test-legacy，且必须继续对外文档化**（它定义的是 **guest 可安装的包格式**，属产品 ABI 而非旧构建残留 —— §3 精神同上）。长期方向是并入 SDK/打包工具（与 `devtools/` 打包路径统一）；不要 C 化也不要删除，除非产品同时撤回 `.api` 格式承诺 |
| `build_auth_upstream.py` (251) | `configs/auth-upstream.json`（libxcrypt 4.5.2、Linux-PAM **1.7.2**、libmd 1.2.0、libbsd 0.12.2、sudo 1.9.17p2、util-linux 2.41.6、shadow 4.20.2） | **混合 —— 本项是 §2 点名的最大阻塞**：`linux-pam` 用 **Meson**（`meson setup --cross-file musl.ini`、`meson compile`、`meson install`，行 157-164）+ `build.ninja` 存在时 `--reconfigure --clearcache`；其余 6 个包用 **autotools configure + make + make install DESTDIR** | **是：Meson + 隐藏 Ninja** | **是：`patches/linux-pam/000{1,2,3}-*.patch`**（`--fuzz=0`，摘要寻址的 `adapted/linux-pam-<sha>` 目录） | `build.py:807,808,818`（target `auth-upstream`，依赖 `musl`）；`build_storage_upstream.py:14` import 其 `run` | **必须按 §2/§P0 上报，不能自行降级**。事实要点：(i) 交叉文件 `musl.ini` 是 Python 风格字面量生成（行 100-107 注释）；(ii) 编译身份极具体：`--gcc-toolchain=/nonexistent`、`--rtlib=compiler-rt`、`--unwindlib=none`、`-nostdinc -isystem <musl>/include -isystem <clang-res>/include -idirafter <linux-headers>`（行 89-93）；(iii) libbsd/shadow 需要把 musl include 从 `-isystem` 改成 `-idirafter`（行 151-155）；(iv) util-linux 需 `-include linux/openat2.h` 绕过上游 2.41.6 缺头 + 把 `-L<musl>/lib` 放进 `CC` 以压过 libtool relink 时的 `-L/usr/lib`（行 182-191）；(v) `linux-pam-build.json` 等 3 份 metadata 是 `package_devtools.py` 的输入（`build.py:2360-2362`）。可选方案（供你写入台账）：A. 用 `linux-pam` 自带 `configure` 的发行 tarball 是否含 autotools 兼容层 —— 实测需验证，1.7.x 官方 release 已 Meson-only；B. 降级 PAM 到最后一个 autotools 版本并评估 ABI/补丁（§2 要求先报告风险）；C. 手写 `Linux-PAM` 的 Makefile 移植（工作量大，且要保 `securedir=/lib/security`、setuid `unix_chkpwd 4755` 权限契约，行 205-209）；D. 保留 Meson 但把它从"构建执行链"隔离到 `make fetch` 之后的预构建产物 —— 违反 §2，需用户明示同意 |
| `fetch_auth_upstream.py` (111) | 同 auth manifest | 通用 fetch：`curl --fail --location --retry 2 --connect-timeout 20 --max-time 300` → sha256 校验 → `.partial`→rename → 解包到配置专属 src → **`verify_source_tree()` 逐成员比对（含 tar `data_filter` 符号链接规范化、绝对路径/`..`/首段目录名/符号链接替换/未知文件拒绝）**；校验 license 文件存在 | — | — | `build.py:774,800,817,1772` 输入；被 `build_musl.py:16`、`build_auth_upstream.py:20`、`build_storage_upstream.py:13` import | **(b) 新 C 工具（`fetch` + `unpack-verify`）+ §9 直接对应**。它就是 §9 描述的形态（下载 `.partial`、摘要不符不自动更新锁、防绝对路径/`..`/符号链接逃逸），迁移价值最高。TLS 校验与代理变量要保留（当前 curl 默认即校验，但**未显式 `--proto =https`** —— 只有 `package_fastfetch.py:62` 有 → 迁移时统一加上并写进台账差异）。`MANIFEST = configs/auth-upstream.json` 是同一职责的第二份清单 → §9 要求"迁移而非并存两个权威来源"，应与 `dependencies.lock.json` 合并 |

### 5.3 Meson/Ninja/Python 审计事实（供 P0-c 直接使用）

| 位置 | 类型 | 说明 |
| --- | --- | --- |
| `build_auth_upstream.py:157-164` | **Meson + Ninja（隐藏）** | 仅 `linux-pam` 1.7.2；`build.py` 不直接调 meson，全部由该脚本驱动；CI 为此安装 `ninja-build`/`meson`（`build-installer.yml:62-63`） |
| `build_less.py:76` | **Python（上游 `mkhelp.py`）** | less 的 `help.h` 由上游 Python 脚本生成，属"默认路径中调用 Python"（§9 禁止） |
| 上游 configure 生成的 shell/m4/libtool | 允许 | `build_file_magic.py`（autoreconf）、`build_terminal_packages.py`、`build_storage_upstream.py`、`build_musl.py`、`build_kconfig_frontends.py`、`build_musl_ltp.py` —— 均为 autotools/Make，无 Meson/Ninja/CMake |
| `libpng` `pnglibconf.h` 的 Lua 生成路径 | **已规避** | `build.py:874,979` 直接用 `scripts/pnglibconf.h.prebuilt` + 定点化补丁，不跑 libpng 生成器 |
| CMake | 未发现 | `third_party/libpng/CMakeLists.txt`、`sqlite` 等存在但均未被构建脚本使用 |
| Python 解释器自身 | 生产链广泛存在 | 所有 `(PYTHON, "tools/...")`（`build.py` 约 60 处 command）+ `build.py:1546` 直接执行 SDK 内的 Python 包装器 + `make_ext2_root.py:20` `fakeroot -- python3 <self>` |

---

## 6. 其它：清单/索引/校验/报告类

| 文件:行数 | 职责 | 调用者 | 输入 → 输出 | 外部程序 | 迁移归属 + 理由 |
| --- | --- | --- | --- | --- | --- |
| `build_rpr_apps.py` (173) | 造 RPR 可选官方应用 APK（helloworld/doom/oschinpt）：manifest.ini + 图标 + 索引 + 包脚本 | `build.py:3310,3323`（target `rpr-apps:*`） | 读 `--build-info`（正则取 `LEONOS_KERNEL_VERSION` → `<ver>-r<build>` 版本号）、`--helloworld/--doom/--doomlauncher/--oschinpt/--oschinpt-index/--icon-dir`、`tools/oschinpt-apk-post-*` → 写 `--output` 下 payload + `.apk` | `openssl`（签名，经 `apk_distribution.signing_key/make_package`） | **(b)/(c) + 依赖 §2 的 APK 层**。真正的构建期版本从 `build_info.h` 反解正则 → 迁移时改为直接消费版本工具的显式输出（§7 版本契约），不要保留"从生成的 C 头里再 grep"的回路 |
| `build_rpr_pages.py` (193) | 组装已签名的 GitHub Pages 远程包仓库（RPR）：APK 集合、kernel/middlelayer 发布物、SHA256SUMS、index.json、`.nojekyll`、`health.txt`、`index.html`、`.complete` | `build.py:3341,3346`（target `rpr-pages`；CI `publish-rpr.yml:69`） | 读 `--repository`(多个 APK 目录)、`--kernel`、`--middlelayer`、`--build-info` → 写 `--output` stage 树 | `openssl pkey -pubout`；`LEONOS_APK_SIGNING_KEY` 环境变量 | **(c)/(b)**：本质是"造目录树 + 写清单 + 算摘要"。清单内容必须可重现（A13）：现在依赖 `apk_distribution` 的 `time.time_ns()` 版本 → 不可重现，需与 §2 一起修；`json.dumps(sort_keys=True)` 与固定 `index.html` 文本可直译成 C/模板。属可选发布产物，不阻塞核心构建，可在 P3/P4 之后处理 |
| `package_fastfetch.py` (94) | 下载并校验**预编译静态 musl fastfetch 二进制**，staging 到 rootfs | `build.py:901`（import `CACHE`）、`build.py:1844,1848`（target `fastfetch`） | 读 `--source`（可选本地文件，`LEONOS_FASTFETCH`）→ 缓存 `buildsystem/deps/fastfetch/<name>` → 写 `--stamp`（含 sha256/URL/target） | `curl --proto =https --proto-redir =https --fail --location --retry --connect-timeout --max-time`、`readelf` | **(d) `make fetch` + Make copy**。它已符合 §9 的下载姿态（TLS/协议限制/摘要/超时，是 `fetch_auth_upstream` 应对齐的样板）。真正的问题是"直接下载二进制、不构建上游"（`leonos_layout._PAYLOAD_PATHS["fastfetch"]`，`tools/test_fastfetch_package.py` 覆盖）→ 台账要显式记为预构建依赖例外并保留许可记录 |
| `package_python.py` (111) | 解包并校验钉版静态 musl CPython，产出 `opt/python` + `/usr/bin` 符号链接 + C launcher | **无构建调用者**；仅 `userland/python/README.md:28`。`leonos_layout.RETIRED_TOOL_PATHS` 含 `opt/python` 与 `python*` 命令 | 读 `--source`/`--archive`（`buildsystem/deps/python/cpython-3.14.7+...-musl-lto+static-full.tar.zst`）→ 写 `--out/root` | `zstd`、`tarfile`、`clang` 编 `userland/python/launcher.c`、`musl_link.executable` | **(f)/(e)**：Python 作为 guest 组件已退役（`test_builtin_tool_removal.py` 覆盖移除）。**注意语义区分**：§2 说"不要求删除系统提供的 Python 应用"。因此这不是"清理 Python 构建"，而是产品决定；台账里要区分"构建链不用 Python"（本次目标）与"镜像里有无 CPython"（产品能力）。若要保留能力则列入 `test-legacy`+手工文档 |
| `package_devtools.py` (566) | 生成 `LeonOS4-Developer-SDK.zip`（headers/libs/ncurses/zlib/libpng/PAM 头与 .pc 重写/示例/许可清单） | `build.py:2359,2392`（target `sdk`） | 读 `devtools/` 树、sysroot include/lib、`include/uapi/**`、`include/leonos/**`、`userland/libc/include/**`、zlib/libpng 源与归档、`--pam-root` 与 `*-build.json` metadata、`--musl-source`(COPYRIGHT) → 写 SDK zip | 无外部程序（`zipfile` 定时间戳 1980-01-01）；**拷贝 `tools/leonos_musl_cc.py` 为 `bin/leonos-musl-cc`（行 407）** | **(a) Make 打包 + (b) 清单工具 + §10 违例修复**。三点：(i) 大段"权威头文件优先级/去重"规则（行 340-375 的 `authoritative_names`、`ncurses_header_names`、`musl_names` 三集合与"UAPI 拥有 wire 定义"注释）是 §10 SDK 契约，应变成声明式清单而非 if 链；(ii) `.pc` 文件重写为 `${pcfiledir}/../..` 可移动前缀（行 425-432），必须保留（§10"安装前缀可移动，不能嵌入开发者绝对路径"）；(iii) Python 包装器 → 换 §1.2 的 C 驱动。zip 字节可重现（固定 timestamp + sorted 遍历）是正面基线，A13 可沿用 |
| `make_oschinpt_index.py` (82) | 从 Rime 拼音字典生成 `OSCI` v1 二进制偏移索引（8 字节码 + 偏移，头 `<4sIII`/条目 `<8sII`） | `build.py:3146,3149`（target `rpr-apps:oschinpt-index`）；被 `build_rpr_apps.py --oschinpt-index` 消费 | 读 `--input`（`third_party/rime-pinyin-simp` 字典）→ 写 `--output`（临时文件 + rename） | 无 | **(b) C 工具**。纯二进制格式转换、无解析歧义、与 guest 查询侧 ABI 强耦合（结构体布局注释即契约）。必须保留"按字典原始行序、不做排序"的语义与 `--` 注释/字段数判定 |
| `rebuild_archive.py` (23) | `ar` 前删旧 archive，保证被删目标文件不残留成员 | `build.py:1475,1476,1497,1498` | `--ar/--output/objects` | `ar`（`llvm-ar`） | **(a)**，见 §3.1 同条目 |
| `openrc_packages.py` / `image_test_accounts.py` | — | 见 §2 / §3.1 | — | — | — |
| `analyze_boot_log.py` (447) | 解析 loader/kernel/middlelayer/ELF/动态加载器串口日志，`--json` 报告、`--strict`、`--self-test` | **无构建调用者**；`docs/README.md:37-45`、`docs/FILESYSTEM.md:210`（人工诊断入口） | 读日志文件或 stdin → 写 `tools/dist/boot-report.json` | 无 | **(e) test-legacy / 保留为人工诊断工具**。它是排障生产力，不参与构建执行链（§2 不要求删除无关 Python）。若入 `make test-smoke` 日志判读需先参数化 `tools/dist` 硬路径（§6.3 临时/日志不跨配置共享） |
| `license_server.py` (720) | 本地许可证激活服务器（HTTP + sqlite + HMAC 在线/离线密钥） | `build.py:3730,3731` 仅作为 `test-license-server` 的 inputs；实际由 `tools/test_license_server.py` 启动；CI 有 `license_server_url` 输入并注入 `KCONFIG_LICENSE_SERVER_URL` | 读环境变量 `LEONOS_LICENSE_BIND/PUBLIC_BASE/LICENSE_DB` → 写 sqlite `build/license-server/license.db` | `sqlite3`（stdlib）、`http.server`；无网络（是被连接方） | **不属于构建系统**（§2"不要求删除无关开发服务"）。**归属：产品/开发服务，原样保留 + 列入 test-legacy 并声明依赖 sqlite3**。迁移注意事项只有两点：`build/license-server/` 路径要随 `O=` 走；`.github/workflows/build-installer.yml:20,131,171-174` 对该服务 URL 的参数校验是**对外契约**（§P4），必须一并迁到新入口 |
| `check_abi_migration.py` / `check_unix_paths.py` / `check_licenses.py` / `count_code.py` / `generate_doxygen.py` | 见 §4.1（同表，避免重复） | — | — | — | — |
| `leonos_layout.py` | 见 §3.1 | — | — | — | — |

---

## 7. 「无调用者」汇总（P4 `legacy-removal.md` 直接输入）

判定口径：**在 `build.py`、`buildsystem/`、其它生产 tools 脚本、CI workflow、`.vscode/` 中都没有任何引用点**（仅有 README/docs 文字提及，或仅被 `tools/test_*.py` 引用，均按"无生产调用者"单列）。

### 7.1 完全零引用（仅剩自引用） — 3 个

| 文件 | 备注 |
| --- | --- |
| `tools/build_tcc.py` (513) | `build.py` 中 `tcc` 出现 0 次；`RETIRED_TOOL_PATHS` 已含 `opt/tcc`；仅 `qmp_terminal_smoke.py:87 --tcc` 遗留 |
| `tools/make_grub_theme.py` (160) | 输出目录/文件既不存在也未被任何 grub cfg 或 `theme.txt` 引用 |
| `tools/generate_doxygen.py` (128) | 全仓 `doxygen` 仅命中 `buildsystem/cache/` 内 vim 文档（无关） |

### 7.2 仅被 `tools/test_*.py` 引用（生产无调用者） — 3 个

| 文件 | 唯一引用 |
| --- | --- |
| `tools/populate_exfat.py` (487) | `tools/test_make_image.py:25` |
| `tools/build_api.py` (369) | `tools/test_apiapp_qemu.py:12,110`（+ devtools 对外文档） |
| `tools/license_server.py` (720) | `tools/test_license_server.py`（经 `build.py:3729-3735` 的 `test-license-server`） |

### 7.3 仅被 README/文档指引（人工入口，构建图外） — 8 个

`analyze_boot_log.py`(447)、`check_licenses.py`(449)、`generate_linux_syscalls.py`(38，另 3 个 README 指引再生路径)、`package_musl_gcc.py`(109，+ `prepare_gcc_probe.py` import)、`package_python.py`(111)、`prepare_gcc_probe.py`(67)、`run_gcc_probe_qemu.py`(98)、`count_code.py`(940，另 CI `code-count.yml:35` 使用)。

### 7.4 统计

| 项 | 数量 |
| --- | --- |
| `tools/` 下 `.py` 总数（不含 `__pycache__`） | 200 |
| `test_*.py` | 129 |
| **生产脚本（非 `tools/test_*.py` 的 `.py`）** | **71**（任务书 69；差异来自 `tools/vscode/` 2 个 + 计数口径） |
| 非 Python 生产件（`oschinpt-apk-post-install/-deinstall`、`provision_rpr_signing_key.sh`） | 3 |
| 生产子目录 | `tools/tests/`(183 `.c` + 4 非 c)、`tools/abi/`(1 tbl)、`tools/browser_test_page/`(5 静态)、`tools/vscode/`(2 `.py` + README) |
| **完全零引用（§7.1）** | 3 |
| **生产图无调用者合计（§7.1+§7.2+§7.3）** | 14 |
| 其中判定 **(f) 可删除** | 5：`build_tcc.py`、`make_grub_theme.py`、`generate_doxygen.py`、`populate_exfat.py`（含 `test_make_image.py` 用例）、`prepare_gcc_probe.py`+`run_gcc_probe_qemu.py`（成对，2 项计 2） |
| 判定 **(e) 保留 / test-legacy** | 9：`build_api.py`、`analyze_boot_log.py`、`check_licenses.py`、`check_abi_migration.py`、`check_unix_paths.py`、`count_code.py`、`license_server.py`、`qmp_terminal_smoke.py`、`generate_linux_syscalls.py` |

---

## 8. 最难迁移项（含理由与验收钩子）

| 排序 | 项 | 为什么难 | 验收钩子 |
| --- | --- | --- | --- |
| 1 | `build_auth_upstream.py` 的 `linux-pam 1.7.2` Meson+Ninja 路径 | §2 点名；Meson-only 上游与"无 Ninja/Python"直接冲突，且交叉 argv 含大量非显然修补（`--gcc-toolchain=/nonexistent`、`--rtlib=compiler-rt`、`--unwindlib=none`、`-idirafter` 覆盖、`-include linux/openat2.h`、`CC` 内塞 `-L` 压过 libtool）。降级 PAM 或手写 Makefile 都动 ABI 与 3 个补丁 | `esp:auth` 产物集合（`build.py:791-805`）、`test_auth_upstream*.py`、`test_pam_*`、`pam_leonos_password.so`、`unix_chkpwd 4755` |
| 2 | `leonos_musl_cc.py` 生成的 SDK 编译器包装器 | 它是 §2 "包括生成的包装器"的字面对象，且同时出现在 SDK tarball、SDK zip、`build.py:1546` 生产链接命令三处；其 CRT/`--start-group` 顺序知识与 `musl_link.py` 重复 → 必须同步改否则 SDK 与内部构建 ABI 不一致 | `test_auth_sdk.py`、`test_musl_gcc_*`、`musl-probes`、最小 C 程序编译+来宾加载（§10） |
| 3 | `apk_distribution.py` | 561 行、语义密度最高：签名/索引/事务全交给上游 `apk`（§3 禁重造），但还夹带 musl-dev 重打包、ELF provider 解析、rootfs 布局、`.repackage-cache` 指纹、userns/fakeroot 双路径；且 `time.time_ns()` 版本使包仓库不可能字节重现 | `test_apk_{distribution,ownership,layout,bootstrap,qemu}`、`A15`、`A13` |
| 4 | `kconfig_sync.py` + `generate_component_kconfig.py` + `build_kconfig_frontends.py` 三件套 | 必须同时满足 §7（Kconfig 权威、不复制 defaults 到常量、生成物移出源码树、include 重启不循环）与 §4（`defconfig/olddefconfig/menuconfig` 三目标）。当前 default/preset/retired 白名单都在 Python 常量里，`Kconfig.components` 与 `include/generated/*` 还被 Git 跟踪 | `test_component_config.py`（CI 直接跑）、`test_kconfig_frontends.py`、A02/A05 |
| 5 | `prepare_ui_font.py`（290）与 `make_image.py` 的 GPT/UUID 部分 | 前者是真正的 TTF 表级改写器，仓库内没有可用的 C 字体库（FreeType 不在 `third_party/`），替代方案只有自研 C 或引入并锁定新依赖；后者每次生成随机 disk/partition GUID 并写进 guest `fstab`，直接封死 `A13` 的哈希一致性 | `test_prepare_ui_font.py`、来宾字体加载；`test_make_image.py`、`tools/tests/rootfs_gpt_test.c`、`A13` |

---

## 9. 已知缺口清单（本底稿未覆盖或需二次确认）

1. 各 `build_*.py` 的**完整编译 flag 逐条对照**（`--compile-flag/--linker-flag/--leonos-*` 与 `build.py` 的 `cflags_*`）未逐项列举，只确认了链接顺序与 ABI 例外（lua 的 SSE、`-mgeneral-regs-only`）。`tests/build/` 契约测试需要基线抓取（P0-b）来钉。
2. `tools/test_*.py` 129 个未逐个盘点（任务书限定为生产脚本）。§7.2/§8 的验收钩子仅按文件名登记，需在 P0-a 终稿补"该测试是否还依赖已退役目标"。
3. `make_ext2_root.write_ext2_root()` 的容量估算与 `make_image` 的 128MiB 最小 root 校验构成"镜像尺寸基线"，但真实分区扇区数需 P0-b 从 `build/images/*.raw` 反查确认。
4. `apk_distribution.bootstrap()` 读 `build/apk-preparation-reference/`（行 133）作为离线来源 —— 该目录不在仓库内，属本地准备；`make fetch` 设计需决定是否保留此捷径。
5. `build.py:146-147` 仍保留 `gcc-probe-image`/`gcc-probe-runner` 名称但无对应 target（`build.py:136` `BUILD_NUMBER_EXEMPT_TARGETS`），属陈旧残留；P4 需与 `package_musl_gcc.py`/`prepare_gcc_probe.py`/`run_gcc_probe_qemu.py` 一并处置。
6. `docs/BROWSER.md:87` 引用了不存在的 `tools/gen_ninja.py`；`docs/CODE_COUNT.md` 引用 `build/code-count.json`。迁移时需同步改文档（§P4"不遗漏"），本条为文档缺陷而非脚本缺陷。
