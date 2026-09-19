# Python / Ninja / Meson 调用点审计 + Linux-PAM 1.7.2 Makefile 移植技术底稿

生成时间：2026-09-19。范围：原构建重建计划（已清理，历史版本可从 Git 查看） §2 末两段、§3、§9、§12 P0。
方法：只读检查（`ls`/`grep`/`find`/`file`/`diff`/AST 解析/读取既有构建产物）。**未修改任何源码，未运行 `build.py`。**

标注约定：
- **[E]** = 已实证（附路径与行号，或可复现的只读命令）。
- **[S]** = 推测（未验证，不作为决策依据）。

主要证据源（都是仓库内既存文件，可复跑校验）：

| 证据 | 路径 |
| --- | --- |
| 上游源码（未打补丁） | `build/auth-upstream/src/Linux-PAM-1.7.2/` |
| 打补丁后的源码 | `build/auth-upstream/adapted/linux-pam-404f0405.../Linux-PAM-1.7.2/` |
| 上一次 meson 构建树 | `build/auth-upstream/linux-pam-404f040518b42d15/`（含 `build.ninja`、`compile_commands.json`、`meson-info/intro-*.json`、`.ninja_log`、`libpam/include/config.h`、`meson-logs/meson-log.txt`） |
| 上一次安装 staging | `build/auth-upstream/root/` |
| 构建元数据 | `build/auth-upstream/linux-pam-build.json`、`musl.ini` |

---

# 第一部分 任务 A：Linux-PAM 1.7.2 → 手写 Makefile 移植

## A.0 结论与难度评级（先给答案）

**可行，且比预期的简单。** 总体难度评级 **中等（3/5）**；其中"生产安装子集"（49 个链接产物）难度 **中低（2/5）**，风险集中在 *配置探测的等价* 与 *安装语义等价（soname 链 / RPATH 剥离 / setuid 模式）*，而不是编译本身。

决定性事实（全部 [E]）：

1. **45 个模块共用一份模板**。`modules/*/meson.build` 全部是符号链接指向 `modules/module-meson.build`（516 行），`md5sum` 45 份完全相同。[E] `ls -la build/auth-upstream/src/Linux-PAM-1.7.2/modules/pam_unix/meson.build` → `../module-meson.build`。
   → 模块部分可压成 **一张声明表**（模块名 → 额外源文件 / 额外宏 / 额外库 / 跳过条件），不需要 45 个 Makefile。
2. **`docs=disabled` 已经把整条 XML/xslt/docbook 链砍掉**。[E] `linux-pam-build.json` commands 中有 `-Ddocs=disabled`；`meson.build:492-592` 全部在 `if enable_docs` 内；`root/usr/share/man/` 里没有任何 PAM man 页。
   → 移植**不需要** xsltproc/xmllint/fop/w3m/xmlcatalog，也不需要 `custom-man.xsl`。
3. **flex/bison 只服务于一个不安装的可执行文件**。[E] `conf/pam_conv1/meson.build:1-25` 生成 `pam_conv_y.[ch]`/`pam_conv_l.c` 并 `executable('pam_conv1', ...)` **没有 `install:`**；`meson-info/intro-installed.json` 的 161 条里 **没有** `/sbin/pam_conv1`。
   → 生产子集**完全不需要 bison/flex**。
4. **构建期不需要以宿主方式运行任何目标端可执行文件**。[E] `musl.ini` 有 `needs_exe_wrapper = true`；`build.ninja` 里所有 `CUSTOM_COMMAND` 只有 `bison`/`flex`/`msgfmt`/`meson --internal *`（见 A.4）。目标端 `unix_chkpwd`/`faillock`/`pam_timestamp_check` 只被链接、从不被执行。`pam_cap` 不在本项目里（没有 libcap 依赖）。
5. **唯一"必须保留"的生成步骤只有 3 类**：`config.h`（34 个宏）、`pam_namespace_helper`/`pam_namespace.service`（2 个 `@SCONFIGDIR@` 文本替换）、82 个 `.mo`（`msgfmt`，外部成熟工具，非 Python）。
6. **发现的等价陷阱**：当前 meson 基线实际上**没有**应用上游的 22 个 `-W` 加固警告旗标，也没有 `-U_FILE_OFFSET_BITS`（详见 A.2.4）。这不是移植引入的，移植后若"照抄上游意图"会产出与基线不同的参数集，必须在验收口径里显式处理。

**不推荐的替代路线**：降级 PAM 版本 / 删认证 / 给 meson 开例外 —— 已被用户否决，本文不再评估。

---

## A.1 meson.build 实际定义的编译单元、生成步骤与依赖 [E]

### A.1.1 文件全集与角色

顶层 `meson.build` 663 行；子目录 74 个 `meson.build`。按角色分：

| 文件 | 行数 | 角色 |
| --- | --- | --- |
| `meson.build` | 663 | 选项/路径推导、`configuration_data`（`cdata`）全部探测、全局编译/链接旗标、外部依赖发现、`subdir()` 调度 |
| `meson_options.txt` | 105 | 40 个 option |
| `libpam/meson.build` | 73 | `libpam.so` 33 源 + `subdir('include')` + `pkgconfig.generate` |
| `libpam/include/meson.build` | 3 | **`configure_file(output:'config.h')`** + `subdir('security')` |
| `libpam/include/security/meson.build` | 10 | `install_headers` ×7 |
| `libpam_internal/meson.build` | 19 | `libpam_internal.a` 静态库，3 源 |
| `libpamc/meson.build` | 36 | `libpamc.so` 3 源 + pc |
| `libpam_misc/meson.build` | 40 | `libpam_misc.so` 2 源 + pc |
| `modules/meson.build` | 45 | 仅 45 个 `subdir()` |
| `modules/module-meson.build` | 516 | **全部 45 个模块 + 各自的 man/README/helper/test** |
| `conf/pam_conv1/meson.build` | 25 | bison/flex + 不安装的 `pam_conv1` |
| `tests/meson.build` | 48 | 19 个 `tst-pam_*` 可执行文件，不安装 |
| `xtests/meson.build` | 70 | `xtests=false`（默认）→ 不进入 |
| `examples/meson.build` | 14 | 5 个示例，不安装（`examples` 默认 true） |
| `po/meson.build` | 15 | `i18n.gettext()` → 82 个 `.mo` |
| `doc/**`（8 个文件） | ~460 | 全部在 `enable_docs` 分支内，当前 disabled |

### A.1.2 实际构建产物（来自上次真实 meson 构建，不是读代码猜的）

[E] `meson-info/intro-targets.json`：**180 个 target** = 84 `custom`（82 `.mo` + `pam_conv_y.[ch]` + `pam_conv_l.c`）+ 50 `executable` + 39 `shared module` + 3 `shared library` + 1 `static library` + 2 `run` + 1 `alias`。
[E] `build.ninja`：**158 条 `c_COMPILER` 边、92 条 `c_LINKER` 边、42 条 `SHSYM` 边**。

`modules/pam_*/` 有 6 个在配置期被探测结果 `subdir_done()` 掉，所以 45 → 39：

[E] 被跳过的 6 个：`pam_lastlog`（`option('pam_lastlog')` 默认 `disabled`，`meson_options.txt:100`）、`pam_rhosts`（无 `ruserok`/`ruserok_af`，musl 不提供，`module-meson.build:61-64`）、`pam_selinux`/`pam_sepermit`（无 libselinux，`:70-79`）、`pam_tty_audit`（无 `struct audit_tty_status`，`:99-101`）、`pam_userdb`（无 db/gdbm/ndbm，`:107-111`）。

### A.1.3 构建期生成的源文件（穷举）

| 生成物 | 机制 | 位置 | 生产子集是否需要 |
| --- | --- | --- | --- |
| `config.h` | `configure_file(output:'config.h', configuration:cdata)` | `libpam/include/meson.build:1`，输出到 build 树 `libpam/include/config.h` | **需要**（唯二的头注入点） |
| `pam_conv_y.c` + `pam_conv_y.h` | `custom_target` + `yacc_cmd`（bison `-o @OUTPUT0@ --defines=@OUTPUT1@ @INPUT@`，`meson.build:620-636`） | `conf/pam_conv1/meson.build:1-7` | 不需要（不安装） |
| `pam_conv_l.c` | `custom_target` + `flex -o @OUTPUT@ @INPUT@` | `conf/pam_conv1/meson.build:8-14` | 不需要 |
| `pam_namespace_helper` | `configure_file(input:'pam_namespace_helper.in', configuration:cdata)` | `module-meson.build:331-335` | **需要**，装到 `/sbin`，只替换 `@SCONFIGDIR@` 一个 token |
| `pam_namespace.service` | `configure_file(input:..., configuration:cdata)` | `module-meson.build:341-345` | **需要**，装到 `/usr/lib/systemd/system` |
| `Linux-PAM.pot` + `msgattr` | `i18n.gettext()`，meson Python 模块 | `po/meson.build:6` | 2 个 `run` target，`meson compile` 不跑 |
| 82 × `po/<lang>/LC_MESSAGES/Linux-PAM.mo` | `custom_target`，命令 = `/usr/bin/msgfmt -o ... <lang>.po` | `po/meson.build` 生成 | **需要**（装到 `/usr/share/locale`） |
| `pam.pc` / `pamc.pc` / `pam_misc.pc` | `pkgconfig.generate()`（meson Python 模板） | `libpam/meson.build:64`、`libpamc/meson.build:31`、`libpam_misc/meson.build:35` | **需要**，3 个 10 行文本文件 |
| `<lib>.so` + `<lib>.so.<soname>` 符号链接 | `shared_library(version:)` 隐式 | — | **需要** |
| `*.symbols`（42 个） | `SHSYM` = `/usr/bin/meson --internal symbolextractor` | `build.ninja:31-34` | **纯 meson 内部产物，不需要移植** |
| man 页 / `README.html` / `<module>.txt` | `custom_target` + xsltproc + `aux/redir_exe.sh` + w3m/elinks | `module-meson.build:167-256` | disabled，不需要 |
| `custom-man.xsl` | `configure_file` | `doc/meson.build` | 不需要 |

**注意 `*.pc.in` 是死文件**：[E] `grep -rn "pc.in" */meson.build meson.build` 零命中；`root/lib/pkgconfig/pam.pc` 缺 `exec_prefix` 行且键序与 `libpam/pam.pc.in` 不同 → 证实由 `pkgconfig.generate` 而非模板产出。移植时直接按 meson 实际输出写 3 个 10 行文件即可（内容见 A.5.5）。

**没有任何 `generator()` 用法**，也没有 header 生成脚本、`.c` 生成器。[E] `grep -rn "generator(" --include=meson.build .` 零命中。`pam_appl.h` 等 9 个安装头全部是静态文件（`find -name '*.h.in'` 零命中）。

### A.1.4 依赖关系（真实链接图）

[E] 取自 `build.ninja` 的 `LINK_ARGS`：

