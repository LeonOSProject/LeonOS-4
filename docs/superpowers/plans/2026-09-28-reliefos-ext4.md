# ReliefOS ext4 基本特性实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `subagent-driven-development`（推荐）或 `executing-plans` 逐任务实现此计划。每完成一个复选框步骤就运行该步骤指定的命令；只有预期结果出现后才继续。每个任务完成后单独提交，提交信息使用本文给出的文本。

**目标：** 在 ReliefOS 中实现原生 ext4 基本特性，使 Linux v7.3-rc5/e2fsprogs 生成的常规 ext4 镜像可被 ReliefOS 读写、恢复和卸载，并使 ReliefOS 生成的 root 镜像可被 Linux 挂载和 `e2fsck` 验证，同时满足既定性能门槛。

**架构：** 保留现有 AHCI/IDE/NVMe 块传输和 `storage_node` VFS ABI，把 ext2 后端重构为统一 ext-family 后端。新增按职责拆分的 ext4 格式、checksum、缓存、extent、分配器、目录、journal 和 VFS ops 模块；旧 `ext2_*` 符号先保留为兼容 wrapper。所有 feature bit 先经过 Linux 语义策略检查，未知 `INCOMPAT` 不得静默忽略；本次只读写 ordered metadata journal 和已列入规格的基本特性。

**技术栈：** C11 内核代码；现有 ReliefOS AHCI/IDE/NVMe transport、执行锁、固定缓冲区和 VFS；Python 3 镜像工具；POSIX shell；Linux `mke2fs`/`e2fsck`/`debugfs`/`mkfs.ext4`；QEMU；host C11 ASAN/UBSAN 测试。

**规格：** [2026-09-28-reliefos-ext4-design.md](/home/xiaobai/Projects/Projects/ReliefOS/docs/superpowers/specs/2026-09-28-reliefos-ext4-design.md)

## 全局约束

- 只参考当前 `kernel/reliefnt`、当前仓库存储接口、`/home/xiaobai/Projects/Projects/ReliefOS/linux` v7.3-rc5 的 `fs/ext4` 和 `Documentation/filesystems/ext4`；历史提交中已废弃的 ext4 文档不得引用、复制或恢复。
- 保留 `kernel/reliefnt/drivers/bootstrap/storage.c` 的单一翻译单元 include 模式；每个新模块只能承担一个职责，并按 include 顺序解决静态函数可见性。
- 任何 on-disk 整数都通过 little-endian 读取/写入 helper 处理；不得把 packed struct 直接强转为可写磁盘布局后修改。
- 所有 block、sector、inode、group、extent index、journal sequence 计算使用 `uint64_t` 中间值，并在转换为 `uint32_t` 或地址前做上限检查。
- 未知 `feature_incompat` 或不支持的改变 on-disk 解释的 `feature_ro_compat` 必须返回 `-EOPNOTSUPP`；不得降级为 ext2 逻辑。
- 元数据变更必须先获得 journal handle；写入顺序固定为 data -> metadata descriptor/bitmap/inode/dir -> commit -> device flush。
- 所有 cache 容量、readahead、journal transaction 上限和超时必须是有名字的编译常量，并可在 host test 编译时覆盖。
- 保持 ext2、FAT32、exFAT、ISO9660、TMPFS 现有测试通过；默认 rootfs 切换到 ext4 时保留 `--root-fs ext2` 兼容测试入口。
- 每个任务只修改任务列出的文件；发现无关改动时停止该任务并记录 `git status --short`，不得使用 destructive git 命令清理它们。
- 每个任务结束时执行 `git diff --check`，再按任务给出的命令运行最小测试，最后使用指定 commit message 提交。
- 不能用“应该”“适当”“以后补充”等描述代替函数名、返回值、命令或验收条件；计划中的错误路径必须给出具体 errno 和清理动作。

## 文件清单与职责

### 新建文件

- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c`：superblock、group descriptor、inode、extent header、directory、xattr、journal on-disk 编解码；只包含纯格式函数和边界检查。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c`：CRC16、CRC32C、superblock/group/inode/extent/dir/journal checksum；提供 `storage_ext4_crc32c()`、`storage_ext4_verify_*()` 和 `storage_ext4_update_*()`。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_cache.c`：每卷 ext4 block/inode/extent/dir/journal cache，LRU 淘汰、dirty/pin、失效和统计。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_mount.c`：superblock 读取、feature policy、group descriptor 验证、journal 建立、replay、root inode 初始化和卸载。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_extent.c`：extent path 查找、hole、insert/merge/split/remove、extent status cache 和旧 indirect block 兼容。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_alloc.c`：flex group、block/inode bitmap、goal/reservation、free count、block/inode 回收。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_dir.c`：目录 entry 编解码、线性扫描、HTREE 查找/插入/删除、目录增长、`.`/`..` 和 dir checksum。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_journal.c`：ordered metadata journal、descriptor/data/commit/revoke、transaction、checkpoint、replay 和 flush。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_ops.c`：统一 ext-family 的 lookup/read/write/readdir/create/mkdir/link/unlink/rmdir/rename/truncate/symlink/fsync/fallocate/FIEMAP ops。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_xattr.c`：xattr block/header/entry 读写、`user.*` 基础操作、未知 security namespace 的只读/拒绝策略。
- `tools/make_ext4_root.py`：fakeroot + `mke2fs -t ext4` 的可重复 root image 生成器。
- `tools/tests/ext4_format_test.py`：镜像工具参数、feature mask、可重复输出和 e2fsck 检查。
- `tools/tests/ext4_interop_test.py`：Linux 生成/ReliefOS 生成镜像的双向数据集互操作驱动。
- `tools/tests/ext4_crash_replay_test.py`：journal 窗口故障注入、QEMU reset、重启后语义检查。
- `tools/tests/ext4_performance.py`：顺序读、随机读、小文件创建、fsync 和 cache 统计基准。
- `tools/tests/ext4_feature_matrix.py`：从参考 Linux 树和生成镜像验证 feature 支持/拒绝矩阵。
- `tools/tests/ext4_fixture.py`：共享临时镜像、mke2fs/e2fsck/debugfs 命令封装、测试数据集和 sha256 校验。
- `tools/tests/ext4_mount_test.py`、`tools/test_ext4_read.py`、`tools/test_ext4_write.py`、`tools/test_ext4_dir.py`、`tools/test_ext4_vfs.py`：分别驱动挂载、读取、写入、目录和 VFS fixture；这些脚本只负责准备镜像、调用 host/QEMU harness 和检查结果，格式语义由 C fixture/内核实现。

### 修改文件

