# M1 等价替代专项设计审查（阶段 5.7）

日期：2026-09-27。对象：NTCLKS `5cc9621` 内核 + 主仓 `refactor/ntclks-separation`。
本文是交接 §5.7 要求的"专项设计审查结论先行"交付物；实施按 §6 顺序进行，
在本文件入库之前不动任何检查或实现。

## 0. 结论先行

**判定：充分（采用"角色 gid = 文件属主"方案）。**

- 授权编码进镜像 inode 的属主字段（保留 root 所有 + 非可写 + root 执行者三道
  既有闸门，以保留 gid 作为角色标记），路径退出信任判定——正是
  03-permission-matrix §2 表 M1 行"把权威编码进镜像文件本身"的可落地形态。
- "文件能力位"（Linux `security.capability` xattr）**不可行**：存储元数据只有
  `{mode,uid,gid}`（storage.h:24-28），ext2 驱动不含 xattr，`LEONACL.SYS`
  sidecar 仅 FAT；实现它等于新开存储基建，超出尾项范围（§3.A）。
- set-id 位做标记**不充分**：setuid/setgid 位与 exec 凭证语义耦合，误配即提权
  （§3.B）。大小写敏感名字比较**不满足方向**：保留名字信任核心，违反 03 §2
  与 §6.5（§3.D）。
- m1c-case-variant-path-impersonation 由此从 xfail 转绿的机制明确（§4.3），
  **不删除、不放松任何既有检查**；负例夹具只做"单一失败条件隔离"加固。

## 1. 范围与输入

输入（交接 §5.7 指定）：03-permission-matrix §1 M1 行、§2 替代表、§5 负例
清单第 1 条、§6.1/§6.3（按 §6 条目计数）。约束：不放松断言、不删检查、
权限改动负例先行、死代码语义勿复刻（M14）、内核函数补 Doxygen。

## 2. 现状盘点（源码已检查，NTCLKS 5cc9621）

### 2.1 授予点：唯一的安全名字判定

`userland.c:1084-1104`（`userland_exec_current_node` 尾部）是 SERVICE/
WINDOW_SERVER 的唯一标记点：

```c
preserved_flags = task->flags & (TASK_FLAG_ELEVATED_ADMIN | TASK_FLAG_WAITABLE_CHILD);
struct leonos_permissions permissions;
if (!task->uid && !task->euid &&                                   /* root 执行者 */
    storage_inode_permissions(&node, &permissions, false) == 0 &&
    permissions.uid == 0 && !(permissions.mode & 0022)) {          /* root 所有、非组/世界可写 */
    if (path_is_system_desktop(path))      { … SERVICE|WINDOW_SERVER; desktop_pid = … }
    else if (path_is_windowd(path))        { … SERVICE; windowd_pid = … }
    else if (path_is_imd(path))            { … SERVICE; imd_pid = … }
}
task->flags = preserved_flags | TASK_FLAG_STARTED;
```

- 名字辅助函数 `path_eq_ignore_case`（:83）、`path_is_system_desktop`（:134）、
  `path_is_windowd`（:142）、`path_is_imd`（:147）**只服务这一个调用点**
  （:1093/:1096/:1099），无其他消费者。
- `spawn_path_internal_ex`（:647-666）的 `TASK_FLAG_SERVICE` 分支对当前全部
  调用点（:956/:1150/:1187/:1222 均传 `flags=0`）不可达；其注释"拒绝重复实例"
  是陈旧漂移（03 §6.7），内核无此检查。
- 三个 pid 变量（`desktop_pid/windowd_pid/imd_pid`，:39-43）只被
  `userland_enter_first` 的"no Ring-3 userland"或检查（:973 一带）消费，
  **互相可替代**；windowd 与 imd 的授予结果完全同型（皆 SERVICE）。

### 2.2 标志消费方分类（替代必须保平的语义面）