```
libpam_internal.a          <- pam_debug.c pam_econf.c pam_line.c            (无外部依赖)
libpam.so.0.85.1           <- 33 objects + libpam_internal.a
                             --version-script=libpam/libpam.map  -Wl,-soname,libpam.so.0
libpamc.so.0.82.1          <- 3 objects  + libpam_internal.a                 map: libpamc.map
libpam_misc.so.0.82.1      <- 2 objects  + libpam_internal.a + libpam.so     map: libpam_misc.map
modules/*.so (shared_module) <- 自身 objects + libpam_internal.a + libpam.so + <可选 libcrypt.so>
                             --version-script=modules/<m>/module.map，name_prefix 空，**无 soname**
sbin/unix_chkpwd 等 exe     <- 同上 + -pie（project default_options b_pie=true）
```

模块 map 文件是 **每个模块一份 `module.map`**（源码里就有），移植时必须原样传给链接器 —— 它决定导出符号集，是 ABI 的一部分。

---

## A.2 交叉编译选项与被 `dependency()` 发现的外部库 [E]

### A.2.1 `musl.ini`（由 `build_auth_upstream.py:103-113` 生成）逐项语义

| 段/键 | 值 | 语义 |
| --- | --- | --- |
| `[binaries] c` | `['clang','--target=x86_64-linux-musl','--sysroot=<musl>','--gcc-toolchain=/nonexistent','-fuse-ld=lld','--rtlib=compiler-rt','--unwindlib=none','-nostdinc','-isystem <musl>/include','-isystem /usr/lib/clang/22/include','-idirafter <linux-headers>']` | 整个 argv 以列表传入，不走 PATH 上的默认 clang |
| `ar`/`strip`/`pkg-config` | `llvm-ar` / `llvm-strip` / `pkg-config` | |
| `[host_machine]` | `system=linux, cpu_family=x86_64, cpu=x86_64, endian=little` | 显式声明 endian，**没有**任何 endian 探测代码（`grep endian` 在 `meson.build` 中只出现在 cross 文件） |
| `[properties] needs_exe_wrapper` | `true` | 禁止 meson 执行目标端可执行文件 |
| `[properties] sys_root` | `<work>/root` | pkg-config sysroot 前缀 |
| `[properties] pkg_config_libdir` | `[<root>/lib/pkgconfig, <root>/usr/lib/pkgconfig]` | **只**搜 staging，屏蔽宿主 .pc |
| `[built-in options] c_args` | `['-O2','-mno-avx','-mno-avx2']` | |

`--gcc-toolchain=/nonexistent` 是用来压制 clang 对宿主 GCC 的隐式发现的（[S] 推断，未查 clang 文档）。

### A.2.2 命令行 `--` 选项

`build_auth_upstream.py:157-162` [E]：`--prefix=/usr --libdir=/lib --sbindir=/sbin --sysconfdir=/etc --localstatedir=/var`。
→ 影响 `_PAM_ISA`（`../../lib/security`）、`SECUREDIR`、`SYSCONFDIR`、`SCONFIGDIR`、`sbindir` 注入到各 `-D*_HELPER=` 宏。

### A.2.3 四个 `-D` 选项的确切语义

| 选项 | 语义（源码位置） | 实际效果 |
| --- | --- | --- |
| `-Dsecuredir=/lib/security` | `meson.build:30-34`：非空则直接用；空则 `libdir/security` | 因为 `--libdir=/lib`，**值与默认推导相同**，属"显式化"而非改动。作用面：模块安装目录（`module-meson.build:151 install_dir:securedir`）+ `libpam` 编译期 `-DDEFAULT_MODULE_PATH="/lib/security/"`（`libpam/meson.build:53`） |
| `-Dpam_unix=enabled` | `meson.build:436`：`enable_pam_unix = not disabled()`；但 `enabled` 额外把若干缺失变成 **hard error**：`lckpwdf` 缺失（`:437-444`）、`nis=enabled` 时接口缺失（`:484-486`）、`userdb=enabled` 缺 db（`:425-427`） | 语义 = "pam_unix 必须建成，否则配置期失败"。默认是 `auto`（`meson_options.txt:102`），auto 下 musl 若无 `lckpwdf` 会静默降级 → 用 `enabled` 是 **防静默丢失 pam_unix 的守卫**。移植后等价实现 = Make 末尾显式断言 `pam_unix.so` 存在，缺失即 fail |
| `-Dvendordir=` （空） | `meson.build:88-102` | `vendor_sconfigdir = sconfigdir = /etc/security`、`vendor_sysconfdir = sysconfdir = /etc`；**不**定义 `VENDORDIR`/`VENDOR_SCONFIG_DIR`；docbook profile 条件 `without_vendordir`。效果：发行版提供的 `/etc/security/*.conf` 与 `/etc/environment` **直接写进 /etc**，而不是 `/usr/share/pam` 覆盖层（默认值是 `/usr/share/pam`，`meson_options.txt:90`）。这是"配置只有一个权威来源"的产品决定 |
| `-Ddocs=disabled` | `meson.build:492` `enable_docs = not disabled()` | 跳过整个 `subdir('doc')` 及所有 man/README 的 `custom_target` 与 `run_command`；连带让 `prog_xsltproc/xmllint/fop/xmlcatalog/browser` 全变 `disabler()`（`:594-603`）。**这就是为什么移植不需要任何 XML 工具链** |

`meson compile` 传 `-j12`；`meson install --destdir <root>`；再次进入时加 `--reconfigure --clearcache`（`:160-162`）。

### A.2.4 `dependency()` / `find_library()` 的实际解析结果

[E] 权威来源：`meson-info/intro-dependencies.json` —— **只有 3 个真正落地**：

| dep | 类型 | 解析结果 | 影响的宏/目标 |
| --- | --- | --- | --- |
| `dl` | **builtin** | musl 下为空（`dlopen` 在 libc 内） | `libpam` 依赖位，无链接旗标 |
| `intl` | **builtin**，`found: YES` | 由 **LeonOS musl 自身的 GNU 兼容 gettext** 满足：`third_party/musl/src/locale/dcngettext.c:46` 定义 `bindtextdomain`，`include/libintl.h:24` 声明；`build/musl/sysroot/lib/libc.so` 导出 9 个 `*textdomain/*gettext` 符号 | `ENABLE_NLS=1`、`LOCALEDIR="/usr/share/locale"`、`HAVE_BINDTEXTDOMAIN`、`HAVE_DNGETTEXT` → **82 个 `.mo` 是有运行时意义的，不能随手砍掉** |
| `libcrypt` | **pkgconfig**，v4.5.2 | `libcrypt`/`libxcrypt` 两条名字，命中 staging 的 `libxcrypt.pc`：`-I<root>/usr/include` + `<root>/lib/libcrypt.so` | `HAVE_CRYPT_H`、`HAVE_CRYPT_R`、`HAVE_CRYPT_RN`；`pam_unix`/`pam_pwhistory`/`pam_userdb` 依赖位 |

未解析（自动 disabled，不产生任何宏）：`audit`、`libeconf`、`libselinux`、`libpwaccess`、`libcrypto`(openssl 默认 disabled)、`libsystemd`/`libelogind`、`libtirpc`/`libnsl`(NIS)。
`find_library('util')`（pam_lastlog）、`db/gdbm/ndbm`（pam_userdb）也未命中。

→ **移植时的外部库集合就三个：libxcrypt（必需，且是唯一 pkg-config 探测）、musl 自带 libc（dl/intl 都由它满足）。** libmd/libbsd **不参与 PAM**（它们只被 shadow/libbsd 链使用，见 `build_auth_upstream.py:126-128` 的 `libbsd→libmd`、`shadow→libbsd` 依赖闭包）。

`libxcrypt` 必须先于 `linux-pam` 安装到 staging：[E] `build_auth_upstream.py:122-123`（`if "linux-pam" in selected: selected.add("libxcrypt")`），`order` 元组把 libxcrypt 排在 linux-pam 之前。

### A.2.5 全局编译/链接旗标的"真实值"（重要陷阱）

[E] 代码意图 vs `build.ninja` 实际：

| 意图（`meson.build`） | 实际进入命令行的 |
| --- | --- |
| `add_project_arguments(cc.get_supported_arguments(try_cc_flags))`，22 个 `-W` 旗标（`:137-163`） | **一个都没有** |
| `add_project_arguments('-U_FILE_OFFSET_BITS')`（`:128-133`，条件 `cc.sizeof('long')>=8`） | **没有**（探测同样失败，条件为假） |
| `add_project_link_arguments(['-Wl,--fatal-warnings','-Wl,--no-undefined-version','-Wl,-O1'])`（`:165-171`） | **有**，见 `LINK_ARGS` |
| `warning_level=2`、`buildtype` 默认 `debug` | 有：`-Wall -Winvalid-pch -Wextra -O0 -g` 然后叠加 `-O2 -mno-avx -mno-avx2`（后者胜）；`-D_FILE_OFFSET_BITS=64` 是 meson 默认注入的 |

根因 [E]：`meson-logs/meson-log.txt:61-64`

```
Command line: `clang ... -fuse-ld=lld --rtlib=compiler-rt ... -c ... -Werror=unused-command-line-argument -Wbad-function-cast` -> 1
clang: error: argument unused during compilation: '-fuse-ld=lld' [-Werror,-Wunused-command-line-argument]
clang: error: argument unused during compilation: '--rtlib=compiler-rt' [-Werror,-Wunused-command-line-argument]
```

meson 的"参数是否支持"探测用**只编译不链接**的方式跑，而 cross 文件把 `-fuse-ld=lld`/`--rtlib=compiler-rt` 混进了 `c = [...]` 主命令；这两个旗标在 `-c` 阶段未使用 → meson 自带的 `-Werror=unused-command-line-argument` 把探测判失败 → 22 个警告旗标被静默丢弃（`grep -c "Compiler for C supports arguments"` = 3318 次，全部 NO）。

**这是一条独立于本次迁移的现存缺陷**，应当在迁移台账里记账：基线产物**不含**上游加固警告集。移植方案必须显式声明自己走哪条路（见 A.6.3 判据 P3）。

---

## A.3 `patches/linux-pam/` 三个补丁 [E]

| 补丁 | 触及文件 | 是否碰 meson 文件 | 移植后如何继续应用 |
| --- | --- | --- | --- |
| `0001-reject-unavailable-salt-entropy.patch`（103 行） | `modules/pam_unix/passverify.c` | 否 | 纯 C，`patch -p1` 原样应用 |
| `0002-use-sized-libcrypt-entry-point.patch`（154 行） | `meson.build`(+1/-1)、**新增** `libpam/include/pam_crypt.h`、`modules/pam_unix/passverify.c`、`modules/pam_unix/bigcrypt.c`、`modules/pam_pwhistory/opasswd.c`、`modules/pam_userdb/pam_userdb.c` | **是** | 需要**拆分**：C/头文件部分保留为上游补丁；`meson.build` 的 `foreach f: ['crypt_r']` → `['crypt_r','crypt_rn']` 那一格迁到"我们自己的探测清单"里 |
| `0003-limits-handle-unimplemented-resources.patch`（39 行） | `modules/pam_limits/pam_limits.c` | 否 | 纯 C |