- `kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h`：ext4 常量、feature bits、on-disk view、`storage_ext4_state`、volume 字段、filesystem enum、cache/journal/ops 原型。
- `kernel/reliefnt/drivers/bootstrap/storage.c`：按顺序 include 新 ext4 模块，保留兼容 wrapper include。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_mount.c`：GPT root 和 runtime boot 的 ext-family 探测、日志名称、mount/unmount 路由。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_vfs.c`：所有 ext2 分支迁移至统一 ext-family ops；path cache key 增加 volume/mount generation。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_inode.c`：inode refresh/metadata 写回使用 ext4 inode size、generation、checksum 和 64-bit size。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_sync.c`：ext4 journal commit、device flush、只读状态和错误传播。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_statfs.c`：ext4 free blocks/inodes、block size、feature flags 和 read-only 状态。
- `kernel/reliefnt/drivers/bootstrap/storage/storage_ext2.c`、`storage_ext2_write.c`、`storage_ext2_cache.c`：迁移期间改成 ext4 wrapper 或移除已迁移静态实现；禁止保留两套可写 allocator。
- `tools/make_image.py`：root-fs 默认值、ext4 populate、fstab 类型和 root image 名称。
- `tools/make_ext2_root.py`：保留 ext2 测试入口并抽出共享 staging 逻辑，默认生成由新 ext4 工具接管。
- `tools/build/images.sh`：增加 `ext4` 模式，显式 feature profile、时间戳、UUID 和 e2fsck。
- `mk/images.mk`：root/live/installer ext4 目标、stamp、host timestamp tool 和 ext2 compatibility 目标。
- `kernel/reliefnt/Kconfig`、`Kconfig`：rootfs 文案、默认 filesystem 配置和 ext4 feature profile 配置项。
- `userland/busybox/block_storage.c`、`userland/busybox/leonos_shim.c`、`userland/cmd/leonos_cmd_shim.c`：mkfs/fsck/mount 的 ext4 名称和 filesystem enum 路由。
- `userland/runtime/src/blockdev.c`：ext4 格式化/检查请求转发或明确报告内核驱动负责，避免生成 ext2 假镜像。
- `userland/apps/installer/main.c`：安装 fstab、root format、界面文本和 target mount 类型。
- `system/rootfs/etc/fstab`、`docs/FILESYSTEM.md`、`docs/ABI.md`、`docs/ADVANCED_INSTALL.md`、`docs/UPSTREAM_TOOLS.md`、`README.md`：当前 ext4 默认值和兼容范围；不得恢复废弃历史 ext4 文档。
- `tests/build/test-image-adapters.sh`、`tests/integration/test-motd-qemu.py`、`tests/integration/upstream-products.sh`、`mk/upstream.mk`、`scripts/doctor.sh`：ext4 工具名、镜像名、依赖探测和 fixture。
- `tools/tests/ext2_cache_test.c`、`ext2_write_batch_test.c`、`ext2_performance_test.c`：保留 ext2 fixture，增加 ext4 同样的参数化入口但不删除旧断言。

## 执行前的代理规则

- 执行 Agent 在开始任何任务前运行：

```bash
cd /home/xiaobai/Projects/Projects/ReliefOS
git status --short
git log -1 --oneline
```

- 如果工作树出现不是本任务产生的改动，记录文件名并继续只修改任务文件；若任务文件已有用户改动，先停止并报告，不能覆盖。
- 每一步只做一个可验证动作。命令返回非零时，保留完整输出，执行同一任务的“失败分支”步骤，不跳到后续任务。
- 每个任务提交后运行 `git show --stat --oneline HEAD`，确认提交只包含任务文件。
- 代码任务遵守 TDD：先增加最小失败测试，再运行确认失败，再写实现，再运行通过测试。
- 任务之间通过 commit 传递，不使用 `git reset --hard`、`git checkout --` 或删除未知文件回滚。
- 低成本代理可以按任务粒度一次只领取一个任务；依赖未满足时不能并行领取后续任务。

## 实现任务

### 任务 1：建立 ext4 feature 矩阵和可重复基线

**目的：** 把 Linux v7.3-rc5 的 ext4 feature、现有 ReliefOS 能力和本次支持/只读/拒绝结果固化为机器可读数据，避免后续任务自行猜测 feature bit。

**文件：**
- 创建：`tools/tests/ext4_feature_matrix.py`、`tools/tests/ext4_fixture.py`
- 修改：`docs/FILESYSTEM.md`（仅新增当前计划的 feature 表）
- 测试：`tools/tests/ext4_feature_matrix.py`

- [ ] **步骤 1：建立失败测试，验证矩阵字段完整。**

```bash
python3 tools/tests/ext4_feature_matrix.py --check-schema
```

预期：失败并报告矩阵文件尚不存在；退出码非零。

- [ ] **步骤 2：从参考树提取 feature 常量和 ext4 对象清单。**

实现 `load_linux_reference(root)`，读取 `linux/fs/ext4/super.c`、`linux/fs/ext4/ext4.h`、`linux/include/uapi/linux/ext4.h`（存在时）和 `linux/Documentation/filesystems/ext4`；输出字段 `name`, `mask`, `class`, `scope`, `status`, `reader_tests`, `writer_tests`。只把下列本次状态写成 `rw`：`extents`、`64bit`、`flex_bg`、`sparse_super`、`sparse_super2`、`uninit_bg`、`metadata_csum`、`gdt_csum`、`large_file`、`huge_file`、`extra_isize`、`dir_nlink`、`dir_index`、`has_journal`。`bigalloc`、`inline_data`、`casefold`、`encrypt`、`verity`、`quota`、`project`、`fast_commit`、`mmp`、`dax` 写成 `reject` 或 `api-eopnotsupp`，并注明触发错误。

- [ ] **步骤 3：增加 fixture 命令封装。**

在 `ext4_fixture.py` 中实现并固定签名：

```python
def run_checked(argv: list[str], *, cwd: Path | None = None) -> subprocess.CompletedProcess[str]: ...
def create_image(path: Path, size_mib: int, features: list[str], inode_size: int = 256) -> None: ...
def e2fsck_read_only(path: Path) -> None: ...
def debugfs(path: Path, request: str) -> str: ...
def sha256_tree(root: Path) -> dict[str, str]: ...
```

`create_image` 必须显式传 `-t ext4 -F -b 4096 -I <inode_size> -O <features> -m 0`，禁止依赖 host 的 mke2fs 默认 feature。

- [ ] **步骤 4：运行 schema 和 fixture 测试。**

```bash
python3 tools/tests/ext4_feature_matrix.py --check-schema
python3 -m py_compile tools/tests/ext4_feature_matrix.py tools/tests/ext4_fixture.py
```

预期：schema PASS，py_compile 无输出退出 0。

- [ ] **步骤 5：更新现行文件系统文档并检查来源边界。** 文档只说明本规格依据当前源码和 Linux 参考树；不得复制已删除设计文档中的 feature 表、接口名或测试命令。审查命令：

```bash
git diff -- docs/FILESYSTEM.md docs/superpowers/specs/2026-09-28-reliefos-ext4-design.md
```

预期：新增内容只描述当前实现、Linux 参考树和本计划的支持矩阵。

- [ ] **步骤 6：提交。**

```bash
git add tools/tests/ext4_feature_matrix.py tools/tests/ext4_fixture.py docs/FILESYSTEM.md
git diff --check
git commit -m "test: add ext4 feature matrix and image fixtures"
```

### 任务 2：定义 ext4 on-disk 类型、endian helper 和 volume 状态

**目的：** 先让格式层可在 host 上独立解析，不触碰 transport 和 VFS。

**文件：**
- 创建：`kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c`
- 修改：`kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h`
- 测试：新建 `tools/tests/ext4_format_test.c`

- [ ] **步骤 1：写解析失败测试。** 测试构造 1024-byte superblock，覆盖 magic、log block size、inode size、feature fields、64-bit block count 和 invalid geometry；先引用以下函数原型：

```c
int storage_ext4_parse_super(const uint8_t *raw, uint32_t raw_len,
                             struct storage_ext4_super_view *out);
int storage_ext4_parse_group_desc(const uint8_t *raw, uint32_t raw_len,
                                  uint32_t desc_size, struct storage_ext4_group_view *out);
int storage_ext4_parse_extent_header(const uint8_t *raw, uint32_t raw_len,
                                     struct storage_ext4_extent_header_view *out);
```

运行：

```bash
cc -std=c11 -O0 -g -fsanitize=address,undefined -Iinclude -Ikernel/reliefnt/include \
  tools/tests/ext4_format_test.c kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c \
  -o /tmp/reliefos-ext4-format-test && /tmp/reliefos-ext4-format-test
```

预期：编译失败，报告缺少 `storage_ext4_*` 实现。

- [ ] **步骤 2：在 header 增加固定宽度常量和 view 类型。** 定义 ext4 magic、super offset、feature masks、`EXT4_MIN_BLOCK_SIZE=1024`、`EXT4_MAX_BLOCK_SIZE=4096`、`EXT4_GOOD_OLD_INODE_SIZE=128`、descriptor minimum/maximum、extent depth limit、directory name limit；定义 `storage_ext4_super_view`（64-bit counts、uuid、label、feature masks、journal inode）、`storage_ext4_group_view`（64-bit bitmap/table、counts、checksum）和 `storage_ext4_state` 的几何字段。所有 view 字段使用 native integer，不直接暴露 packed disk struct。

- [ ] **步骤 3：实现 endian 和边界读取。** 在 `storage_ext4_format.c` 实现 `ext4_get_le16/32/64`、`ext4_put_le16/32/64`、`ext4_range_ok(raw_len, offset, size)`；任何 offset+size 溢出返回 `-EOVERFLOW`，raw 为 null 返回 `-EINVAL`。

- [ ] **步骤 4：实现 super/group/extent parser。** parser 必须拒绝 magic 错误、非 1024/2048/4096 block、inode size 非 128 的倍数或大于 block size、group count 为 0、blocks_per_group/inodes_per_group 为 0、extent header magic 错误、extent entries 超过 max；64-bit 字段只在 `64bit` feature 存在时组合 high/low。

- [ ] **步骤 5：运行测试并补齐断言。**

```bash
/tmp/reliefos-ext4-format-test
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/reliefos-ext4-format-test
```

预期：所有 valid fixture PASS；每个 invalid fixture 返回指定 errno；无 sanitizer 输出。

- [ ] **步骤 6：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c \
        tools/tests/ext4_format_test.c
git diff --check
git commit -m "feat: add ext4 on-disk format views"
```