| 消费点 | 语义 | 状态 |
| --- | --- | --- |
| `net.c:3204-3205` 连接列表可见性（B5）：SERVICE 且非 WINDOW_SERVER 见全部 socket | **活** | 替代后必须保平 |
| `sched.c:3304-3335` 会话身份（B6）：`(!SERVICE \|\| WINDOW_SERVER)` 才应用/清除会话身份 | **活** | 替代后必须保平 |
| `sched.c:978-983` fork 剥离（M3 机制） | 机制 | 不动 |
| `sched.c:2534/2992/3044` kill/挂断/登出免疫 | 死代码可达（M14 订正） | **不复刻**，待清死代码 |
| `syscall.c:2748` 登出清理（F3 调用 `auth_kill_session_tasks_for_logout` :2724） | 死代码 | 同上 |
| `syscall.c:359-368` `require_background_service` | 零调用者 | 同上 |

即：角色区分（desktop=SERVICE|WINDOW_SERVER，windowd/imd=SERVICE）是**承重**的
——B5/B6 对 WINDOW_SERVER 取反/放行，二者必须继续分开授予。

### 2.3 镜像元数据与 staging 事实

- `struct leonos_permissions { mode, uid, gid }`（storage.h:24-28）：**没有**
  xattr/能力存储；`LEONACL.SYS` sidecar 仅 FAT（ext2 rootfs 不适用）。
- exec 侧读 `{mode,uid,gid}` 与 setuid/setgid 凭证同源：`userland_prepare_exec_credentials`
  （userland.c:441-470）在执行/存储锁内读持住的 ELF inode（A7 已验证无 TOCTOU，
  注释明言对标 `bprm_fill_uid`）。任何编码在该三元组内的权威同样继承 A7 保证。
- 伪造闸门：`fs_permissions_chown`（permissions.c:459-481）chown/chgrp 均需
  属主或 `CAP_CHOWN`；非 root 不能造 root-owned 文件，也不能改到无成员的保留组。
- staging：rootfs-stage.sh 的 plan 是 6 列（kind/src/dest/mode/组件/策略），有
  **每文件 mode、无 gid**；`leonos-stage` 清单输出硬编码 `"uid":0,"gid":0`
  （tools/host/manifest/leonos-stage.c:310-311）；全部程序文件以 0755 staged
  （rootfs-stage.sh:52-131）。镜像实证（p5-out 构建产物 debugfs）：
  desktop/windowd/imd 三镜像 `Mode: 0755` uid/gid 0；guest `/etc/group` 只有
  gid 0/10/14/1000/65534；安装器 root 的 desktop 同为 0755 uid/gid 0。

## 3. 备选方案评估

### A. 文件能力位（`security.capability` xattr）——不可行

需要 ext2 xattr 或扩展 sidecar 到 ext2 的存储层工作；03 §2 把它列为
"或"选项，但以当前存储边界（{mode,uid,gid}，无 xattr）**无标准机制可用**。
列为后续工作：待存储具备能力位存储后再评估；本变更不假装已实现。

### B. set-id / 模式位标记——不充分（有凭证耦合）

- **setuid 位**：exec 凭证（userland.c:453 `file.mode & 04000 → euid=file.uid`）
  会让**任何**执行者（含 uid 1000）拿到 euid 0——把 desktop.elf 变成 setuid-root
  等于开一条提权通道。直接否决。
- **setgid 位**（`02010` → `egid=file.gid`，userland.c:454）：改变三服务的运行
  凭证（egid 60001/60002）；更糟的是"setgid 但无组执行位"做角色位时，一次
  `chmod g+x` 就把角色标记变成给普通执行者发 egid 的提权。危险耦合，否决。
- **sticky 位**：对 exec 无语义（权限层只在目录用 sticky，permissions.c:405），
  可作惰性标记，但 desktop 与 windowd/imd 的角色差仍需第二维编码——第二维
  一旦回到 gid，就不如直接用 gid（方案 C）。且"root-owned sticky 常规文件"
  在导入的上游包里出现即误授，爆炸半径不可枚举。否决为首选。

### D. 仅把名字比较改大小写敏感——参考对照，不满足方向