验证 [E]：`diff -u src/.../meson.build adapted/linux-pam-404f.../meson.build` 的**唯一**差异就是 `crypt_rn` 那一行；`grep -n crypt_rn meson.build` → `260:`。

0001 与 0002 都改 `passverify.c` → **顺序敏感**，现有脚本按文件名排序串行应用（`build_auth_upstream.py:130` `sorted(glob('*.patch'))`），移植后保持同一顺序即可。

**移植后的补丁策略建议**（对应 §9"补丁在配置专属工作目录应用"）：
把 0002 拆成 `0002a-*`（仅 C + 新头，进 `patches/linux-pam/`）和"探测项 `crypt_rn`"（进我们自己的 config 生成器输入清单）。理由：给 meson.build 打补丁在 Make 路线上没有落点，保留会让补丁永远冲突。
注意 `pam_userdb.c` 那一段修改的是一个**根本不构建**的模块（无 db），拆补丁时可保留（无害）但应在台账中注明"该 hunk 在生产配置下不产生任何效果"。

旁证 [E]：`adapted/linux-pam-404f.../modules/pam_unix/passverify.c.orig` 与 `.../pam_pwhistory/opasswd.c.orig` 存在 → 说明当前 `patch` 调用留下了 `.orig`（`build_auth_upstream.py:135` 用 `--batch --forward --fuzz=0`，但 `.orig` 来自 `patch` 对新建文件的处理）。移植脚本应显式 `--no-backup-if-mismatch` 或用干净目录，避免 `.orig` 被 glob 成源文件。

---

## A.4 交叉工具链 / sysroot 传入，与"宿主方式跑一次"需求

### A.4.1 工具链传入方式

[E] **PAM 这条路径不使用 `leonos_musl_cc.py`**。`build_auth_upstream.py:90-96` 自己拼了一份 clang argv 并同时注入三处：环境变量 `CC`（给 autotools 包）、`musl.ini [binaries].c` 列表（给 meson）、`build.json.compiler`（记账）。`leonos_musl_cc.py` 只在 SDK 侧出现（A.4.3）。

同一份 argv 里有两个"绝对宿主耦合"点，移植时必须参数化：
- `-isystem /usr/lib/clang/22/include` ← `clang -print-resource-dir` 的产物（`build_auth_upstream.py:87`），版本号硬写在缓存里；
- `--sysroot=<abs>/build/musl/sysroot`、`-idirafter <abs>/build/linux-6.12-headers/include`。

Linux 头文件由 `make ARCH=x86_64 headers_install` 现场生成（`build_auth_upstream.py:85`）—— 这是**纯 Make**，可直接留在新体系里（§9 允许）。

musl sysroot 的一致性由 `.leonos-musl.json` 的 `sources`/`target` 双校验保证（`:72-75`），移植要保留同等检查。

### A.4.2 目标端可执行文件"以宿主方式跑一次"清单 —— **PAM：空**

[E] 穷举 `build.ninja` 里所有 `CUSTOM_COMMAND` 的 `COMMAND =`（去重）只有：
`/usr/bin/bison`、`/usr/bin/flex`、`/usr/bin/msgfmt ×82`、`/usr/bin/meson` 自身（`dist`/`install --no-rebuild`/`--internal cleantrees`/`--internal gettext pot`/`--internal gettext update_po`/`--internal scanbuild`/`--internal uninstall`/`meson test`）、以及 42 条 `SHSYM` = `/usr/bin/meson --internal symbolextractor ...`。

结论：**没有任何目标端 ELF 在构建期被执行**。`needs_exe_wrapper=true` + `meson test` 未被调用（生产链只跑 `meson setup/compile/install`）。所以 `pam_cap`（本项目无 libcap，不构建）、`unix_chkpwd`（只链接）都不需要 host 版。

**endian / 尺寸探测**：[E] `meson.build` 里唯一的类型探测是 `cc.sizeof('long') >= 8`（`:116`，因 A.2.4 的原因当前实际为"探测失败→跳过"）与 `cc.sizeof('struct audit_tty_status')`（`:241`，audit 不存在所以不执行）。endian 完全来自 cross 文件声明。**没有 `cc.run()`、没有 `alignmentof` 探测。**

### A.4.3 需要顺带注意的"宿主跑一次"邻居（非 PAM，但在同一子图）

| 项 | 证据 | 性质 |
| --- | --- | --- |
| ncurses 需要宿主 `tic`/`infocmp` | `tools/build_terminal_packages.py:51` | 已在生产链，宿主二进制，非 Python |
| `file-magic` 用**宿主** autoreconf+configure+make 生成 `magic.mgc` | `tools/build_file_magic.py:37-53` | autotools，§9 允许 |
| SQLite 融合体生成需要 **`tclsh`** | `tools/build_sqlite.py:84` → `third_party/sqlite/main.mk:662-663`（`tclsh tool/mksqlite3c.tcl`）、`:731-732`（`tclsh tool/mksqlite3h.tcl`）；`src-verify` 用宿主 `$(BCC)` 编译后执行 | 见 B.4 |
| LTP `make autotools` | `tools/build_musl_ltp.py:58` | autotools |

---

## A.5 最小可行移植方案

### A.5.0 规模结论