### 任务 3：实现 checksum 层

**目的：** 在任何可写 mount 之前提供 Linux-compatible CRC32C/CRC16 和各类 metadata checksum，防止把损坏镜像当作健康镜像修改。

**文件：**
- 创建：`kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c`
- 修改：`kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h`
- 测试：新建 `tools/tests/ext4_checksum_test.c`

- [ ] **步骤 1：写 CRC 向量失败测试。** 使用公开 CRC32C 向量 `"123456789" -> 0xe3069283` 和 ext4 inode checksum fixture；声明 `storage_ext4_crc32c(seed, data, len)`、`storage_ext4_crc16(seed, data, len)`、`storage_ext4_verify_super_checksum()`、`storage_ext4_verify_group_checksum()`、`storage_ext4_verify_inode_checksum()`。

- [ ] **步骤 2：实现 table-driven CRC。** 建立 256 项只读 CRC32C 表和 CRC16 表；seed、length、null 指针检查明确返回 0 或 `-EINVAL`；不得在每个 block 计算中动态分配表。

- [ ] **步骤 3：实现 checksum seed 和 metadata checksum。** 按 UUID/metadata_csum_seed 选择 seed；校验前把 checksum 字段临时按零处理，校验后恢复；inode checksum 输入包括 inode number、generation、extra inode bytes；group descriptor checksum 输入包括 group number 和 descriptor bytes。

- [ ] **步骤 4：运行 host sanitizer 测试。**

```bash
cc -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude -Ikernel/reliefnt/include \
  tools/tests/ext4_checksum_test.c kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c \
  -o /tmp/reliefos-ext4-checksum-test
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/reliefos-ext4-checksum-test
```

预期：向量、valid metadata、single-byte corruption、wrong seed 全部按预期 PASS。

- [ ] **步骤 5：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c \
        tools/tests/ext4_checksum_test.c
git diff --check
git commit -m "feat: add ext4 metadata checksums"
```

### 任务 4：接入 ext4 block I/O、缓存和挂载策略

**目的：** 让真实块设备可以读取 ext4 superblock/group descriptors，并在未知 feature、边界错误和 checksum 错误时安全拒绝。

**文件：**
- 创建：`kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_cache.c`、`storage_ext4_mount.c`
- 修改：`storage_internal.h`、`storage.c`、`storage_mount.c`、`storage_sync.c`
- 测试：新建 `tools/tests/ext4_mount_test.c`、`tools/tests/ext4_mount_test.py`，现有 `tools/test_ext2_cache.py`

- [ ] **步骤 1：增加 mount parser 的失败测试。** 用 fake `storage_read_device` 提供 valid ext4、unknown incompat、ro-compat、bad partition boundary、bad group descriptor checksum、journal-present 镜像；C fixture 验证 parser 返回值，Python driver 负责构造临时镜像和汇总结果。先运行：

```bash
python3 tools/test_ext2_cache.py
```

预期：现有 ext2 测试继续 PASS，新的 ext4 mount case 因 `STORAGE_FILESYSTEM_EXT4` 和 mount entry 不存在而失败。

- [ ] **步骤 2：增加 filesystem enum 和 volume 状态。** 在 `storage_internal.h` 增加 `STORAGE_FILESYSTEM_EXT4`；将 `ext2_start_lba/ext2_sector_count` 改成 ext-family 通用字段并保留旧别名访问宏；在 `storage_volume` 增加 `storage_ext4_state ext4`、`mount_generation`、`read_only_reason`。

- [ ] **步骤 3：实现有界 block cache。** `storage_ext4_cache.c` 提供固定容量 `EXT4_BLOCK_CACHE_ENTRIES=64`、`EXT4_INODE_CACHE_ENTRIES=64`、`EXT4_DIR_CACHE_ENTRIES=32`、`EXT4_JOURNAL_CACHE_ENTRIES=32`；每个 entry 有 `valid`, `dirty`, `pinned`, `block`, `age`, `checksum_ok`, `volume_generation`。提供并实现：

```c
int storage_ext4_cache_read(struct storage_volume *, uint64_t block, void *out);
int storage_ext4_cache_get(struct storage_volume *, uint64_t block, uint8_t **data, bool for_write);
int storage_ext4_cache_mark_dirty(struct storage_volume *, uint64_t block);
int storage_ext4_cache_flush(struct storage_volume *);
void storage_ext4_cache_invalidate(struct storage_volume *);
```

- [ ] **步骤 4：实现 superblock/group descriptor mount。** `storage_ext4_mount()` 读取分区内 byte offset 1024，计算 block/group descriptor locations，验证分区边界和 checksums；`storage_ext4_feature_policy()` 只允许规格中的 rw feature，拒绝 unsupported incompat，标记 ro-only ro_compat；设置 `filesystem=STORAGE_FILESYSTEM_EXT4`，保持 ext2 volume 在旧格式下使用同一 parser。

- [ ] **步骤 5：改 root mount route。** `storage_mount.c` 按“exFAT -> ext-family probe”路由；ext4 成功时日志打印 `root=ext4`；runtime boot 仍只挂载 FAT32 ESP；root mount 失败必须清理 ext4 state/cache 并恢复旧 active volume。

- [ ] **步骤 6：运行 fake mount 和现有 storage 测试。**

```bash
python3 tools/tests/ext4_mount_test.py
python3 tools/test_ext2_cache.py
python3 tools/test_storage_sync.py
git diff --check
```

预期：ext4 valid/ro/reject 矩阵 PASS；ext2 cache、sync 及 transport 行为无回归。

- [ ] **步骤 7：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_cache.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_mount.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_mount.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_sync.c \
        tools/tests/ext4_mount_test.py
git diff --check
git commit -m "feat: mount ext4 volumes with bounded caches"
```

### 任务 5：实现 group descriptor、bitmap 和高性能分配器

**目的：** 正确支持 64-bit/flex_bg/uninit_bg，并让新文件优先获得连续 blocks，不在每次分配时从 group 0 线性扫描。

**文件：**
- 创建：`kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_alloc.c`
- 修改：`storage_internal.h`、`storage_ext4_mount.c`
- 测试：新建 `tools/tests/ext4_alloc_test.c`

- [ ] **步骤 1：写 allocator 失败测试。** fake disk 构造两个 flex group、跨 group descriptor、uninit bitmap 和 hole；断言 `storage_ext4_alloc_blocks(goal, len)` 返回连续 extent，`free_blocks`/bitmap 一致，跨 group 不越界。

- [ ] **步骤 2：定义 allocator API。** 在 header 中固定：

```c
int storage_ext4_read_group(struct storage_volume *, uint64_t group, struct storage_ext4_group_view *);
int storage_ext4_alloc_blocks(struct storage_volume *, uint64_t goal, uint32_t count,
                              uint64_t *first, uint32_t *allocated);
int storage_ext4_free_blocks(struct storage_volume *, uint64_t first, uint32_t count);
int storage_ext4_alloc_inode(struct storage_volume *, bool directory, uint64_t *ino);
int storage_ext4_free_inode(struct storage_volume *, uint64_t ino, bool directory);
```

- [ ] **步骤 3：实现 group descriptor checksum 和 bitmap cache。** descriptor address 使用 `desc_size` 和 64-bit group count；bitmap block 必须经过 partition boundary/checksum 检查；`uninit_bg` 首次写 bitmap 时初始化 valid free counts；free count 更新在内存状态和 journal dirty metadata 中同时记录。