一行改动即可让 m1c 转绿（`DESKTOP.ELF` ≠ `desktop.elf`），但**保留名字信任
核心**，正是 03 §2 指定要退出的判定方式；且硬链接/备份语义仍随路径漂移
（03 §6.5）。交接要求是"以文件属主/能力位替代名字判定"，此方案不满足。
列出仅作对照，不采用。

### C. 角色 gid（文件属主）——采用

见 §4。它把 03 §2 的"root 所有 + 无 TOCTOU 的镜像内权威"落到现有标准机制
（POSIX uid/gid/模式 + A7 锁内读）上，不新增存储基建。

## 4. 采用方案细则

### 4.1 授予规则（替换后的 M1）

以**保留 gid** 替换名字判定；三道既有闸门原样保留：

```text
授予条件：root 执行者（uid==0 && euid==0）
       && 镜像 root 所有（permissions.uid == 0）
       && 镜像非组/世界可写（!(permissions.mode & 0022)）
       && permissions.gid ∈ { G_WIN_SERVER, G_SERVICE }
角色：gid == G_WIN_SERVER → TASK_FLAG_SERVICE | TASK_FLAG_WINDOW_SERVER，记 desktop_pid
      gid == G_SERVICE    → TASK_FLAG_SERVICE，记 windowd_pid（imd_pid 不再单独记）
路径：不参与信任判定。
```

| 常量 | 值 | /etc/group | 镜像 | 授予 |
| --- | --- | --- | --- | --- |
| `G_WIN_SERVER` | 60001 | `leonos-window-server:x:60001:` | desktop.elf | SERVICE\|WINDOW_SERVER |
| `G_SERVICE` | 60002 | `leonos-service:x:60002:` | windowd.elf、imd.elf | SERVICE |

- gid 选 60001/60002：镜像 `/etc/group` 现用 0/10/14/1000/65534，常规系统组
  <1000；60000+ 为高位私用区，无碰撞。组无成员，非 root 无法 `chgrp` 进入。
- 常量落点按 `include/leonos/layout.h` 双仓并存先例：内核侧声明
  （kernel/ntclks/include/leonos/ 或 ntclks 私有头），staging 与测试镜像数值并
  由 §4.4 的盘点断言钉住一致性。
- `/etc/group` 增两条为纯追加：`test_image_accounts.py` 是 `assertIn` 子串断言、
  `test_installer_wheel.py` 只读绑定，均不钉全量内容，不受影响。

### 4.2 安全性质

- **无新增 TOCTOU**：`storage_inode_permissions(&node, …)` 读持住 inode 的
  `{mode,uid,gid}`，与 setuid/setgid 凭证同一锁域（A7 已验证）。
- **伪造闸门**：非 root 不能造 uid=0 文件（chown 需 `CAP_CHOWN`，
  permissions.c:459-481），不能进无成员保留组；root 才能 `chgrp` 标记镜像——
  而 root 本就是 TCB（等价于旧模型"root 安装到固定路径"的权威来源）。
- **fail-closed**：chown 走 uid、chmod 可写、chgrp 走角色——都丢权威而非误授。
- **硬链接/符号链接**：权威随 inode（setuid 同模型）；`cp -p` 备份保留标记是
  刻意语义（权威随镜像文件本身，03 §2"路径退出信任判定"），默认 `cp` 新建
  inode 不继承（gid 落执行者属组）。
- **与 m2/m3 一致**：exec 保留位仍只留 ELEVATED_ADMIN|WAITABLE_CHILD 后按新
  镜像重新判定；fork 剥离（sched.c:978-983）不动。

### 4.3 与负例夹具的交互（不删检查，只做隔离加固）