| 维度 | 数量 |
| --- | --- |
| 需要手写的 Makefile | **3 个**：`Makefile`（入口+驱动）、`modules.mk`（模块声明表）、`config.mk`（探测与 config.h 生成）。可选第 4 个 `install.mk` |
| 生产子集的链接产物 | **49**：1 `libpam_internal.a` + 3 `shared library` + 39 `shared module` + 6 `executable`（`faillock`、`mkhomedir_helper`、`pwhistory_helper`、`pam_timestamp_check`、`unix_chkpwd`、`upperLOWER`） |
| 生产子集的 `.o` 集合 | **49 组**（每产物一组对象，**目录必须按产物分**，见 A.6.2）；总 `.o` 数 **110** |
| 必须保留的生成步骤 | **3 类**：`config.h`（1）、`@SCONFIGDIR@` 文本替换（2 文件）、`msgfmt`（82）；外加 3 个 `.pc`（静态文本）与 6 个符号链接 |
| 完全丢弃的 meson 步骤 | `SHSYM`(42)、`REGENERATE_BUILD`、bison/flex（若不做 `pam_conv1`）、doc/**、xtests |
| 可选（不安装）子集 | `pam_conv1`(需 bison+flex)、`bigcrypt`、`hmacfile`、5 examples、19+17 tests |

对象数核算 [E]：`3 + 33 + 3 + 2`（库）+ `55`（39 模块：34×1 + faillock3 + namespace3 + pwhistory3 + timestamp3 + unix9）+ `14`（6 个可执行：3+1+2+1+6+1）= **110**；含非安装子集 = 158 = `build.ninja` 的 `c_COMPILER` 边数。✓ 交叉验证通过。

### A.5.1 目录/变量骨架

```
mk/third-party/pam/
  Makefile          # 49 个产物 + install 目标；递归自顶层 make，继承 jobserver（§6.1）
  config.mk         # 路径推导（prefix/libdir/sbindir/sysconfdir/securedir/vendordir）
  probe.mk          # 探测清单 -> 驱动 gen-config-h
  modules.mk        # 模块声明表（45 行）
  gen-config-h.c    # tools/host 下的 C 工具：读 probe 结果 + 路径 -> 写 config.h（write-if-changed）
  subst.c           # 或者复用同一个 C 工具做 @SCONFIGDIR@ 替换
```

关键：把 meson 的 `cdata` 变成**一个显式的、被版本控制的探测清单**（哪些头/函数需要探测 + 期望值），而不是每次构建重新猜。理由见 A.6.1。

### A.5.2 config.h 生成（唯一真正需要"重新实现"的部分）

[E] 目标产物就是现成的：`build/auth-upstream/linux-pam-404f040518b42d15/libpam/include/config.h`，**34 个宏**，逐条来源分类：

| 类别 | 条目 | 移植做法 |
| --- | --- | --- |
| 常量（无需探测） | `_GNU_SOURCE`、`PACKAGE`、`PAM_VERSION`、`UNUSED`、`PAM_NO_HEADER_FUNCTIONS`、`LTDIR` | 直接写 |
| 纯路径 | `SYSCONFDIR`、`sbindir`、`SCONFIGDIR`、`SCONFIG_DIR`、`LOCALEDIR`、`_PAM_ISA`、`PAM_PATH_RANDOMDEV` | 由 `config.mk` 传入 |
| 选项直通 | `DEFAULT_USERGROUPS_SETTING`、`PAM_USERTYPE_UIDMIN`、`PAM_USERTYPE_OVERFLOW_UID`、`PAM_MISC_CONV_BUFSIZE`、`PAM_DEBUG`、`PAM_LOCKING`、`PAM_READ_BOTH_CONFS`、`PAM_UNIX_TRY_GETSPNAM` | 来自 profile，值已知 |
| **需要探测：头（3）** | `HAVE_CRYPT_H`、`HAVE_PATHS_H`、`HAVE_SYS_RANDOM_H` | `$(CC) -E` 存在性 |
| **需要探测：函数（18）** | `HAVE_EXPLICIT_BZERO`、`HAVE_GETDOMAINNAME`、`HAVE_GETGRGID_R`、`HAVE_GETGRNAM_R`、`HAVE_GETGROUPLIST`、`HAVE_GETMNTENT_R`、`HAVE_GETPWNAM`、`HAVE_GETPWNAM_R`、`HAVE_GETPWUID_R`、`HAVE_GETRANDOM`、`HAVE_GETSPNAM_R`、`HAVE_UNSHARE`、`HAVE_QUOTACTL`、`HAVE_LCKPWDF`(USE_LCKPWDF 前置)、`HAVE_CRYPT_R`、`HAVE_CRYPT_RN`、`HAVE_BINDTEXTDOMAIN`、`HAVE_DNGETTEXT` | 见 A.6.1：**必须交叉链接、绝不执行** |
| **需要探测：宏取值（2）** | `PAM_PATH_MAILDIR` = `_PATH_MAILDIR`（取自 `<paths.h>` 的 `get_define`）或字面 `"/var/spool/mail"`；`USE_LCKPWDF` | `$(CC) -E` 提取 |
| **由缺失即不定义（反向）** | `HAVE_GETUTENT_R`、`HAVE_INNETGR`、`HAVE_RUSEROK`、`HAVE_RUSEROK_AF`、`HAVE_CLOSE_RANGE`、`HAVE_MEMSET_EXPLICIT`、`HAVE_DBM_*`、`WITH_SELINUX`、`USE_ECONF`、`USE_PWACCESS`、`HAVE_NIS`、`USE_LIBSYSTEMD`、`USE_LOGIND` 等 | 同一探测循环，未命中即 `#undef` |

**关键要求**：探测必须产出一个**可 diff、可版本控制**的结果文件（例如 `config.probed.txt`），并做"内容不变 → 不更新 mtime"（§6.3），否则每次构建都会重编 110 个对象。

### A.5.3 模块声明表（从 `module-meson.build` 逐条翻译，[E]）

| 模块 | 额外源 | 额外宏 | 额外库 | 构建条件 |
| --- | --- | --- | --- | --- |
| 通用（其余 34 个） | 自身 `<m>.c` | — | `libpam_internal.a`,`libpam.so` | — |
| pam_env | — | — | `libeconf`(NF) | — |
| pam_faillock | `faillock.c`,`faillock_config.c` | — | `libaudit`(NF) | — |
| pam_issue | — | — | `logind_dep`(NF) | — |
| pam_keyinit | — | — | — | `__NR_keyctl` 可得（`meson.build:204`，走 `get_define`，不链接） |
| pam_lastlog | — | — | `libutil` | **不构建**（option 默认 disabled） |
| pam_limits | — | `-DLIMITS_FILE_DIR="/etc/security/limits.d"` | logind(NF) | — |
| pam_mkhomedir | — | `-DMKHOMEDIR_HELPER="/sbin/mkhomedir_helper"` | — | — |
| pam_namespace | `md5.c`,`argv_parse.c` | — | libselinux(NF) | `HAVE_UNSHARE=1` |
| pam_pwhistory | `opasswd.c`,`pwhistory_config.c` | `-DPWHISTORY_HELPER=...` 仅当 selinux（当前**不定义**） | libcrypt, libselinux(NF) | — |
| pam_rhosts | — | — | — | **不构建**（无 ruserok*） |
| pam_rootok | — | — | libselinux/libaudit(NF) | — |
| pam_selinux / pam_sepermit | — | sepermit: `-DSEPERMIT_LOCKDIR="/run/sepermit"` | libselinux | **不构建** |
| pam_setquota | — | — | — | `HAVE_QUOTACTL=1` |
| pam_shells | — | — | libeconf(NF) | — |
| pam_timestamp | `hmacsha1.c`,`sha1.c`（无 OpenSSL 分支） | — | libcrypto(NF), logind(NF) | — |
| pam_tty_audit | — | — | — | **不构建** |
| pam_unix | `bigcrypt.c pam_unix_acct.c pam_unix_auth.c pam_unix_passwd.c pam_unix_sess.c support.c passverify.c md5_good.c md5_broken.c`（**替换**默认源） | `-DCHKPWD_HELPER="/sbin/unix_chkpwd"` `-DUPDATE_HELPER="/sbin/unix_update"` | libcrypt, libselinux(NF), libtirpc(NF), libnsl(NF) | `enable_pam_unix` |
| pam_userdb | — | — | libdb, libcrypt | **不构建** |
| pam_xauth | — | — | libselinux(NF) | — |

**不构建的 6 个**（`pam_lastlog`/`pam_rhosts`/`pam_selinux`/`pam_sepermit`/`pam_tty_audit`/`pam_userdb`）必须在移植后**继续不构建**：`root/lib/security/` 现在就没有它们，凭空多出来就是行为变更。

### A.5.4 编译/链接旗标（照抄基线，不"顺手改进"）

[E] 逐字取自 `build.ninja`：

```
CC      = clang --target=x86_64-linux-musl --sysroot=... --gcc-toolchain=/nonexistent \
          -fuse-ld=lld --rtlib=compiler-rt --unwindlib=none -nostdinc \
          -isystem <musl>/include -isystem <clang-res>/include -idirafter <linux-headers>
CFLAGS  = -Wall -Wextra -O2 -g -mno-avx -mno-avx2 -D_FILE_OFFSET_BITS=64 -fPIC
          + -I<gen>（config.h 所在目录） -Ilibpam/include -Ilibpam_internal/include
          + 模块私有 -D（A.5.3 表）
          + libpam 专有：-DDEFAULT_MODULE_PATH="/lib/security/" -DLIBPAM_COMPILE
          + tests 专有： -DLIBPAM_COMPILE
          + pam_conv1 专有：-Wno-unused-function -Wno-sign-compare
LINK-SO  = -shared -fPIC -Wl,--as-needed -Wl,--no-undefined -Wl,--fatal-warnings \
           -Wl,--no-undefined-version -Wl,-O1 -Wl,-z,relro,-z,now \
           -Wl,--version-script=<map> [-Wl,-soname,libpam.so.0] --start-group ... --end-group
LINK-MOD = 同上但 **不加 soname**，且带 -Wl,--allow-shlib-undefined（meson 对 shared_module 的行为）
LINK-EXE = -pie + exe_link_args(-Wl,-z,relro,-z,now)
```

差异点必须保留 [E]：`libpam.so` 链接用 `-Wl,--no-undefined`，而模块用 `-Wl,--allow-shlib-undefined`（`build.ninja:1474`）。这不是笔误：模块允许引用 libpam 尚未导出的符号。

### A.5.5 安装步骤（等价清单）

[E] `meson-info/intro-installed.json` 共 **161 条 = 79 非 `.mo` + 82 `.mo`**。分组：

```
/etc/environment                                        (pam_env/environment)
/etc/security/{access,faillock,group,limits,namespace,pam_env,pwhistory,time}.conf
/etc/security/namespace.init                            (mode 0755)
/etc/security/limits.d/                                 (空目录 install_emptydir)
/etc/security/namespace.d/                              (空目录)
/lib/libpam.so            -> libpam.so.0 -> libpam.so.0.85.1
/lib/libpamc.so           -> libpamc.so.0 -> libpamc.so.0.82.1
/lib/libpam_misc.so       -> libpam_misc.so.0 -> libpam_misc.so.0.82.1
/lib/security/pam_*.so                                  ×39
/lib/security/pam_filter/upperLOWER                     (可执行)
/lib/pkgconfig/{pam,pamc,pam_misc}.pc
/sbin/{faillock,mkhomedir_helper,pam_namespace_helper,pam_timestamp_check,pwhistory_helper,unix_chkpwd}
/usr/include/security/{_pam_compat,_pam_macros,_pam_types,pam_appl,pam_client,pam_ext,pam_filter,pam_misc,pam_modules,pam_modutil}.h   ×10
/usr/lib/systemd/system/pam_namespace.service
/usr/share/locale/<82 lang>/LC_MESSAGES/Linux-PAM.mo
```

另有 **我们自己的** `lib/security/pam_leonos_password.so`（`build_auth_upstream.py:229-247`，独立 clang 命令，与 meson 无关，可原样搬）。

**`/etc/pam.d/*` 不来自 Linux-PAM**：meson 全树无 `pam.d` 安装语句（`grep` 确认），`root/etc/pam.d/{chfn,chpasswd,chsh,login,newusers,passwd}` 由 shadow 的 `make install` 提供。移植时不要把它算进 PAM 的验收清单。

**特权元数据**：`/sbin/unix_chkpwd` = `uid 0, gid 0, mode 4755`（[E] `build_auth_upstream.py:214-218` 与 `linux-pam-build.json:image_permissions`）。staging 里是 04755（[E] `file sbin/unix_chkpwd` → `setuid ELF`）。真正的 owner/mode 强制发生在镜像打包阶段，移植要保留这条双层契约。

### A.5.6 非安装子集怎么处置

建议：**默认不构建**，作为显式 `make pam-tests` 目标（`build_by_default` 语义）。理由：`pam_conv1` 需要 bison+flex，`tests/` 的 42 个可执行文件需要 `tst-dlopen` + `aux/chdir_meson_build_subdir.sh` 才能跑，且它们**从不参与 rootfs**。基线构建它们只是 meson "所有 target 默认全建"的副作用。

⚠ 这是与基线的**一处有意的产物差异**（基线多产出 48 个不安装的 ELF）。必须在台账里写明，不能被"PAM 装好了"这句话掩盖。

---

## A.6 最难 / 最容易出错的部分

### A.6.1 【最难】config.h 探测的等价性
18 个函数探测里，**至少 12 个在 musl 上是"链接才能确定"**的（`getgrgid_r`、`getspnam_r`、`lckpwdf`、`crypt_rn` 等）。手写时常见的两个错：
- 用 `-c` 只编译 → 全部误判为"存在"→ `pam_unix` 链期才炸，或者更糟：静默生成错误 `#undef` 组合；
- 用宿主编译器探测 → glibc 结果 ≠ musl 结果（例如 `HAVE_GETUTENT_R`、`HAVE_INNETGR` 在 glibc 下为真，会把 `pam_rhosts` 错误地纳入构建，产出一个基线里不存在的模块）。
**并且 A.2.4 已经证明这套探测在现网 meson 路线上就已经错过一次。** 建议把探测结果作为**签入仓库的固定清单**（配合 §9 的"依赖只读消费"），每次构建只做校验而不是重探，探测变化需要显式 `make pam-reprobe`。这也顺带满足 §7"相同输入不更新时间"。

### A.6.2 【高频坑】对象文件命名冲突
同一 `.c` 在多个产物里各编一份，且**编译宏不同**：
- `faillock.c` + `faillock_config.c` → `pam_faillock.so` 与 `sbin/faillock`；
- `opasswd.c` → `pam_pwhistory.so`（无 `-D`）与 `pwhistory_helper`（`-DHELPER_COMPILE="pwhistory_helper"`）；
- `bigcrypt.c` → `pam_unix.so`、`unix_chkpwd`、`bigcrypt` 三份；
- `md5_good.c`/`md5_broken.c`/`passverify.c`/`audit.c` → 模块与 helper 两份；
- `hmacsha1.c`/`sha1.c` → `pam_timestamp.so` 与 `hmacfile`。
必须按产物分对象目录（§6.1 已要求）。混用会导致 helper 里带进模块的宏或反之。

### A.6.3 【静默差异】meson 的 install 期 RPATH 剥离
[E] `build.ninja:1474` 链接时带 `-Wl,-rpath,$ORIGIN/../../libpam:<abs>/build/auth-upstream/root/lib`，但 `llvm-readelf -d root/lib/security/pam_unix.so` **没有 RPATH/RUNPATH**，`root/sbin/unix_chkpwd` 也没有。
→ meson 在 `meson install` 阶段改写掉了。手写 Make 若"链接即安装"会把**开发者绝对工作区路径写进所有 39 个模块和 6 个 helper**，同时违反 §9 与 §10（SDK 不得嵌入绝对工作区路径）的可移植精神，还会毁掉可复现性。
正确做法：链接期用 `-Wl,-rpath-link=<staging>/lib`（只解析、不写入）而非 `-rpath`。验收判据 P5。

### A.6.4 【ABI】版本脚本与 `-Wl,--no-undefined-version`
42 个 map 文件（`libpam.map`、`libpamc.map`、`libpam_misc.map`、45×`module.map`）。`-Wl,--no-undefined-version` + `--fatal-warnings` 意味着版本脚本引用了未定义符号时**链接失败**——这是上游的 ABI 守卫，必须保留。若移植时因为某个符号在 musl 下未定义就顺手删掉 `--no-undefined-version`，等于放开 ABI。

### A.6.5 【符号链接与打包】三件套 version/soname/link
`shared_library(version:'0.85.1')` → 实体 `libpam.so.0.85.1` + `SONAME=libpam.so.0` [E] `readelf -d` + 两条链。rootfs/APK 清单必须把符号链接作为独立条目（type=symlink）表达，而不是复制文件。

### A.6.6 【范围蔓延】82 个 `.mo`
`i18n=auto` + 宿主有 `msgfmt` → 82 个安装项。它们**有运行时意义**（musl 自带 gettext 会加载，见 A.2.4 表）。移植要么保留 `msgfmt`（外部 C 程序，§5 允许），要么显式改变产品决定并记账。**不能因为"只是翻译文件"就悄悄丢掉**——那会让 79 条安装项变成 161-82=79 的伪等价通过。

### A.6.7 【构建目录复用】`--reconfigure --clearcache`
`build_auth_upstream.py:160-162` 在已存在 `build.ninja` 时加 `--reconfigure --clearcache`，即每次强制重探。移植后如果沿用"探测一次即固化"，需要保证补丁摘要变化会让缓存签名失效（现脚本已经用 `linux-pam-<patch_digest>` 目录名做这件事，[E] `:131-133`）。要保留等价的失效键：**上游摘要 ⊕ 补丁摘要 ⊕ profile 摘要 ⊕ 工具链身份**。

---

## A.7 可测试的等价验收判据（不以"能编译"为准）

> 参照物 = 仓库现存的 `build/auth-upstream/root/`（meson 基线）。所有判据都可用只读命令复核。

| # | 判据 | 命令（示意） | 通过条件 |
| --- | --- | --- | --- |
| P1 | 安装清单逐条相等 | 把新 `make install DESTDIR=` 产物路径排序 vs `intro-installed.json` 的 161 条 | 集合差为空；任何新增（如 `pam_rhosts.so`）/缺失（如某个 `.mo`）都是失败 |
| P2 | ELF 集合与类型 | 对 79 个非 `.mo` 项逐个 `file` + `llvm-readelf -h` | 类型一致：39 个模块 = `shared object` 且**无** SONAME；3 库 = `shared object` 且有 SONAME；6 helper + upperLOWER = `pie executable`/`shared object` |
| P3 | 导出符号集 | 对 3 个库 `llvm-readelf --dyn-syms --version-info`，42 个 map 逐个 `nm -D --defined-only` 取集合 | 逐产物集合相等（**不要求地址/顺序**）。符号数不等即失败 |
| P4 | 未定义符号（依赖面） | `nm -D --undefined-only` | 逐产物集合相等。特别检查 `unix_chkpwd` 与 `pam_unix.so` 里 `crypt_r`/`crypt_rn` 的实际引用（补丁 0002 生效证据） |
| P5 | 无绝对工作区路径 | `strings -a <产物> \| grep -F "$PWD"`，且 `readelf -d` 无 `RPATH/RUNPATH` | 命中数 0（对应 A.6.3） |
| P6 | `config.h` 宏集 | 新 `config.h` vs 基线 `linux-pam-*/libpam/include/config.h`，做 `#define/#undef` 名字-值归一化 diff | 34 条逐条相等（对应 A.6.1） |
| P7 | 模块搜索路径与 helper 路径 | `strings -a libpam.so.0.85.1 \| grep '/lib/security'`；各模块里 `grep '/sbin/unix_chkpwd'` | 与基线同串（`DEFAULT_MODULE_PATH`/`_PAM_ISA`/`CHKPWD_HELPER`） |
| P8 | 权限/归属元数据 | 读 rootfs/APK 的 mode/uid/gid 清单 | `unix_chkpwd` = `04755 0:0`；`pam_namespace_helper`/`namespace.init` = `0755`；`limits.d`/`namespace.d` 存在且为空目录 |
| P9 | 数据文件逐字节 | `/etc/security/*.conf`、`/usr/include/security/*.h`、3 个 `.pc`、`pam_namespace_helper` | `sha256` 相等（这些本应不受编译器影响；`pam_namespace_helper` 467B、基线 `b0ff59c7...`） |
| P10 | 体积带 | 每个 ELF `size -A` 的 `.text/.rodata/.data/.bss` | 与新构建的**自身可复现性**一致；对基线允许 ±5% 带（因为 -g/-O 决策可能变化），超出必须逐项解释 |
| P11 | 功能级（真正的验收） | 在来宾里跑既存 PAM 契约测试（`tools/test_pam_login_switch_qemu.py`、`test_sudo_policy`、`pam-helper` 系列）的**非 Python 等价脚本** | 登录/改密/sudo/faillock/umask/limits 行为与基线一致。注意 `linux-pam-build.json` 里 `"guest_verified": false` —— **当前基线本身就未做来宾验证**，因此 P11 必须同时补上新旧两侧，不能拿"旧系统也这样"当通过 |
| P12 | 生产链无 Python/Ninja | `strace -f -e trace=execve` 包住整个 `make` 阶段 | `execve` 目标里 0 次命中 `python*`、`ninja`、`meson`、`/usr/bin/meson --internal`；命中 `bison`/`flex` 只允许出现在显式 `pam-tests` 目标里 |