- [ ] **步骤 4：实现 goal/reservation 分配。** 先在 inode 所属 flex group 从 goal 附近寻找连续 run，再按 flex group free count 选择候选；没有满足 run 时返回实际较短 run，不重复扫描已失败 group；每次分配更新 `next_goal_block` 和 reservation window。

- [ ] **步骤 5：实现 inode bitmap 分配和回收。** regular inode 使用 group-local free inode；directory 优先 free inode 多的 flex group；回收必须验证 inode 范围、bitmap 已置位、links 为 0，再清零 inode table 并减少 used_dirs（目录时）。

- [ ] **步骤 6：运行 allocator sanitizer 测试。**

```bash
cc -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude -Ikernel/reliefnt/include \
  tools/tests/ext4_alloc_test.c kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_alloc.c \
  -o /tmp/reliefos-ext4-alloc-test && /tmp/reliefos-ext4-alloc-test
```

预期：连续分配、碎片 fallback、跨 group、回收、double-free 拒绝和 bitmap checksum 测试 PASS。

- [ ] **步骤 7：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_alloc.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h \
        tools/tests/ext4_alloc_test.c
git diff --check
git commit -m "feat: add ext4 flex group allocator"
```

### 任务 6：实现 extent tree、旧 indirect 兼容和读路径

执行勘误（2026-09-29）：接手后采用内联执行。真实节点读取入口位于
`storage_vfs.c`，而不是 `storage_inode.c`；任务 6 增加最小 ext4 读分支及
`tools/tests/ext4_vfs_read_test.c`，验证 inode hold/refresh/read/release。
同时在 `storage.c` 接入 allocator/extent/ops，头文件统一声明新接口。
完整路径查找、变更操作和 ext-family 分派统一仍由任务 9–10 完成。

**目的：** 让 Linux ext4 regular file、hole 和大文件可以高效读取，同时继续读取 ext2/ext3 indirect inode。

**文件：**
- 创建：`storage_ext4_extent.c`
- 修改：`storage_ext4_format.c`、`storage_ext4_cache.c`、`storage_ext4_ops.c`、`storage_inode.c`
- 测试：新建 `tools/tests/ext4_extent_test.c`、`tools/test_ext4_read.py`

- [ ] **步骤 1：写失败测试。** 覆盖 inline extent、leaf extent、index depth 1/2、unwritten extent、hole、logical block overflow、checksum error 和 indirect inode；先运行：

```bash
python3 tools/test_ext4_read.py --case all
```

预期：fixture 尚未接入 ext4 ops，测试失败并指出缺失 `storage_ext4_map_block`。

- [ ] **步骤 2：定义 extent API 和 path 类型。** 固定以下接口：

```c
int storage_ext4_map_block(struct storage_volume *, uint64_t ino, const struct ext4_inode_view *,
                           uint64_t logical, bool allocate, struct storage_ext4_map_result *out);
int storage_ext4_insert_extent(struct storage_volume *, uint64_t ino, uint64_t logical,
                               uint64_t physical, uint32_t len, bool unwritten);
int storage_ext4_remove_range(struct storage_volume *, uint64_t ino, uint64_t start, uint64_t end);
int storage_ext4_fiemap(struct storage_volume *, uint64_t ino, uint64_t start, uint64_t len,
                        struct storage_ext4_fiemap_extent *out, uint32_t capacity, uint32_t *count);
```

- [ ] **步骤 3：实现 read-only extent lookup。** 从 inode `i_block` root 开始按 `eh_depth` 验证 header、entries、max；index key 使用 lower-bound，leaf extent 处理 initialized/unwritten/hole；每个 child block 都验证 block range 和 extent checksum；cache hit 只跳过设备读，不跳过结构校验。

- [ ] **步骤 4：实现 indirect fallback。** 当 `EXT4_EXTENTS_FL` 未设置时支持 direct、single、double、triple indirect；block pointer 读取使用 64-bit 中间值和 block size 限制；超过 supported mapping 返回 `-EFBIG`。

- [ ] **步骤 5：实现 readahead 读路径。** 在 `storage_ext4_ops.c` 中按 extent 连续物理 block 合并最多 `EXT4_READAHEAD_BLOCKS=32` 个 block，转换为现有 sector read；遇到 hole 用零填充；不为随机读扫描整个 extent tree。

- [ ] **步骤 6：运行 host/QEMU read 测试。**

```bash
cc -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude -Ikernel/reliefnt/include \
  tools/tests/ext4_extent_test.c kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_extent.c \
  -o /tmp/reliefos-ext4-extent-test && /tmp/reliefos-ext4-extent-test
python3 tools/test_ext4_read.py --case all
```

预期：extent/indirect/hole/FIEMAP 测试 PASS；读取吞吐记录 block size 和 readahead 命中率。

- [ ] **步骤 7：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_extent.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_cache.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_ops.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_inode.c \
        tools/tests/ext4_extent_test.c tools/test_ext4_read.py
git diff --check
git commit -m "feat: read ext4 extents and legacy indirect files"
```

### 任务 7：实现 journal/JBD2 ordered metadata 和 recovery

执行勘误（2026-09-29）：统一使用 `tools/tests/ext4_crash_replay_test.py`。
任务 7 同时更新受接口影响的 T5/T6 host fixtures 和 storage_sync_test 显式依赖；
在 storage.c 接线新模块。journal 状态包含动态分配的指针，因此同步修复
storage_mount.c / storage_disk.c 的 root→ESP 拷贝及卸载释放，避免共享所有权。
提前闭合任务 4 暂存问题：识别为 ext-family 的损坏镜像不再回退 legacy ext2。

**目的：** 提供 Linux ext4 镜像的崩溃一致性和挂载恢复，这是写路径进入真实磁盘前的阻塞依赖。

**文件：**
- 创建：`storage_ext4_journal.c`
- 修改：`storage_ext4_mount.c`、`storage_ext4_cache.c`、`storage_ext4_alloc.c`、`storage_sync.c`、`storage_internal.h`
- 测试：新建 `tools/tests/ext4_journal_test.c`、`tools/test_ext4_crash_replay.py`

- [ ] **步骤 1：写 journal parser/replay 失败测试。** 构造 descriptor/data/commit/revoke block，覆盖 valid sequence、checksum mismatch、short transaction、wrap-around、revoke 和 repeated replay；运行测试确认缺少实现。

- [ ] **步骤 2：定义 journal API。** 固定：

```c
int storage_ext4_journal_open(struct storage_volume *);
int storage_ext4_journal_start(struct storage_volume *, uint32_t credits,
                               struct storage_ext4_handle *out);
int storage_ext4_journal_dirty(struct storage_ext4_handle *, uint64_t block);
int storage_ext4_journal_stop(struct storage_ext4_handle *);
int storage_ext4_journal_commit(struct storage_volume *, bool wait);
int storage_ext4_journal_replay(struct storage_volume *);
int storage_ext4_journal_checkpoint(struct storage_volume *);
```

- [ ] **步骤 3：实现 journal superblock、ring 和 checksum。** 读取 journal inode 或 external journal 拒绝；验证 first/sequence/start/maxlen/blocksize；transaction block 只在 checksum、sequence、target block 和 revoke 全部有效时重放；不完整 commit 停止于上一事务。

- [ ] **步骤 4：实现 ordered transaction。** handle 记录 credits 和 dirty metadata blocks；`journal_stop` 在 credits 不足时返回 `-ENOSPC` 并释放 handle；commit 前 flush data blocks，commit 后 device flush；checkpoint 后回收 journal ring。

- [ ] **步骤 5：接入 mount/recovery/sync。** mount 在 cache 建立前 replay；replay 失败把 volume 标记 `read_only_reason=JOURNAL_CORRUPT`；`storage_sync_volume()` 调用 journal commit/checkpoint 后调用现有 transport flush；unmount 只在无 active handle 时写 clean state。

- [ ] **步骤 6：运行 sanitizer 和故障注入。**

```bash
cc -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude -Ikernel/reliefnt/include \
  tools/tests/ext4_journal_test.c kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_journal.c \
  -o /tmp/reliefos-ext4-journal-test && /tmp/reliefos-ext4-journal-test
python3 tools/test_ext4_crash_replay.py --mode host-fixture
```

预期：合法事务 replay 后数据块与 metadata 一致；损坏事务只读失败；重复 replay 不改变结果；无死循环。