| 检查 | 现夹具 | 替代后 | 动作 |
| --- | --- | --- | --- |
| m1c 大小写变体 | `DESKTOP.ELF` root-owned 0755（gid 未显式设） | 无角色 gid → 无授予 | **转绿**；断言不变；夹具显式 `gid 0` 并注明"无标记副本必须惰性" |
| m1b 可写副本 | `Desktop.elf` 0777 root-owned | 同样无标记 | 夹具给角色 gid G_WIN_SERVER，使**可写**成为唯一拒绝原因（条件隔离）；断言不变 |
| 正控制 m1pos | `cp /tmp/pp` 覆盖 desktop.elf + chmod 0755 | 覆盖后 inode gid 保留，但不赌 `cp` 语义 | swap/restore 后显式 `chgrp 60001`；断言不变 |
| m1a/m1stock/m1w | 非 root exec | root 执行者闸门未过 | 不变 |
| m2/m3 | exec 清除、fork 剥离 | 机制未动 | 不变 |
| m14 | unverified | 死代码语义不复刻 | 不变 |

m1c 转绿的**原因**是"无标记副本不授予"，名字不再是信任输入；检查文本与
`untagged()` 断言一字不改。若实现后 m1c 仍授予，`Xfail` 机制保留并报告，
不得删检查过渡。

### 4.4 staging 与镜像链改动（主仓）

1. rootfs-stage.sh plan 增 gid 维度（或专用 record 字段），对三服务镜像
   fchown 60001/60002；`leonos-stage` 清单输出真 gid（:310-311 硬编码改为
   plan 值），`--check` 兼容。
2. installer-stage.sh：安装器 runtime 的 desktop.elf 同标记（安装器会话同样
   走 M1 授予点）。
3. `system/rootfs/etc/group` 增 §4.1 两行。
4. **盘点断言**（`make test` 的 python 测试，非生产链）：rootfs/安装器清单中
   携带角色 gid 的文件集合恰好等于 {desktop.elf, windowd.elf, imd.elf} 三个
   路径，防标记扩散；并断言 /etc/group 数值与夹具常量一致。

### 4.5 语义变化披露（诚实边界）

- 授权随镜像文件身份而非路径：root 以 `cp -p` 制作的备份执行时**仍带**权威
  （setuid 同模型）；这是"路径退出信任判定"的刻意代价，也是 03 §2 选定的
  取舍。未标记副本（如 m1c 夹具、`cp` 默认新建的副本）永远惰性。
- windowd/imd 不再区分 pid 变量（角色授予同型；两变量仅服务 OR 检查，可互
  换）；"no Ring-3 userland"检查行为不变。
- 死代码消费者（M14 免疫三函数、F3、`require_background_service`）不在本变
  更清理，也不复刻其语义（03 §5.3 订正）。

## 5. 明确不做

- 不实现文件能力位/xattr（§3.A，待存储基建）。
- 不清理死代码、不动 B5/B6 语义、不动 M4/M10/M17、不动 fork 剥离。
- 不以本变更之名行大范围重构（内核职责目录重排是 §5.7 另一项，逐组另行
  实施与测试）。

## 6. 实施与验证顺序

1. 本审查结论入库（本文件）。
2. 内核（NTCLKS）：授予规则替换为 §4.1，删名辅助函数（:83/:134-149），
   `spawn_path_internal_ex` 注释订正（03 §6.7），新/改函数补 Doxygen；子仓
   提交并推送。
3. staging（主仓）：§4.4 全部。
4. 夹具隔离加固（§4.3）；m1c 靠行为变化转绿，Xfail 路径保留。
5. 验证链（逐项记命令与退出码）：重建镜像 → `make test` 全量 → python tools
   （test_uapi/test_abi_layout/test_header_export + 盘点断言）→ privilege
   negatives 全量（预期 ok=13 xfail=0 unverified=1）→ rpr-pages 守卫不受影响。
6. 回退准则：若 leonos-stage 扩展与其契约测试出现不可调和冲突，**停止并报
   告**（不放松断言、不删检查），保留 m1c xfail 与现状。

## 7. 验证级别声明

本文件为**源码已检查 + 设计结论**：授予点、消费方、元数据、staging 与镜像
实证均已按 §2 逐项核对（构建产物 debugfs 实查）。实施与运行验证按 §6 另行
报告；本文不声称任何编译或运行结论。