**P1–P9 是硬判据；P10 是带说明的软判据；P11 是最终功能验收；P12 是本任务的存在性理由。**

---

## A.8 第三条路评估（实证，不臆测）

| 路线 | 实证结论 |
| --- | --- |
| **上游 release tarball 自带 `configure`/`Makefile.in`？** | **否。** [E] `tar tJf buildsystem/deps/auth/Linux-PAM-1.7.2.tar.xz` 的顶层条目只有 `AUTHORS aux ci conf COPYING Copyright doc examples libpam libpamc libpam_internal libpam_misc meson.build meson_options.txt modules NEWS pgp.keys.asc po README tests xtests`。`find . -name 'configure' -o -name 'configure.ac' -o -name 'Makefile.in' -o -name 'Makefile.am' -o -name 'autogen.sh' -o -name '*.m4'` → **0 命中**；`find . -name 'Makefile*'` → **0 命中**。解包树与 tarball 清单一致（同一组 21 个顶层条目）→ 不是被本地删过 |
| **autotools 分支** | 已随 1.7.0 迁移删除（[S] 未联网核实，仅从 tarball 无 autotools 文件 + `NEWS` 存在推断）。仓库内无该分支的任何制品，且拉取旧分支需要联网，违反本次约束 → 不作为选项 |
| **用 `autogen.sh`/`autoreconf` 现造** | 不可行：没有 `configure.ac`/`Makefile.am` 就没有输入 |
| **保留 meson 但只为 PAM 开例外** | 用户已否决 |
| **改用上游 git 里的 `meson-to-make` 等价物** | 未发现（无此类上游产物） |
| **结论** | **手写 Makefile 是当前唯一同时满足"不降级、不删功能、不用 meson/ninja/python"的路径。** 而且规模比直觉小一个数量级（3 个 Makefile / 49 产物 / 110 对象 / 3 类生成步骤） |

---

# 第二部分 任务 B：全生产链 Python / Ninja / Meson / CMake / Tcl 调用点审计

## B.0 口径（三种"Python 调用点"，分开计数）

| 类型 | 定义 | 计数 |
| --- | --- | --- |
| **T1 子进程 Python** | 生产 target 的 `command[0] == PYTHON`（`PYTHON = sys.executable`，[E] `build.py:93`） | **49** 个 target 调用点（+ 1 个"以 PYTHON 启动生成的 SDK 包装器" = 已含在内，见 B.3 第 15 行） |
| **T2 进程内 Python 逻辑** | target 用 `action=`/`action=Lambda`，逻辑在 build.py 进程内执行，Make 路线下必须改写为 C/Shell | **23** 个 target + 图构造本身（`add_compile` 展开的数百条编译边也是 Python 生成的） |
| **T3 二阶解释器** | 被 T1 启动的脚本再去启动 Python / meson / ninja / CMake / tclsh | **1 meson 组件（3 子命令 + 42 SHSYM + 1 regenerate）**、**2 处 python3 嵌套**、**1 处 tclsh 上游规则** |
| 排除 | `kind="test"` 或 `test-*` target 里的 Python | **37** 个（见 B.8） |

**方法**：AST 解析 `build.py`，枚举全部 **157** 个 `Target(...)` 字面量并按 `command[0]` 分类（脚本在 `/tmp`，只读）。所以生产链 = 157 − 51（test） = **106 个 target**，其中 **49 个直接跑 Python 子进程，23 个跑 Python 逻辑，22 个跑外部非 Python 程序，12 个是聚合/无命令**。

> 换句话说：**"生产链 Python 调用点总数 = 72（49 子进程 + 23 进程内逻辑）"**，最集中的几处见 B.7。

---

## B.1 引擎自身（永远算 Python，直到入口切换）

| 位置 | 程序 | 说明 |
| --- | --- | --- |
| `build.py:1` | `python3 build.py` | 唯一入口，全部 106 个生产 target 都在 Python 进程内构图 |
| `build.py:4646` | `[PYTHON, "build.py", "--worker", "--task-id", ...]` | 后台任务/TUI 每条任务 **再启一个 Python 解释器并重跑整张图** |
| `buildsystem/core/runner.py:1054,1067` | `subprocess.Popen` | 实际执行器 |

---

## B.2 T1：生产链 49 个 Python 子进程调用点（逐条）

分类列 = 归属阶段；替代难度 = **低**（外部成熟工具或纯 argv 拼装的短 Shell）/ **中**（有算法或格式，需 C 工具）/ **高**（复杂格式/加密/需从零验证等价性）。