- [ ] **步骤 7：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_journal.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_mount.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_cache.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_alloc.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_sync.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h \
        tools/tests/ext4_journal_test.c tools/test_ext4_crash_replay.py
git diff --check
git commit -m "feat: add ordered ext4 journal recovery"
```

### 任务 8：实现 inode 写回、extent 分配、truncate 和 fallocate

**目的：** 在 journal 已可用后实现 regular file 的高性能写入、扩展、稀疏 hole、truncate 和基本 fallocate。

**文件：**
- 修改：`storage_ext4_extent.c`、`storage_ext4_alloc.c`、`storage_ext4_journal.c`、`storage_ext4_ops.c`、`storage_inode.c`
- 测试：新建 `tools/tests/ext4_write_test.c`、`tools/test_ext4_write.py`

- [ ] **步骤 1：写失败测试。** 覆盖 partial block write、连续 write 合并 extent、hole write、unwritten extent conversion、truncate grow/shrink、`KEEP_SIZE`、`PUNCH_HOLE`、`ZERO_RANGE`、ENOSPC 和 fsync 后重启。

- [ ] **步骤 2：实现 inode view 与 checksum 写回。** 读取 inode size low/high、blocks、flags、generation、timestamps、extra_isize；写回前更新 ctime/mtime、i_blocks、checksum；inode table block 通过 cache + journal dirty 标记。

- [ ] **步骤 3：实现 extent insert/merge/split。** 先尝试与前后 extent 合并；无法合并时从 leaf 向 root 分裂；depth、entries、physical block 和 len 每一步检查；新 tree block 通过 allocator 分配并在同一 transaction 中写入。

- [ ] **步骤 4：实现 write/truncate。** partial block 读-modify-write；连续 full blocks 一次 allocator 请求；更新 inode size 后 dirty inode；truncate 从高 logical block 向低 logical block 释放 extent/indirect blocks，再更新 free counts。

- [ ] **步骤 5：实现 fallocate 子集。** `KEEP_SIZE` 预分配 unwritten extents；`PUNCH_HOLE` 释放完整 blocks 并清零边界；`ZERO_RANGE` 写零或标记 unwritten；对 `COLLAPSE_RANGE`、`INSERT_RANGE`、`UNSHARE_RANGE` 返回 `-EOPNOTSUPP`。

- [ ] **步骤 6：运行写路径测试。**

```bash
cc -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude -Ikernel/reliefnt/include \
  tools/tests/ext4_write_test.c kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_extent.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_alloc.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_journal.c \
  -o /tmp/reliefos-ext4-write-test && /tmp/reliefos-ext4-write-test
python3 tools/test_ext4_write.py --case all
```

预期：每个写入 fixture 卸载后 `e2fsck -f -n` PASS，Linux loop mount 读取 hash 与写入前一致；ENOSPC 不泄漏 block/inode。

- [ ] **步骤 7：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_extent.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_alloc.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_journal.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_ops.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_inode.c \
        tools/tests/ext4_write_test.c tools/test_ext4_write.py
git diff --check
git commit -m "feat: write ext4 extents with fallocate support"
```

### 任务 9：实现目录、HTREE、link 和 rename

**目的：** 让 Linux 普通目录树、长文件名、硬链接和原子重命名在 ext4 上可读写，并保持目录 checksum/索引一致。

**文件：**
- 创建：`storage_ext4_dir.c`
- 修改：`storage_ext4_ops.c`、`storage_ext4_extent.c`、`storage_ext4_alloc.c`、`storage_ext4_journal.c`、`storage_internal.h`
- 测试：新建 `tools/tests/ext4_dir_test.c`、`tools/test_ext4_dir.py`

- [ ] **步骤 1：写失败测试。** 覆盖 `.`/`..`、rec_len 对齐、长名、目录 block 边界、HTREE hash collision、index split、delete/reuse、hard link count、same-dir rename、cross-dir rename、rename overwrite 和 non-empty rmdir。

- [ ] **步骤 2：实现 dirent parser/writer。** 固定 `storage_ext4_dir_iterate()`、`storage_ext4_dir_lookup()`、`storage_ext4_dir_insert()`、`storage_ext4_dir_remove()`；验证 inode range、rec_len >= minimum、rec_len 不跨 block、name_len <= rec_len-8、file_type 合法；坏 entry 返回 `-EIO` 并标记只读错误。

- [ ] **步骤 3：实现 linear fallback 和 directory growth。** 目录未设 dir_index 时扫描 cached directory blocks；无空间时分配新 block，初始化 rec_len；mkdir 原子创建 `.`/`..` 和 parent link count。

- [ ] **步骤 4：实现 HTREE。** 读取 root info、hash version、limit/count；使用 Linux half-MD4/TEA hash，按 hash lower-bound 选择 leaf；插入时更新 count、必要时 split；索引 checksum/feature 不一致时只退回 linear scan并禁止修改索引。

- [ ] **步骤 5：接入 link/unlink/rmdir/rename。** 所有 parent/child inode、dirent、link count、ctime 更新属于同一 journal transaction；跨目录 rename 先验证目标类型与空目录，再删除旧 entry、插入新 entry；失败按 journal 回滚。

- [ ] **步骤 6：运行测试。**

```bash
cc -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude -Ikernel/reliefnt/include \
  tools/tests/ext4_dir_test.c kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_dir.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_extent.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_journal.c \
  -o /tmp/reliefos-ext4-dir-test && /tmp/reliefos-ext4-dir-test
python3 tools/test_ext4_dir.py --count 10000
```

预期：10,000 个文件的 HTREE lookup 与线性 fallback 结果一致；所有 mutation 后 e2fsck PASS。

- [ ] **步骤 7：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_dir.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_ops.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_extent.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_alloc.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_journal.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h \
        tools/tests/ext4_dir_test.c tools/test_ext4_dir.py
git diff --check
git commit -m "feat: add ext4 directories and htree indexing"
```

### 任务 10：统一 VFS ops 并保留 ext2 兼容

**目的：** 把所有 ext2 条件分支迁移到 ext-family ops，避免 ext4 只支持 lookup/read 而遗漏 syscall mutation。

**文件：**
- 修改：`storage_ext4_ops.c`、`storage_vfs.c`、`storage_mount.c`、`storage_inode.c`、`storage_internal.h`
- 修改/过渡：`storage_ext2.c`、`storage_ext2_write.c`、`storage_ext2_cache.c`
- 测试：现有 `tools/test_storage_mkdir_mount.py`、`test_storage_rename.py`、`test_storage_metadata.py`、`test_regular_file_io.py`；新建 `tools/test_ext4_vfs.py`

- [ ] **步骤 1：列出并替换所有 ext2 分支。**

```bash
rg -n 'STORAGE_FILESYSTEM_EXT2|ext2_' kernel/reliefnt/drivers/bootstrap/storage/storage_vfs.c kernel/reliefnt/drivers/bootstrap/storage/storage_mount.c kernel/reliefnt/drivers/bootstrap/storage/storage_inode.c
```

为每个 lookup/read/write/readdir/mkdir/unlink/rmdir/rename/link/truncate/symlink/special/fsync 分支改用 `storage_ext4_is_ext_family()` 和 `storage_ext4_ops`；保留日志文本通过 `storage_root_filesystem_name()` 返回 ext2/ext4。

- [ ] **步骤 2：定义 ops 表。** 在 header 固定 `struct storage_ext4_ops` 字段：`lookup`, `read`, `write`, `readdir`, `create`, `mkdir`, `link`, `unlink`, `rmdir`, `rename`, `truncate`, `symlink`, `readlink`, `fsync`, `fallocate`, `fiemap`, `getxattr`, `setxattr`, `statfs`；每个函数接收 `struct storage_volume *`，不得读取全局 active volume。

- [ ] **步骤 3：迁移 node flags 和 refresh。** `STORAGE_NODE_FLAG_EXT2` 改成 ext-family flag，node handle 保存 volume id、mount generation、inode number、inode generation；inode refresh 检测 generation 不匹配返回 `-ESTALE`。

- [ ] **步骤 4：保留 ext2 wrapper。** 对没有 `EXT4_EXTENTS_FL` 且使用 128-byte inode 的旧 ext2 image，wrapper 调用 ext4 indirect/linear implementation；旧测试中的 `ext2_mount()` 符号通过 wrapper 保持链接，不保留第二个 allocator/cache。

- [ ] **步骤 5：运行现有和新增 VFS 测试。**

```bash
python3 tools/test_storage_mkdir_mount.py
python3 tools/test_storage_rename.py
python3 tools/test_storage_metadata.py
python3 tools/test_regular_file_io.py
python3 tools/test_ext4_vfs.py --filesystem ext4
python3 tools/test_ext4_vfs.py --filesystem ext2
```

预期：ext4/ext2 两组操作结果一致；FAT32/exFAT/TMPFS 分支未被调用或行为无变化。

- [ ] **步骤 6：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_ops.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_vfs.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_mount.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_inode.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_internal.h \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext2.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext2_write.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext2_cache.c \
        tools/test_ext4_vfs.py
git diff --check
git commit -m "refactor: route ext2 and ext4 through one VFS backend"
```