| # | 文件:行 | 被启动的程序 | 阶段 | 可用什么替代 | 难度 |
| --- | --- | --- | --- | --- | --- |
| 1 | `build.py:767` `musl` | `tools/build_musl.py` | 工具 bootstrap | 上游 `configure`+`make`（脚本里已是这两条，[E] `tools/build_musl.py:73-76`）+ 短 Shell 做补丁/mimalloc.o/libssp_nonshared.a | **中**（脚本还做 tar 解包+摘要+`.leonos-musl.json` 记账） |
| 2 | `build.py:793` `auth-upstream` | `tools/build_auth_upstream.py` | 第三方 | 见 B.4 专列 | **高** |
| 3 | `build.py:813` `storage-upstream` | `tools/build_storage_upstream.py` | 第三方 | `configure`+`make`+`make install DESTDIR` [E] `:69-72`，纯 Shell 可写；但要保留 `BUILD_CC`、`-all-static`、`BLKID_LIBS` 覆盖 | **中** |
| 4 | `build.py:836` `<package>`(ncurses/vim) | `tools/build_terminal_packages.py` | 第三方 | `configure`+`make` [E] `:77-78`；需要宿主 `tic`/`infocmp` 存在性检查（`:51`）与 `fallback.c` 生成检查 | **中** |
| 5 | `build.py:1065` `build-info` | `tools/build_info.py` | 配置/版本 | C 工具（`tools/host/version`，§5 已规划）。注意它写 `include/generated/build_info.h`（**受版本控制的源码树内文件**），§7 要求移到 `O/generated` | 低 |
| 6 | `build.py:1200` `boot-logo` | `tools/generate_boot_logo.py` | 资源 | C 工具。只用 `struct`+`zlib` 解 PNG（无 PIL，[E]），算法可直译 | 中 |
| 7 | `build.py:1344` `loader-integrity` | `tools/gen_loader_integrity.py` | 资源/安全 | C 工具。`hashlib` 摘要 + 头文件模板 | 低 |
| 8 | `build.py:1401` `gbk-table` | `tools/generate_gbk_table.py` | 资源 | **C 工具 + 脆弱点**。用正则从 `third_party/litehtml/src/encodings.cpp` 里抠 `gb18030_decoder::m_index[]`，截断到 126×190（[E] `:13-28`）。上游一改格式就静默错表 | 中 |
| 9 | `build.py:1473` `musl-extension-archive` | `tools/rebuild_archive.py` | 工具链 | 直接 `llvm-ar` + 成员清单（§6.1 要求"重建到新临时文件"）。该脚本存在的理由正是"避免旧 archive 残留成员"，Make 原生能做到 | 低 |
| 10 | `build.py:1496` `archive:installer-libc` | `tools/rebuild_archive.py` | 同上 | 同上 | 低 |
| 11 | `build.py:1512` `musl-sdk` | `tools/package_musl_sdk.py` | SDK | Shell/`tar`（[E] 只用 `shutil`+`tarfile`）。**但它 `copy2(tools/leonos_musl_cc.py → bin/leonos-musl-cc)`（`:49-51`）→ 见 B.5** | 低（前提是先换包装器） |
| 12 | `build.py:1529` `musl-probe:{dynamic,static}` | `PYTHON + <sdk>/bin/leonos-musl-cc` | SDK 验证 | **去掉 PYTHON 前缀后就是直接调用包装器**；包装器本身换成 C 后即清零。这是"生成出来的包装器不得是 Python"的实证案例 | 低 |
| 13 | `build.py:1557` `musl-ltp` | `tools/build_musl_ltp.py` | 测试基础设施（被生产图引用的部分） | `make autotools` + `make`（[E] `:57-67`）→ Shell | 中 |
| 14 | `build.py:1571` `file-magic` | `tools/build_file_magic.py` | 第三方 | 宿主 `autoreconf -fi && ./configure && make`（[E] `:37-53`）→ 短 Shell；§9 明确允许 autotools | 低 |
| 15 | `build.py:1589` `file` | `tools/build_file.py` | 第三方端口 | 逐文件 clang（脚本自己编 obj），→ `mk/third-party/file.mk` | 中 |
| 16 | `build.py:1644` `sqlite` | `tools/build_sqlite.py` | 第三方 | **注意**：融合体生成走 `make -f Makefile.linux-gcc sqlite3.c sqlite3.h`（[E] `:84`），**Make 里是 tclsh**（B.4）。外层 Python 只做 copytree+TOP 重写+后续 clang → Shell/Make 可替；tclsh 无法替 | 中（外层）/ 阻塞（tclsh） |
| 17 | `build.py:1685` `portablegl` | `tools/build_portablegl.py` | 第三方 | 逐文件 clang++ → Make | 中 |
| 18 | `build.py:1765` `busybox` | `tools/build_busybox.py` | 第三方 | busybox 是 **kbuild + Makefile**（[E] `third_party/busybox/Makefile*`；`build_busybox.py:834,843,873` 直接 `make O= allnoconfig/oldconfig/all`）→ 递归 `$(MAKE)`，脚本其余是配置生成/链接/postprocess | 中 |
| 19 | `build.py:1806` `nano` | `tools/build_nano.py` | 第三方 | 逐文件 clang（nano 上游是 autotools，我们不用它）→ Make | 中 |
| 20 | `build.py:1841` `fastfetch` | `tools/package_fastfetch.py` | 第三方（预编译产物打包） | Shell + `install`；有 `LEONOS_FASTFETCH_BINARY` 外部输入旁路 [E] `build.py:902-903` | 低 |
| 21 | `build.py:1865` `sl` | `tools/build_sl.py` | 第三方 | 上游就是一份 `Makefile`（[E] `third_party/sl/Makefile`），但我们手工编 → Make 化容易 | 低 |
| 22 | `build.py:1906` `less` | `tools/build_less.py` | 第三方 | **二阶 Python**：`run(["python3", source/"mkhelp.py"], ...)`（[E] `:76`）。见 B.4 | 中 |
| 23 | `build.py:1944` `lua` | `tools/build_lua.py` | 第三方 | lua 上游 `make`（POSIX Makefile），我们是逐文件 clang → Make | 中 |
| 24 | `build.py:1989` `cmd` | `tools/build_cmd.py` | 第三方 | 上游有 `Makefile` [E] → Make | 低 |
| 25 | `build.py:2028` `app:pleditor` | `tools/build_pleditor.py` | 第三方（Qt-free 端口） | 逐文件 clang++ → Make | 中 |
| 26 | `build.py:2304` `app-manifests` | `tools/generate_app_manifests.py` | 资源 | C 工具（`.app.ini` 解析 + 复用 `buildsystem/components.py` 的 TOML 加载 → §5 要求用已验证 C 库） | 中 |
| 27 | `build.py:2329` `ui-font` | `tools/prepare_ui_font.py` | 资源 | C 工具。只用 `struct`（TTF 表级裁剪，[E]）+ `shutil.copy2` | 中 |
| 28 | `build.py:2334` `browser-font` | `tools/prepare_browser_font.py` | 资源 | Shell `cp`（只用 `argparse`+`shutil`，[E]） | 低 |
| 29 | `build.py:2339` `app-icons` | `tools/make_app_icons.py` | 资源 | C 工具（手写 PNG/BMP，只用 `struct`，**无 PIL**） | 中 |
| 30 | `build.py:2343` `minesweeper-assets` | `tools/make_minesweeper_assets.py` | 资源 | 同上 | 中 |
| 31 | `build.py:2348` `window-button-icons` | `tools/make_window_button_icons.py` | 资源 | 同上 | 中 |
| 32 | `build.py:2516` `grub-bdf` | `tools/make_grub_font.py` | 资源 | C 工具：`system.psf` → BDF（纯文本+PSF 头解析）；下游已是外部 `grub-mkfont` [E] `:2524` | 中 |
| 33 | `build.py:3143` `rpr-apps:oschinpt-index` | `tools/make_oschinpt_index.py` | 资源/索引 | C 工具（`struct` + 目录遍历） | 中 |
| 34 | `build.py:3258` `esp:apk-ownership` | `tools/apk_ownership.py` | APK | C 工具（JSON + 路径归一化 + `posixpath`，[E] imports）；**这是所有权契约，§10 明确要求保留** | **高** |
| 35 | `build.py:3282` `apk-root` | `tools/apk_distribution.py` | APK | **最难单项**：APK 包格式写出 + 索引 + RSA 签名（`openssl genpkey/pkey/dgst`，[E] `:155,177,338,472`）+ `apk mkndx` + `unshare` 能力探测 + `fcntl` 锁 + **`busybox --list` 在宿主上执行**（[E] `:402`） | **高** |
| 36 | `build.py:3302` `rpr-apps` | `tools/build_rpr_apps.py` | runtime 载荷 | C/Shell（打包+清单） | 中 |
| 37 | `build.py:3336` `rpr-pages` | `tools/build_rpr_pages.py` | runtime 载荷 | C/Shell | 中 |
| 38 | `build.py:3366` `image-vmdk` | `tools/make_image.py` | 镜像 | **GPT/FAT/ext2 布局全在 Python**（[E] imports：`struct`+`fcntl`+`subprocess`+`tempfile`；外部只用 `mkfs.fat`/`mcopy`/`qemu-img`/`truncate`/`dd`）→ §3 禁止重写 ELF/APK/文件系统格式工具，但这里是"自己写的格式化器"，需要极谨慎等价 | **高** |
| 39 | `build.py:3378` `desktop-live-root` | `tools/make_live_root.py` | 镜像 | 复用 `make_image.py` 的 `make_ext2_root` + `leonos_layout.py` | **高** |
| 40 | `build.py:3385` `image-iso` | `tools/make_installer_iso.py` | 镜像 | `grub-mkrescue`/`xorriso` 是外部工具 [E] `build.py:4164` → 外层是 Shell + 目录树拼装 | 中 |
| 41 | `build.py:3398` `installer-root` | `tools/make_installer_root.py` | 镜像 | `hashlib`+`shutil`+`subprocess`+`make_image` 复用 | **高** |
| 42 | `build.py:3443` `installer-image` | `tools/make_installer_iso.py` | 镜像 | 同 40 | 中 |
| 43 | `build.py:3501` `<root_target>` | `tools/make_musl_checkpoint.py` | 镜像（检查点） | 用 `debugfs`（[E] 该文件是 `tools/` 里 debugfs 的主要使用者）+ 布局 → Shell/C | 中 |
| 44 | `build.py:3507` `<suffix iso>` | `tools/make_installer_iso.py` | 镜像 | 同 40 | 中 |
| 45 | `build.py:3523` `musl-desktop-vim-root` | `tools/make_live_root.py` | 镜像 | 同 39 | 高 |
| 46 | `build.py:3530` `musl-desktop-vim` | `tools/make_installer_iso.py` | 镜像 | 同 40 | 中 |
| 47 | `build.py:3626` `kconfig-mconf` | `tools/build_kconfig_frontends.py` | host 工具 bootstrap | 上游 `configure`+`make`+`make install`（[E] `:91-92`）→ 短 Shell | **低** |
| 48 | `build.py:3798` `<dyn>` (kind=command) | `PYTHON + "tools/" + script` | 第三方测试链 | 动态拼接，属 test 邻域；按 T1 记入以免漏数 | 低 |
| 49 | `build.py:2390` → `:2391` `sdk` | `PYTHON tools/package_devtools.py` | SDK | `zipfile`+`re`+`subprocess`+`shutil`（[E]）→ C 工具或 `zip`/Shell。这个 target 的 AST 显示为 `tuple(...)`，容易被漏计 | 中 |

> `build.py:950/956/3645/3656/4330/4333` 是 `config-sync`（`tools/generate_component_kconfig.py` + `tools/kconfig_sync.py`）的 **3 处不同调用点**（target action、`run config-sync`、`defconfig`），按 T2 计，见 B.6。

---

## B.3 T3：Meson / Ninja / CMake 调用点 —— **只有 1 个组件**