### 任务 11：实现 xattr、statfs、FIEMAP、statx 和错误边界

**目的：** 补齐 Linux 用户态检查工具和基本文件管理程序需要的 ext4 metadata API，同时明确不支持的 ioctl/API。

**文件：**
- 创建：`storage_ext4_xattr.c`
- 修改：`storage_ext4_ops.c`、`storage_statfs.c`、`storage_vfs.c`、相关 UAPI/ABI 头（只在现有接口缺少字段时）
- 测试：新建 `tools/tests/ext4_metadata_api_test.py`、`tools/tests/ext4_xattr_test.c`

- [ ] **步骤 1：盘点现有 syscall/ioctl。**

```bash
rg -n 'FIEMAP|STATX|fallocate|xattr|getxattr|setxattr|FS_IOC|statfs|fsync|fdatasync' kernel/reliefnt userland include tools/tests
```

为当前已有 syscall 写映射表：输入校验、ext4 ops、返回结构、errno；缺少实现的 Linux API 不添加半成品 syscall。

- [ ] **步骤 2：实现 xattr block parser。** 支持 ext4 xattr header、entry name index/name length/value offset/value size；`user.*` 小值和外部 xattr block 读写纳入 journal；`security.*` 在没有 LSM owner 时返回 `-EOPNOTSUPP`；未知 index 返回 `-EOPNOTSUPP`；checksum 错误返回 `-EIO`。

- [ ] **步骤 3：实现 statfs/FIEMAP/statx 基础。** statfs 返回 block size、blocks、free/available blocks、files/free files、fsid、name max；FIEMAP 使用 extent lookup 输出 logical/physical/length/flags；statx 填 mode/uid/gid/size/blocks/timestamps/inode/dev/mount id，未实现字段清零并清除 mask。

- [ ] **步骤 4：实现 ioctl/fallocate 拒绝矩阵。** 对 collapse/insert/unshare、fscrypt、fsverity、quota、DAX、direct-I/O 专用请求返回 `-EOPNOTSUPP`；对只读卷写入返回 `-EROFS`；对未知 ioctl 返回现有统一 `-ENOTTY`。

- [ ] **步骤 5：运行测试。**

```bash
cc -std=c11 -O1 -g -fsanitize=address,undefined -Iinclude -Ikernel/reliefnt/include \
  tools/tests/ext4_xattr_test.c kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_format.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_checksum.c \
  kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_xattr.c \
  -o /tmp/reliefos-ext4-xattr-test && /tmp/reliefos-ext4-xattr-test
python3 tools/tests/ext4_metadata_api_test.py --case all
```

预期：xattr checksum/value/permission、statfs、FIEMAP、statx 和 unsupported errno 测试 PASS。

- [ ] **步骤 6：提交。**

```bash
git add kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_xattr.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_ext4_ops.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_statfs.c \
        kernel/reliefnt/drivers/bootstrap/storage/storage_vfs.c \
        tools/tests/ext4_metadata_api_test.py tools/tests/ext4_xattr_test.c
git diff --check
git commit -m "feat: expose ext4 metadata APIs and xattrs"
```

### 任务 12：切换镜像生成器和默认 rootfs 到 ext4

**目的：** 让 ReliefOS 自己生成的 root/live/installer 镜像使用显式稳定的 ext4 feature profile，并保留 ext2 compatibility 生成路径。

**文件：**
- 创建：`tools/make_ext4_root.py`
- 修改：`tools/make_image.py`、`tools/make_ext2_root.py`、`tools/build/images.sh`、`mk/images.mk`、`kernel/reliefnt/Kconfig`、`Kconfig`
- 测试：`tools/tests/ext4_format_test.py`、`tests/build/test-image-adapters.sh`、`tools/test_make_image.py`

- [ ] **步骤 1：为 image adapter 写失败测试。** 将 `test-image-adapters.sh` 增加 ext4 case，断言输出文件名、fstab type、feature mask 和 e2fsck；先运行确认 `images.sh ext4` 未知 mode 失败。

- [ ] **步骤 2：实现 `make_ext4_root.py`。** 复用 ext2 staging/ownership 函数但创建命令固定为：

```text
mke2fs -q -t ext4 -F -b 4096 -I 256
-O extents,dir_index,metadata_csum,64bit,flex_bg,has_journal,large_file,huge_file,extra_isize
-E root_owner=0:0,hash_seed=<uuid> -U <uuid> -m 0 -N <count> -d <stage> <image>
```

函数签名为 `populate_ext4(stage: Path, image: Path, inode_count: int, uuid_text: str) -> None`；随后运行 timestamp helper 和 `e2fsck -f -n`。

- [ ] **步骤 3：更新 `images.sh`。** 增加 `ext4)` 分支；检查四个参数、stage directory、UUID 格式、integer epoch；使用 `fakeroot` 和显式 mke2fs command；输出失败时删除工作目录并返回非零；保留 `ext2)` 原逻辑。

- [ ] **步骤 4：更新 Python VMDK builder。** `make_image.py` 默认 `--root-fs ext4`，choices 为 `("ext4", "ext2")`；按 filesystem 选择 `populate_ext4` 或 `populate_ext2`；fstab 写入 `ext4` 或 `ext2`；root image 默认名为 `root.ext4`；帮助文本和 GPT partition type 保持 Linux filesystem。

- [ ] **步骤 5：更新 make targets。** `mk/images.mk` 增加 `ROOT_EXT4`、`DISK_ROOT_EXT4`、`INSTALLER_EXT4` 和对应 ext4 timestamp dependency；`image-vmdk`/`iso`/`installer` 默认依赖 ext4；单独保留 `root-ext2-compat` target；所有 stamp signature 包含 mke2fs/e2fsck 版本和 feature profile。

- [ ] **步骤 6：运行 image tests。**

```bash
python3 tools/tests/ext4_format_test.py
sh tests/build/test-image-adapters.sh
python3 tools/test_make_image.py
```

预期：同一 `SOURCE_DATE_EPOCH` 和 UUID 生成字节一致的 ext4 image；`tune2fs -l`/debugfs 显示指定 feature；e2fsck PASS；ext2 compatibility case 仍 PASS。

- [ ] **步骤 7：提交。**

```bash
git add tools/make_ext4_root.py tools/make_ext2_root.py tools/make_image.py \
        tools/build/images.sh mk/images.mk kernel/reliefnt/Kconfig Kconfig \
        tools/tests/ext4_format_test.py tests/build/test-image-adapters.sh \
        tools/test_make_image.py
git diff --check
git commit -m "build: generate ext4 root images by default"
```

### 任务 13：更新 installer、BusyBox、runtime 和文档契约

**目的：** 让安装、挂载、mkfs/fsck 命令和用户可见文档使用同一 ext4 默认值，避免 kernel 支持 ext4 但安装器仍写 ext2 fstab。

**文件：**
- 修改：`userland/busybox/block_storage.c`、`userland/busybox/leonos_shim.c`、`userland/cmd/leonos_cmd_shim.c`、`userland/runtime/src/blockdev.c`、`userland/apps/installer/main.c`、`mk/upstream.mk`、`scripts/doctor.sh`、`tests/integration/upstream-products.sh`、`system/rootfs/etc/fstab`、`docs/FILESYSTEM.md`、`docs/ABI.md`、`docs/ADVANCED_INSTALL.md`、`docs/UPSTREAM_TOOLS.md`、`README.md`
- 测试：`tools/test_installer_setup.py`、`tools/test_upstream_tools_images.py`、`tools/test_make_image.py`

- [ ] **步骤 1：定义用户态 enum 映射。** 在 `userland/runtime/src/blockdev.c` 增加 `RELIEFOS_BLOCK_FILESYSTEM_EXT4`，名字返回 `ext4`；`format_partition()` 选择 ext4 时调用 kernel `mkfs.ext4` path 或返回明确 unsupported，不得调用 ext2 formatter 伪装成功。

- [ ] **步骤 2：增加 BusyBox applet aliases。** `mkfs.ext4`/`fsck.ext4`/`mount -t ext4` 路由到现有 block storage dispatcher；若实际格式化检查由 upstream e2fsprogs 提供，则 applet 只验证参数和转发，不自己写不完整 ext4 superblock。保留 ext2 aliases。

- [ ] **步骤 3：更新 installer。** root fstab 写 `ext4`；format selection 使用 ext4；所有“ext2 root”文本改成“ext4 root”；update path 接受 ext2/ext3/ext4，但写入的新安装目标固定 ext4；挂载失败时显示实际 errno 和 feature mask。

- [ ] **步骤 4：更新 upstream packaging/doctor。** `mk/upstream.mk`、upstream product list 和 doctor 检查 `mkfs.ext4`/`fsck.ext4`/libext2fs；测试同时保留 ext2 compatibility binary，不删除已有旧工具验证。

- [ ] **步骤 5：更新文档并做 stale scan。** `docs/FILESYSTEM.md` 说明默认 ext4 profile、ext2 read/write compatibility 和拒绝 feature；`docs/ABI.md` 说明 filesystem enum/errno；`docs/ADVANCED_INSTALL.md` 命令改成 ext4。执行：

```bash
rg -n 'root\.ext2|ext2 root|mkfs\.ext2|fsck\.ext2| / ext2 |mount -t ext2' \
  userland system mk tools tests docs README.md --glob '!docs/superpowers/**'
```

预期：只剩明确的 ext2 compatibility test/legacy documentation lines，每一行有“兼容”上下文；默认安装/构建路径不再写 ext2。

- [ ] **步骤 6：运行用户态测试并提交。**

```bash
python3 tools/test_installer_setup.py
python3 tools/test_upstream_tools_images.py
python3 tools/test_make_image.py
git diff --check
git add userland mk/upstream.mk scripts/doctor.sh tests/integration \
        system/rootfs/etc/fstab docs/FILESYSTEM.md docs/ABI.md \
        docs/ADVANCED_INSTALL.md docs/UPSTREAM_TOOLS.md README.md
git commit -m "feat: make ext4 the default installed root filesystem"
```

### 任务 14：补齐 ext4 host fixture、互操作和现有回归入口

**目的：** 将所有 ext4 基本特性放入可重复自动化测试，验证 Linux 双向互操作而不是只验证 ReliefOS 自己生成的镜像。

**文件：**
- 创建：`tools/tests/ext4_interop_test.py`、`tools/tests/ext4_crash_replay_test.py`、`tools/tests/ext4_performance.py`
- 修改：`tools/tests/ext4_fixture.py`、现有 `tools/test_ext2_performance.py`、`tools/test_ext2_write_batch.py`、`tests/integration/test-motd-qemu.py`

- [ ] **步骤 1：实现 Linux -> ReliefOS fixture。** `ext4_interop_test.py --direction linux-to-reliefos` 创建 ext4 image，使用 Linux loop mount 或 debugfs 写入：深目录、10,000 小文件、4 MiB 连续文件、稀疏文件、硬链接、symlink、长名、user xattr；保存 `sha256_tree`、stat 和 FIEMAP goldens。

- [ ] **步骤 2：实现 ReliefOS -> Linux fixture。** 从 ReliefOS QEMU guest 或 host storage fixture 创建同一数据集，clean unmount 后运行 `e2fsck -f -n`、`debugfs -R 'stat /path'`、Linux loop mount 和 sha256 comparison；错误输出保存到 `build/logs/ext4-interop/`。

- [ ] **步骤 3：实现 crash replay driver。** `ext4_crash_replay_test.py` 在 host fake journal 的四个写窗口注入 power loss，在 QEMU 模式使用 monitor `system_reset`；每次重启先只读 mount，检查旧/新版本二选一、目录 entry 完整、free count 与 e2fsck 一致。

- [ ] **步骤 4：参数化现有 ext2 tests。** `test_ext2_performance.py`、`test_ext2_write_batch.py` 增加 `--filesystem ext2|ext4`；默认仍跑 ext2 compatibility，CI 增加 ext4 invocation；测试输出 JSON 字段 `filesystem`, `image_features`, `transport`, `cache_config`。

- [ ] **步骤 5：运行 host interop。**

```bash
python3 tools/tests/ext4_interop_test.py --direction linux-to-reliefos --mode fixture
python3 tools/tests/ext4_interop_test.py --direction reliefos-to-linux --mode fixture
python3 tools/tests/ext4_crash_replay_test.py --mode host-fixture --iterations 32
python3 tools/test_ext2_performance.py --filesystem ext2
python3 tools/test_ext2_performance.py --filesystem ext4
python3 tools/test_ext2_write_batch.py --filesystem ext2
python3 tools/test_ext2_write_batch.py --filesystem ext4
```

预期：双向 hash/stat/e2fsck PASS；每个 crash iteration 有 replay result；旧 ext2 基线仍可运行。

- [ ] **步骤 6：提交。**

```bash
git add tools/tests/ext4_fixture.py tools/tests/ext4_interop_test.py \
        tools/tests/ext4_crash_replay_test.py tools/tests/ext4_performance.py \
        tools/test_ext2_performance.py tools/test_ext2_write_batch.py \
        tests/integration/test-motd-qemu.py
git diff --check
git commit -m "test: add ext4 interoperability and crash fixtures"
```

### 任务 15：QEMU boot、性能优化和发布门禁

**目的：** 在所有功能测试通过后验证真实 ReliefOS 启动，并用同场 ext2 基线确认性能没有退化到不可接受。

**文件：**
- 修改：`tools/tests/ext4_performance.py`、`tools/test_storage_upstream_guest.py`、`tools/test_storage_upstream_runtime.py`、`docs/FILESYSTEM.md`
- 创建：`docs/reports/ext4-performance-2026-09-28.json`、`docs/reports/ext4-compatibility-2026-09-28.md`
- 测试：现有 QEMU storage/rootfs suites 和全局 build checks

- [ ] **步骤 1：运行 clean build 和 rootfs image。**

```bash
make clean
make -j"$(nproc)" image-vmdk
test -s build/images/root.ext4
e2fsck -f -n build/images/root.ext4
```

预期：build 结束 0；root.ext4 存在且非空；e2fsck 不报告错误。

- [ ] **步骤 2：运行 QEMU root boot。**

```bash
python3 tools/test_storage_upstream_guest.py --filesystem ext4
python3 tools/test_storage_upstream_runtime.py --filesystem ext4
python3 tools/test_motd_qemu.py --root build/images/root.ext4
```

预期：内核识别 `root=ext4`，完成 `/`、`/boot`、`/dev`、`/proc`、`/sys` 相关挂载；guest fsync/read/write/rename 测试 PASS；没有 “unknown incompat” 或 transport timeout。

- [ ] **步骤 3：运行性能基准。**

```bash
python3 tools/tests/ext4_performance.py --filesystem ext2 --output /tmp/ext2-perf.json
python3 tools/tests/ext4_performance.py --filesystem ext4 --output /tmp/ext4-perf.json
python3 tools/tests/ext4_performance.py --compare /tmp/ext2-perf.json /tmp/ext4-perf.json
```

比较输出必须包含顺序读、随机 4 KiB 读、10,000 小文件创建、1,000 fsync p95、CPU time、journal commits、extent cache hit rate、HTREE hit rate；满足规格中的 80%/70%/2x 门槛，否则进入步骤 4。

- [ ] **步骤 4：只在有数据时调整性能。** 根据 benchmark counters 调整 named constants（cache entries、readahead blocks、reservation window、transaction credits）；每次只改一个常量，重新跑同一 compare 命令，并把前后 JSON 保留在 `build/logs/ext4-perf/`。禁止通过关闭 checksum、journal 或边界检查来提高速度。