| # | 文件:行 | 程序 | 阶段 | 替代 | 难度 |
| --- | --- | --- | --- | --- | --- |
| M1 | `tools/build_auth_upstream.py:157` | `meson setup ... --cross-file musl.ini -Dsecuredir=/lib/security -Dpam_unix=enabled -Dvendordir= -Ddocs=disabled` | 第三方（linux-pam） | 第一部分整篇方案 | 中（PAM 内部） |
| M2 | `tools/build_auth_upstream.py:163` | `meson compile -C <dir> -j<N>` | 同上 | 同上 | 同上 |
| M3 | `tools/build_auth_upstream.py:164` | `meson install -C <dir> --destdir <root>` | 同上 | 同上 | 同上 |
| M4 | `linux-pam-*/build.ninja:31-34` + 42 条 `SHSYM` 边 | `/usr/bin/meson --internal symbolextractor` | 由 M2 触发 | **移植后自动消失**（Make 直接以 `.so` 为依赖） | — |
| M5 | `linux-pam-*/build.ninja:39-42` `REGENERATE_BUILD` | `/usr/bin/meson --internal regenerate` | 由 ninja 在 mtime 变化时触发 | **自动消失** | — |
| M6 | `build.ninja:152` 附近 `Linux-PAM-pot` `run` target | `/usr/bin/meson --internal gettext pot` / `update_po` | 只有 `meson compile <pot>` 才跑，生产链不触发 | 若保留 `pam-tests` 需注意 | — |
| M7 | `tools/build_auth_upstream.py:160-162` | `--reconfigure --clearcache`（存在 `build.ninja` 时） | 每次强制重探 | Make 侧用签名文件替代（§6.2） | — |

**Ninja**：只作为 M2 的内部执行器存在，仓库内无任何显式 `ninja` 调用（[E] `grep -n '"ninja"' build.py tools/*.py` 零命中，唯一 `meson` 字符串命中是 `build.py:4681` 的 `--theme` 选项名，与构建无关）。

**CMake**：**零生产调用点** [E] `grep -rn '"cmake"\|CMakeLists' build.py tools/*.py`（非 test）零命中。`third_party/{litehtml,mbedtls,mimalloc,libpng,zlib}` 里有 `CMakeLists.txt`，但：
- `mbedtls`：只用 `-Ithird_party/mbedtls/include` + 显式源文件列表（[E] `build.py:1145,1414`）；
- `zlib`/`libpng`：`ar rcs` 自己归档（[E] `build.py:1425,1434`），libpng 的 `pnglibconf.h` 由 build.py 内联文本替换生成（[E] `build.py:978-996`，见 B.6）；
- `mimalloc`：只编 `src/static.c` 一个单元（[E] `tools/build_musl.py:84-92`）；
- `litehtml`：**只被当作数据源**读 `src/encodings.cpp` 抠 GBK 表（[E] `build.py:1405,1408`），完全不编译。

---

## B.4 隐藏的二阶解释器调用（不在 build.py 视图里，最容易漏）

| # | 文件:行 | 被启动 | 阶段 | 说明 / 替代 | 难度 |
| --- | --- | --- | --- | --- | --- |
| H1 | `tools/build_auth_upstream.py:157-164` | `meson`（→ Python + ninja） | 第三方 | 见第一部分 | 高 |
| H2 | `tools/build_less.py:76` | `python3 <less>/mkhelp.py`（stdin/stdout 重定向） | 第三方（less 帮助文本 `less.hlp`） | **less 的 git checkout 不带预生成产物**。可选：(a) 用 C 工具复刻 `mkhelp.py`（它只是把 `LESS.HLP` 模板与 `help.txt` 做文本拼装）；(b) 让上游 `Makefile.in` 路线产出（less 有 `Makefile.in` [E]，其发行 tarball 会预生成 `less.hlp`）。**倾向 (a)**，因为子模块是 git 树 | 中 |
| H3 | `tools/make_ext2_root.py:20` | `fakeroot -- sys.executable <self> ...`（**自我重执**） | rootfs/镜像 | 生产镜像阶段以 rootless 方式重启自己一次。移植：C 工具 + 单次 `fakeroot`，或按 §10 用 `debugfs`/`mke2fs` 的 rootless 元数据保留法。**这是"镜像链跑 Python"的最硬证据** | **高** |
| H4 | `third_party/sqlite/main.mk:662-663,731-732`（由 `tools/build_sqlite.py:84` 触发） | **`tclsh`** `tool/mksqlite3c.tcl`、`tool/mksqlite3h.tcl` | 第三方 | Tcl 不是 Python，但 **`build.py:4166` 的 `task_tools("sqlite")` 返回 `userland`，完全没检查 tclsh** → 这是一条未声明的宿主依赖。§9 只禁 Meson/Ninja/Python，故技术合法，但必须写进依赖表 + doctor | 低（记账）/高（若要去掉） |
| H5 | `tools/build_sqlite.py:79-82` + `main.mk` `src-verify`/`mkkeywordhash`/`fts5parse.c` | 宿主 `$(BCC)` 编译出的 `src-verify`、`mkkeywordhash`、`lemon` 并执行 | 第三方 | "以宿主方式跑一次"的**合法**宿主工具生成（非目标端 ELF），Make 原生支持 | 低 |
| H6 | `tools/build_terminal_packages.py:47-52` | 宿主 `tic`/`infocmp` | 第三方（ncurses terminfo） | 已是外部 C 程序 | 低 |
| H7 | `tools/build_kconfig_frontends.py:76-92` | 宿主 `./bootstrap`（→ autoconf/automake/libtool，内部 Perl）+ `./configure` + `make` | host 工具 bootstrap | §9 允许 autotools；但 autoconf/automake 内部是 **Perl**，与 H4 同性质，需入依赖表 | 低 |
| H8 | `tools/build_musl_ltp.py:58` | `make autotools` → autoconf/automake/libtool | 测试基础设施 | 同上 | 低 |
| H9 | `tools/apk_distribution.py:402` | `<tree>/bin/busybox --list` | APK | **以宿主方式运行目标端 busybox 一次**！[E] `file build/auth-upstream/root/bin/...` 的 busybox 是 x86_64 ELF 且宿主同为 x86_64 Linux，所以能跑；在别的宿主上会断 | 中 |
| H10 | `tools/prepare_gcc_probe.py:60` | `python3 tools/make_image.py` | **非生产**（`userland/musl-gcc/README.md` 手工流程） | 归到 dev-only，不入生产链计数 | — |

---

## B.5 生成出来的包装器脚本的**解释器**（§2 显式点名项）

| 包装器 | 源 | 安装为 | shebang | 结论 |
| --- | --- | --- | --- | --- |
| musl SDK 编译器驱动 | `tools/leonos_musl_cc.py`（100% Python，[E] `:1 #!/usr/bin/env python3`） | `<sdk>/bin/leonos-musl-cc`，`chmod 0o755`（[E] `tools/package_musl_sdk.py:49-51`） | **`#!/usr/bin/env python3`** | **违反 §2"包括生成出来的 SDK 编译器包装器"**。且它内部还 `subprocess.check_output([compiler,'-print-resource-dir'])`（[E] `:14`）—— 每次编译都要拉起一个 Python |
| 同一包装器在构建图里的用法 | `build.py:1546` | `command=(PYTHON, "<out>/musl/sdk/bin/leonos-musl-cc", ...)` | 双重 Python | 修好包装器后这一条也自动干净 |
| `tools/package_musl_gcc.py` | 手工工具（仅 `userland/musl-gcc/README.md:25` 文档化） | 外部 dyne GCC 打包 | `#!/usr/bin/env python3` | **不在 build.py 图里**，归类为 dev-only；若将来纳入 SDK 需一并换 C/Shell |
| 上游 `.in` Shell 模板 | `modules/pam_namespace/pam_namespace_helper.in`（`#!/bin/sh`，[E]） | `/sbin/pam_namespace_helper`（467 B，[E]） | `/bin/sh` | **合法**：§9 允许 configure 生成 Shell。移植后继续用同一模板 |
| PAM 的 `aux/redir_exe.sh`、`aux/chdir_meson_build_subdir.sh` | `meson.build:640-641` | 只在 docs/tests 路径 | `/bin/sh` | docs 已 disabled，移植不引入 |

→ **B.5 的唯一硬违规项就是 `leonos-musl-cc`**。它约 40 行、纯 argv 拼装 + 一次 `-print-resource-dir`，**改写成 C 或 POSIX Shell 的难度：低**。这是"低成本、高价值"的第一优先项。

---

## B.6 资源生成器与配置生成器的现状（全部无第三方 Python 库依赖）

[E] 逐个 `grep "^import"` 结果：`generate_boot_logo.py`(`struct`,`zlib`)、`gen_loader_integrity.py`(`hashlib`)、`generate_gbk_table.py`(`re`)、`prepare_ui_font.py`(`struct`,`shutil`)、`make_app_icons.py`/`make_window_button_icons.py`/`make_minesweeper_assets.py`/`make_oschinpt_index.py`(`struct`)、`make_grub_font.py`(仅 `argparse/pathlib`)、`build_info.py`(`datetime`)、`apk_ownership.py`/`apk_distribution.py`(`json`,`hashlib`,`fcntl`,`tarfile`,`subprocess`)。

**好消息**：没有一个用 PIL/第三方库（CI 装的 `python3-pil` 只服务 `tools/test_*_qemu.py` 的截图断言，[E] `grep -l "from PIL"` 的 6 个文件全是 test）。因此**全部资源生成器都是"算法直译 C"**，不存在必须引入图像库的风险。

进程内 Python 逻辑（T2，23 项，需等价重写）中最要紧的：

| 位置 | 函数 | 职责 |
| --- | --- | --- |
| `build.py:964` | `sync_config` → 启 `generate_component_kconfig.py` + `kconfig_sync.py` | **配置权威转换**：`components.toml` → `Kconfig.components` → `autoconf.h` + `rustcfg.args` + `component-selection.json`（§7 的 C Kconfig 前端替换点） |
| `build.py:997` | `generate_libpng_config` | 文本标记替换（`/* end of options */` 前插 4 行 `#undef`）→ 一次 `sed` 即可 |
| `build.py:1085,1105,2880,2898` | `text_action(...)` | 纯文本头/清单生成 → heredoc/Shell |
| `build.py:1739` | `generate_glxgears_source` | 上游源裁剪 |
| `build.py:1756` | `busybox_source_revision_action` | `git` 取 revision |
| `build.py:2651` | `prune_component_staging` | 关闭组件时删产物（§10"不残留删掉的文件"） |
| `build.py:2700` | **`stage_rootfs`** | rootfs 清单装配（§10 主战场） |
| `build.py:2731,2790,2808,3020,3249` | `stage_rpr_client` / `stage_auth_payload` / `stage_storage_payload` / `stage_terminal_packages` / `stage_layout_links` | 载荷暂存、guest 路径与 symlink（`leonos_layout.py`） |
| `build.py:2944` | `sync_ui_font` | 字体同步 |
| `build.py:3611` | `make_release` | release 组装 |
| `build.py:3661,3678` | `menuconfig`, `clean` | 交互/清理（§6.3 的 O 目录所有权检查） |
| `build.py:568` | `add_copy` → `copy_action` | **保留 symlink 与 mode 的复制原语**，被大量 `esp:*` target 复用 |

---

## B.7 最集中的几处（按"去掉后清零收益"排序）

1. **`apk-root` + `esp:apk-ownership`（`build.py:3282` / `:3258`）** —— 2 个调用点但承载整个包管理系统（签名/归属/索引/`busybox --list` 宿主执行/`unshare` 探测/`fcntl` 锁）。**难度最高**，且 §3 明令"不重写 APK 签名算法"，等价验证代价最大。
2. **三类镜像链：`image-vmdk`/`image-iso`/`installer-*`/`*-live-root`/`musl-desktop-vim`/`musl-checkpoint`（`build.py:3366,3378,3385,3398,3443,3501,3507,3523,3530` = 9 个调用点）** —— 全部经由 `make_image.py` / `make_ext2_root.py`（含 H3 的 `fakeroot` 自我重执）。GPT/FAT/ext2 布局逻辑在 Python 里，**风险最高、回归面最大**（§12 P3 的三条镜像验收全在这）。
3. **`auth-upstream`（`build.py:793` → `build_auth_upstream.py:157-164`）** —— **唯一的 meson/ninja 源**，且被 `storage-upstream`、`esp:auth`、SDK、rootfs 共同依赖（[E] `build.py:816-820` storage 需要 auth 的 `libuuid.a/libblkid.a`；`build.py:2390-2396` SDK 需要 `--pam-root`；`build.py:2790` `esp:auth`）。修好 PAM 一处即让**整条第三方链零 meson/ninja**。
4. **第三方端口构建器族：`build.py:1589,1644,1685,1765,1806,1865,1906,1944,1989,2028` = 10 个调用点** —— 同一个模式（Python 里拼 argv + 逐文件 clang + `musl_link`）。**数量最多但最规整**，一次性抽象成 `mk/third-party/*.mk` 模板即可批量清零。其中 `busybox` 与 `ncurses`/`vim` 本来就是 Make 体系，可直接递归 `$(MAKE)`。
5. **资源生成器族：`build.py:1065,1200,1344,1401,2304,2329,2339,2343,2348,2516,3143` = 11 个调用点** —— 全部 stdlib-only 算法，机械翻译为 `tools/host/{assets,version,manifest}` C 工具（§5 已经按这个边界规划好了）。
6. **SDK 包装器 `leonos-musl-cc`（B.5）** —— **单个文件、约 40 行、低难度、却是 §2 的显式违规项**。建议最先做。
7. **配置链 `sync_config`（`build.py:964/3645/3656/4330/4333` = 5 个 Python 调用点）** —— 4 处不同入口调用同两个脚本，替换点集中，是 §7 的 C Kconfig 前端落地位。

---

## B.8 `test-legacy` 排除清单（既有 Python 回归测试）

[E] `kind="test"` 或以 `test-` 命名的 target 共 **51 个**，其中 **37 个** 的 command[0] 是 `PYTHON`（`build.py:784,846,1857,3521,3681-3786,3801-3832`）。它们启动的脚本：

`tools/test_musl_abi.py`、`test_terminal_packages.py`、`test_fastfetch_package.py`、`test_live_iso.py`、`test_oobe.py`、`test_sudo_policy.py`、`test_builtin_tool_removal.py`、`test_installer_setup.py`、`test_installer_input.py`、`test_svga.py`、`test_license_server.py`、`check_unix_paths.py`、`test_linux_abi_contract.py`、`test_linux_memory.py`、`test_linux_process_vm.py`、`test_linux_sysv_msg.py`、`test_linux_sysv_sem.py`、`test_linux_pty.py`、`test_power.py`、`test_init_power.py`、`test_linux_permissions.py`、`test_storage_metadata.py`、`test_storage_rename.py`、`test_storage_mkdir_mount.py`、`test_ext2_cache.py`、`test_ext2_performance.py`、`test_ext2_write_batch.py`、`test_storage_write_batch.py`、`test_installer_copy.py`、`test_musl_distribution.py`、`test_uapi.py`、`check_abi_migration.py`、`test_component_config.py`、`test_kconfig_frontends.py`、`test_openrc_shutdown.py`。

`build.py:3736` 的 `test-los2w` 用 `PYTHON -c ...`（内联代码）[E]。
`build.py:4043-4107` 的 12 个 `test-qmp-*` 是 `action=Lambda` / `ACTION:qmp_test_suite`（T2，进程内 Python + QEMU QMP）。

**仓库内 `tools/test_*.py` + `tools/check_*.py` 共 132 个文件**，绝大多数**没有**注册进 build.py 图（只在 CI 或手工调用）。它们的依赖：`PIL`（6 个 QEMU 截图类 [E]）、`debugfs`/`qemu-img`/`openssl`/`unshare`（外部工具）、`socket`/`threading`（标准库）。

**归类建议**：
- `kind="test"` 的 37 个 → `test-legacy` target，按 §2"列出依赖、不参与核心构建验收"。
- 其余 95 个未注册的 `tools/test_*.py` → 先出清单，逐个标"已注册/未注册/死代码"，不在本次删除（§3"不把全部 Python 文件视为旧构建残留"）。
- `tools/prepare_gcc_probe.py`、`tools/package_musl_gcc.py`、`tools/package_python.py`（往客户机里装 Python 应用，§2 明确不要求删）→ 非构建调度脚本，单独归类。

**注意（红线）**：`test-musl-abi`（`build.py:784`）与 `test-terminal-packages`（`:846`）在 `depends_on` 图上**不在**镜像/SDK 的生产闭包里（它们是 `kind="test"` 叶子）→ 可以安全归入 `test-legacy`。但 `musl-probe:*`（`:1529`）是 `kind="link"`、是 `musl-probes` 聚合的成员，`build.py:1550`；若 `release` 依赖 `musl-probes`，它就**不是** test-legacy。[S] 需人工确认 `all`/`release` 的 `depends_on` 是否包含 `musl-probes`。

---

## B.9 Rust 组件与 doctor 缺口

| 事实 | 证据 |
| --- | --- |
| `middlelayer/osmlayer` 是 **Rust + C**：8 个 `.rs`，无 `Cargo.toml`/`build.rs`，**直接 `rustc --crate-type lib --target x86_64-unknown-none`** 产出 `osmlayer.o` | [E] `build.py:1305`（target）+ `1311-1315`（rustc argv）；`ls middlelayer/osmlayer/` → `linker.ld runtime.c src/`；`find middlelayer -name Cargo.toml` 空 |
| `rustc` 与 `ld.lld` 分别编 Rust 与 C，再 `ld -nostdlib -T linker.ld` 合成 `middlelayer.sys` | [E] `build.py:1322-1336` |
| `rustcfg.args` 由 `config-sync` 生成并作为 implicit input 喂给 Rust 编译 | [E] `build.py:762`、`1309` |
| doctor 已检查 `rustc`：`esp`、`middlelayer`、`run*` 路径都含 | [E] `build.py:4159-4170` |
| **doctor 未检查 `tclsh`**（SQLite 融合体生成必需） | [E] `build.py:4166` `task_tools("sqlite")` → `userland` = `clang,ld.lld,llvm-ar,autoreconf,autoconf,automake,libtoolize,make,gcc,gawk` |
| **doctor 未检查 `msgfmt`/`gettext`**（PAM `.mo` 必需，i18n=auto 时） | [E] 4159-4200 全表无 `msgfmt`/`gettext`/`bison`/`flex`（除 `kconfig-mconf` 分支有 `flex`,`bison`,`gperf`,`g++`） |
| **doctor 未检查 `patch`、`tar`/`xz`、`curl`、`pkg-config`、`clang` 版本/resource-dir** | [E] 同上；`patch` 用于 `build_auth_upstream.py:135`、`build_musl.py:56`；`pkg-config` 用于 PAM 的 `libcrypt.pc` |

→ 计划 §2"Rust 组件的既有编译器依赖需在 doctor 和依赖表中列出"：`rustc` 已登记，但**新增依赖表必须补 `tclsh`、`msgfmt`、`pkg-config`、`patch`、`xz`**，并把 `x86_64-unknown-none` 是否需安装 `rust-std` 目标写清 [S]（当前用 `-target x86_64-unknown-none`，需 std-less 目标支持，未验证本机如何满足）。

---

## B.10 CI 中非生产构建的部分

| 位置 | 内容 | 分类 |
| --- | --- | --- |
| `.github/workflows/build-installer.yml:79-80` | 安装 `python3`、`python3-pil` | 为 `test_*` 准备；生产构建只需要 `python3` 直到入口切换 |
| `:145` | `python3 ./build.py run defconfig` | 配置入口，P4 要改 |
| `:187-191` | `python3 tools/test_component_config.py`、`build.py info ...` | 门禁/信息，非产物生产 |
| `:201-203` | `python3 ./build.py run image-vmdk / image-iso / installer` | **生产链**，P4 必须整体换 `make` |
| `.github/workflows/publish-rpr.yml:35,68-69` | 装 `python3 python3-pil`；`build.py run defconfig` + `run rpr-pages` | 生产链（RPR 载荷） |
| `.github/workflows/code-count.yml:35` | `python3 tools/count_code.py` | **非构建**，度量类，可保留 Python |
| `sourcehut-sync.yml` | 纯 git 镜像 | 无关 |

---

## B.11 汇总

| 项 | 数值 | 备注 |
| --- | --- | --- |
| 生产 target 总数 | 106 | 157 − 51 个 test |
| **T1 直接跑 Python 子进程的生产调用点** | **49** | B.2 全表 |
| **T2 进程内 Python 逻辑的生产 target** | **23** | B.6 表；另有图构造本身与 `add_compile` 展开 |
| 生产链 Python 相关调用点合计 | **72** | 49 + 23 |
| **T3 二阶解释器/生成器调用点** | **13** | H1–H9（+M4/M5/M6）；H10 为 dev-only |
| meson/ninja 使用点 | **1 个组件、3 条子命令** | `build_auth_upstream.py:157/163/164`，全在 linux-pam |
| CMake 生产调用点 | **0** | 第三方 CMakeLists 全部不被触发 |
| Tcl 生产调用点 | **1**（未声明） | `third_party/sqlite/main.mk:663,732` |
| test-legacy 可归类的 Python 调用点 | **37** | B.8 |
| `tools/*.py` 总数 | 132 个 `test_*`/`check_*` + 其余构建工具脚本 | §3 禁止"全当旧残留删除" |

**替代难度分布（T1+T2 的 72 项）**：低 ≈ 13、中 ≈ 37、高 ≈ 22（高项集中在 APK 2 项 + 镜像 9 项 + 若干 stage/rootfs 逻辑 + PAM）。