- [ ] **步骤 5：运行全局回归和静态检查。**

```bash
python3 tools/tests/ext4_feature_matrix.py --check-schema
python3 tools/test_storage_metadata.py
python3 tools/test_storage_mkdir_mount.py
python3 tools/test_storage_rename.py
python3 tools/test_storage_sync.py
python3 tools/test_storage_write_batch.py
python3 tools/test_ext2_cache.py
python3 tools/test_tmpfs.py
python3 tools/test_live_iso.py
git diff --check
```

预期：所有命令退出 0；没有 ext2/FAT32/exFAT/ISO/TMPFS 回归。

- [ ] **步骤 6：生成报告并提交。** 把实际命令、host/kernel commit、QEMU 版本、镜像 feature mask、性能 JSON 摘要、失败 fixture 和未实现 feature 列入报告；报告只能陈述实际输出。

```bash
git add tools/tests/ext4_performance.py docs/reports/ext4-performance-2026-09-28.json \
        docs/reports/ext4-compatibility-2026-09-28.md
git diff --check
git commit -m "test: record ext4 compatibility and performance gates"
```

## 执行 Plan：低成本 Agent 调度顺序

### 依赖 DAG

```text
任务1 -> 任务2 -> 任务3 -> 任务4
                         |
                         +-> 任务5 -> 任务6 -> 任务7 -> 任务8
                                                    |
                                                    +-> 任务9 -> 任务10 -> 任务11
                                                                         |
                                                                         +-> 任务12 -> 任务13
任务10 + 任务12 ----------------------------------------------------> 任务14
任务10 + 任务12 + 任务13 + 任务14 -------------------------------> 任务15
```

### 每个子代理的固定启动提示

将下列文字与具体任务编号一起发送给低成本 Agent：

```text
你正在执行 ReliefOS ext4 计划中的“任务 N”。先读取 docs/superpowers/specs/2026-09-28-reliefos-ext4-design.md 和 docs/superpowers/plans/2026-09-28-reliefos-ext4.md 中的任务 N。只修改该任务列出的文件；不要恢复历史 ext4 文档。先运行计划中的失败测试，再按步骤实现。每一步都记录命令和结果；测试失败时停在该任务并报告完整 stderr。任务完成后运行 git diff --check、任务测试和 git show --stat --oneline HEAD，并使用计划指定的 commit message 提交。
```

### 推荐调度批次

1. **批次 A（单代理，顺序）**：任务 1、2、3、4。它们共享格式 view、checksum 和 mount state，不能并行修改 `storage_internal.h`。
2. **批次 B（单代理，顺序）**：任务 5、6、7、8。allocator、extent 和 journal 有双向依赖，使用同一代理减少接口漂移。
3. **批次 C（可分两个代理但必须串行合并）**：任务 9、10、11。任务 9 完成后再启动任务 10；任务 11 必须等待 ops 表稳定。
4. **批次 D（先后顺序）**：任务 12 等待任务 1；任务 13 等待任务 12 的文件名和默认值稳定；任务 14 等待任务 10 和任务 12，因为它需要完整 VFS ops 与 ext4 image builder。任务 14 的 fixture 编写可以由独立代理进行，但只能在依赖 commit 可见后运行。
5. **批次 E（主代理执行）**：任务 15 必须在所有前置 commit 合并后执行，负责 QEMU、性能调参、全局回归和报告。

### 每个任务的审查检查点

- **接口检查：** 新增函数的声明只在 `storage_internal.h` 出现一次；调用者使用同一参数顺序、返回值和 errno；没有使用未定义的 `storage_ext4_*` 名称。
- **磁盘检查：** 对每个新写入 block，审查其分区边界、block range、checksum、journal handle 和 flush 顺序；对每个 free 操作审查 bitmap double-free 防护。
- **锁检查：** 不在 transport I/O、journal replay 或 cache eviction 中持有不可睡眠的执行锁；active volume 切换前后保存并恢复指针。
- **兼容检查：** ext2 无 extents 镜像走 indirect fallback；FAT32/exFAT/ISO/TMPFS 不经过 ext4 ops；未知 incompat 不挂载。
- **测试检查：** 每个功能至少有一个 valid、一个 boundary、一个 corruption/negative fixture；每个写路径都有 e2fsck；每个 journal 路径都有 crash/replay。

### 失败处理和回滚

- 编译错误：只在当前任务分支修复声明/include/类型，不修改后续任务文件；重复运行该任务最小编译命令。
- host sanitizer 错误：保留 ASAN/UBSAN 输出，定位到首个越界/未定义行为后增加针对性 fixture；不以关闭 sanitizer 作为修复。
- e2fsck 错误：保存 image、superblock dump、journal dump、debugfs stat 和命令输出；优先回滚最近一个写路径 commit，禁止在测试脚本中忽略 e2fsck 退出码。
- QEMU boot 回归：先把 `--root-fs ext2` 作为诊断启动参数验证 transport/VFS 是否仍可用，再修复 ext4 mount 或恢复默认值；不得删除 ext4 代码以绕过失败。
- 性能不达标：保留基线 JSON，逐一调整 named cache/readahead/reservation 常量；如果仍不达标，在报告中写明测量结果和瓶颈，不能移除 journal/checksum。
- 任务需要修改未列文件：停止并由主代理更新计划文件和依赖；低成本代理不能自行扩大范围。

### 最终验收命令集

主代理在任务 15 后必须按顺序执行：

```bash
git status --short
git log --oneline --decorate -20
git diff --check HEAD~15..HEAD
make -j"$(nproc)" image-vmdk
e2fsck -f -n build/images/root.ext4
python3 tools/tests/ext4_feature_matrix.py --check-schema
python3 tools/tests/ext4_interop_test.py --direction linux-to-reliefos --mode fixture
python3 tools/tests/ext4_interop_test.py --direction reliefos-to-linux --mode fixture
python3 tools/tests/ext4_crash_replay_test.py --mode host-fixture --iterations 32
python3 tools/test_storage_upstream_guest.py --filesystem ext4
python3 tools/test_storage_upstream_runtime.py --filesystem ext4
python3 tools/tests/ext4_performance.py --compare /tmp/ext2-perf.json /tmp/ext4-perf.json
python3 tools/test_ext2_cache.py
python3 tools/test_storage_sync.py
python3 tools/test_tmpfs.py
```

验收必须同时给出：通过/失败命令、实际 feature mask、e2fsck 版本、QEMU 版本、性能 JSON 路径、ext2 回归结果和明确列出的未实现 feature。不能只报告“编译成功”。

## 规格覆盖自检

- 磁盘格式、feature policy、checksum：任务 1–4。
- flex_bg、bitmap、64-bit allocation：任务 5。
- extents、indirect、hole、FIEMAP：任务 6 和 8。
- journal、ordered write、replay、fsync：任务 7、8、15。
- 目录、HTREE、link/rename：任务 9。
- VFS/syscall compatibility 和 ext2 fallback：任务 10、11。
- xattr/statfs/statx/fallocate 和明确拒绝矩阵：任务 11。
- 镜像工具、rootfs、installer、BusyBox：任务 12、13。
- Linux 双向互操作、crash replay、性能：任务 14、15。
- ext2/FAT32/exFAT/ISO/TMPFS 回归：任务 10、14、15。

## 计划自检

逐节检查计划中是否出现未定义的任务、文件、函数名、错误码或测试命令；对每个规格章节确认至少有一个任务和至少一个负向测试。执行 Agent 在提交前运行 `git diff --check`，并使用 `rg` 检查禁止的占位式措辞；预期没有占位文本、未闭合代码块或空任务步骤。

## 执行交接

计划已完成并保存到 `docs/superpowers/plans/2026-09-28-reliefos-ext4.md`。两种执行方式：

1. **子代理驱动（推荐）**：使用 `subagent-driven-development`，按依赖 DAG 为每个任务调度低成本 Agent，每个任务完成后由主代理做接口、磁盘一致性和测试审查。
2. **内联执行**：使用 `executing-plans`，在当前会话按批次执行任务，并在任务 4、8、11、15 设置审查检查点。
