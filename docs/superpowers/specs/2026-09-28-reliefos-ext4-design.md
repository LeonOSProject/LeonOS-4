# ReliefOS ext4 基本特性设计规格

## 1. 目标与边界

本项目为 ReliefOS 增加原生 ext4 文件系统后端，目标是让 Linux 生成的常规 ext4 镜像可以在 ReliefOS 上挂载、读取、创建、修改、删除、重命名、同步和卸载，并让 ReliefOS 生成的 ext4 镜像可以被 Linux 内核和 e2fsprogs 检查与挂载。驱动必须保持当前块设备访问路径的安全边界，并在固定内存预算下提供可预测的读写性能。

本次交付定义为“ext4 基本特性”，不是把 Linux `fs/ext4` 目录逐文件复制进 ReliefOS。实现依据只包括当前 `kernel/reliefnt` 源码、当前仓库中的存储接口、参考树 `/home/xiaobai/Projects/Projects/ReliefOS/linux`（Linux v7.3-rc5）及其 `fs/ext4` 和 `Documentation/filesystems/ext4` 内容。历史提交中已经废弃的 ext4 文档不属于需求、设计依据或测试依据。

本次必须支持：

1. ext2、ext3、ext4 超级块的识别，并将没有 extents 的旧 ext2/ext3 镜像通过同一 ext-family 后端兼容。
2. Linux 常规 `mke2fs`/`mkfs.ext4` 产生的 1 KiB、2 KiB、4 KiB block size 镜像；分区起始 LBA 不要求 4 KiB 对齐，但块设备读写必须正确处理 512-byte sector 边界。
3. extents（包括深度大于 0 的 extent tree）、64-bit block group descriptor、flex_bg、sparse_super/sparse_super2、uninit_bg、metadata_csum、gdt_csum、inode checksum、journal checksum、large_file/huge_file、extra_isize、dir_nlink 和 dir_index/HTREE。
4. ext4 journal 的检查、挂载时恢复、元数据事务、ordered 写入顺序、显式 `fsync`/`sync` 提交和干净卸载。首次交付只承诺 Linux ext4 常规的 ordered journal 模式，不承诺 data=journal、data=writeback、fast_commit 或 journal-less 强制切换。
5. 常规 VFS 操作：路径查找、目录枚举、regular file 读写、创建、mkdir、unlink、rmdir、rename、truncate、符号链接、硬链接、权限位、所有者、时间戳、文件大小、inode flags 的安全子集和挂载点根目录。
6. Linux 互操作所需的 xattr 基础存取、`security.*`/`user.*` 名称空间的拒绝或透传边界、`statfs`、`statx` 所需的基本字段、FIEMAP 所需的 extent 查询、`fallocate` 的 `KEEP_SIZE`、`PUNCH_HOLE`、`ZERO_RANGE` 基本模式。
7. 由现有镜像工具生成 ext4 root 镜像，并提供 Linux `e2fsck`、`debugfs`、`mount -t ext4` 可验证的输出。

本次明确不阻塞基本 ext4 交付的特性：bigalloc、inline_data、casefold、fscrypt/encryption、fs-verity、project quota、传统 quota、DAX、direct-I/O、iomap、atomic write、snapshot、metadata-only journal mode、data=journal、data=writeback、fast_commit、MMP、多设备 RAID、在线 resize 和 journal external device。对这些特性，驱动必须在挂载阶段识别 feature bit；如果它们影响 on-disk 解释，返回稳定的“不支持”错误并保持卷未修改；如果只是可选 VFS API，则返回 `-EOPNOTSUPP`。禁止忽略未知 `INCOMPAT` 位或以 ext2 逻辑强行写入 ext4 镜像。

## 2. 现状约束

当前存储实现由 `kernel/reliefnt/drivers/bootstrap/storage.c` 单一翻译单元按顺序 include 多个 `storage/*.c` 文件。`storage_internal.h` 中的 `storage_volume`、`ext2_superblock`、`ext2_group_desc`、`ext2_inode` 和 `ext2_dirent` 仍然以 ext2 为中心；`storage_mount.c` 在 GPT 根分区探测时调用 `ext2_mount()`；`storage_vfs.c` 的 lookup/read/write/readdir/mutate 路径直接判断 `STORAGE_FILESYSTEM_EXT2` 并调用 `ext2_*` 函数。AHCI、IDE PIO 和 NVMe 共享现有块传输接口，当前 AHCI 命令和若干缓存使用固定的全局内存；VFS 外层还使用执行锁保护活动卷和路径缓存。

因此本设计不引入第二套块设备驱动，不复制 FAT/exFAT 的路径逻辑，也不假设 Linux 的 buffer_head、page cache、iomap、workqueue、keyring 或 LSM 已经存在。ext4 后端通过现有 `storage_volume` 和块读写原语工作，并把需要的新状态限制在每卷的 ext-family 状态和固定大小缓存中。单一翻译单元的 include 方式暂时保留，但每个新 `.c` 文件只承担一个可测试职责。

## 3. 总体架构

### 3.1 ext-family 识别与公开分类

增加 `STORAGE_FILESYSTEM_EXT4`，同时保留 `STORAGE_FILESYSTEM_EXT2` 作为旧接口和日志兼容值。挂载解析器统一读取 ext superblock：

```text
ext2/ext3/ext4 magic
  -> superblock geometry and feature flags
  -> feature policy (rw / ro / reject)
  -> descriptor/checksum validation
  -> journal discovery and recovery
  -> ext4 volume state
  -> VFS mount
```

`storage_filesystem_is_ext_family()` 是 VFS 中唯一允许判断 ext2/ext4 的帮助函数。新代码不得在每个系统调用分支重新复制 `filesystem == STORAGE_FILESYSTEM_EXT2` 条件。现有 `ext2_*` 符号在过渡期间保留为兼容 wrapper，实际实现由 `storage_ext4_*` 完成；当所有调用点迁移后再删除 wrapper。

### 3.2 每卷状态

在 `storage_internal.h` 中增加 `storage_ext4_state`，由 `storage_volume` 持有指针或内嵌实例。状态必须包括：

- 分区起始 LBA、分区扇区数、block size、cluster/readahead size、blocks/inodes per group、first data block、group count、descriptor size、64-bit block count。
- `feature_compat`、`feature_incompat`、`feature_ro_compat` 原始值和经过策略检查的 capability bit；挂载模式和只读原因。
- UUID、volume label、journal inode/起始块/长度、journal sequence、最后恢复结果、下一次 checksum seed。
- 固定大小的 group descriptor cache、inode cache、extent path cache、extent status cache、directory block/HTREE cache、journal metadata cache；每个 cache 必须含 volume id、block number、generation/sequence、dirty 状态。
- 分配器的每组 free block/free inode 摘要、最近 goal block、预留窗口和 dirty bitmap block 集合。
- 每个挂载的统计计数器：读写字节、extent 命中/未命中、目录索引命中、journal transaction/commit/replay、checksum failure、回收次数和拒绝的 feature。

禁止使用依赖镜像大小线性增长的数组保存所有 inode、block 或目录项；所有缓存容量由常量限定，并在容量不足时采用明确的 LRU/clock 淘汰。

### 3.3 层次与职责

1. **块 I/O 层**：复用现有 AHCI/IDE/NVMe 接口，提供对 ext4 block 的 sector-split read/write、对齐检查、短读重试、错误转换和 barrier/flush hook。
2. **格式层**：解析 superblock、group descriptor、inode、extent header、directory entry、xattr header 和 journal block；所有 packed on-disk 结构通过 little-endian accessor 读取，禁止直接依赖宿主机结构布局。
3. **校验层**：实现 CRC16/CRC32C、superblock checksum、group descriptor checksum、inode checksum、extent tree checksum、directory checksum 和 journal checksum；校验失败必须在返回前标记卷错误。
4. **缓存层**：缓存 inode/extent/directory/group/journal block，提供 pin/dirty/writeback 和无效化接口；路径 cache 的 key 必须包含 volume id、mount generation 和 inode generation。
5. **分配层**：按 flex group 和 goal block 选择块，优先使用连续 extent；按 inode table/bitmap 分配 inode；维护 free count 并把 bitmap、descriptor、inode 更新纳入 journal。
6. **日志层**：实现 JBD2 兼容的 descriptor、data、commit、revoke、checksum、transaction、replay；本次只实现 ordered metadata journal，但保留 data mode 枚举和拒绝路径。
7. **inode/extent/dir 层**：完成 inode 读取/写回、extent 查找和插入、旧 indirect block 兼容、目录线性扫描和 HTREE 查找/插入/删除。
8. **VFS 适配层**：把当前 `storage_vfs.c` 的 ext2 分支迁移到统一 ext-family ops；保持现有 syscall 返回值、`storage_node` 句柄和路径 cache 行为。
9. **工具和测试层**：新增 ext4 root image 生成、fstab、mkfs/fsck 选择和 host/QEMU/interop/performance 测试；现有 ext2 测试继续验证旧镜像兼容。

## 4. 挂载、错误和一致性策略

挂载顺序固定为：读取并验证 superblock -> 验证 block geometry 和分区边界 -> 解析 feature policy -> 读取并校验 group descriptors -> 建立 journal -> replay 未完成事务 -> 建立缓存和根 inode -> 设置 ready。任何一步失败都必须释放已分配的 per-volume state，恢复原 active volume，并让根分区探测继续尝试下一个候选或返回明确错误。

feature policy 必须按 Linux 的 compat/incompat/ro_compat 语义处理：

- 已知且本次实现的 `INCOMPAT` 位允许读写。
- 未知或本次不实现但会改变 block/inode 解释的 `INCOMPAT` 位拒绝挂载，返回 `-EOPNOTSUPP`，日志打印 bit mask。
- 已知但本次只读安全的 `RO_COMPAT` 位允许只读挂载并记录原因；需要写入的 syscall返回 `-EROFS`。
- 未知 `RO_COMPAT` 位默认拒绝读写挂载，避免产生 Linux 不可恢复的元数据。
- `COMPAT` 位可以忽略但必须保留原值，除非对应 feature 的实现需要修改它。

所有元数据变更先建立 journal handle，再修改内存中的 cache block；事务提交顺序为 data blocks -> descriptor/bitmap/inode/directory metadata -> commit block -> flush/barrier。`fsync` 必须提交包含目标 inode 和其已分配 data block 的事务；`sync` 提交所有 dirty transaction。日志空间不足时，先提交最旧事务再重试，禁止覆盖未提交日志。

恢复阶段只接受 checksum、sequence、block number 和 revoke table 全部有效的事务；检测到不可恢复的 journal 损坏时以只读错误结束，不清理或覆盖 journal。正常卸载写入 clean state，并在 journal 已清空后更新 superblock checksum。

## 5. 性能设计与量化门槛

性能优化必须建立在正确的事务和边界检查之上。第一版采用固定内存和可测量的优化：

- 连续文件读取按 extent 批量读取，最小 I/O 单位为一个 ext4 block，跨 sector 时只拆分在块 I/O 层；顺序读取启用有界 readahead，随机读取不触发整文件扫描。
- extent path 和 extent status cache 使用 inode generation + logical block range 作为 key；缓存命中不能跳过 checksum/边界验证。
- 目录查找优先 HTREE，未建立索引或索引校验失败时退回有界线性扫描；目录块缓存独立于文件数据缓存。
- 分配器使用 goal block、flex group free count、连续空闲窗口和预留窗口，避免每次从 group 0 扫描；bitmap 更新按事务批量写回。
- 单个系统调用内合并相邻 metadata 更新；`fsync` 只提交依赖该 inode 的事务，不强制全卷扫描。
- 复用现有 transport lock，但不在 journal replay、bitmap 扫描或路径 cache 计算时持有；任何可睡眠 I/O 前释放纯计算锁。
- 所有 cache 大小、readahead 大小、journal transaction 上限和超时都有常量，测试可通过编译宏覆盖。

验收门槛以同一机器、同一磁盘镜像和同一编译配置下的现有 ext2 基线为参照：

1. 顺序读吞吐不低于 ext2 基线的 80%，随机 4 KiB 读 IOPS 不低于 70%。
2. 批量创建 10,000 个小文件的总时间不高于 ext2 基线的 2 倍；启用 dir_index 后不得比线性目录扫描基线更慢。
3. 1,000 次单文件 `fsync` 的 p95 延迟必须有记录，且不得出现无界忙等；journal 空间不足时测试必须完成而不是死循环。
4. 运行 ASAN/UBSAN host tests、e2fsck、QEMU boot 和 crash-replay 测试时不得出现 checksum mismatch、越界、double allocation 或 lost update。

如果硬件或 QEMU 传输性能使绝对吞吐不可比较，测试输出必须同时记录 block size、镜像大小、transport、CPU 频率、cache 配置和 ext2/ext4 同场基线，不能只报告单一速度数字。

## 6. 测试和互操作矩阵

### 6.1 单元和 host 集成测试

- superblock/feature parser：覆盖 1/2/4 KiB、inode size 128/256/扩展 inode、未知 feature、分区边界和 checksum 错误。
- extent tree：覆盖 inline extent、二级/三级 index、hole、边界 block、insert/merge/split/remove 和最大 logical block。
- descriptor/bitmap allocator：覆盖 flex_bg、64-bit descriptor、uninit bitmap、连续分配、回收、跨 group 和 free count 更新。
- journal：覆盖 descriptor/commit/revoke checksum、事务提交、空间回收、重放、重复重放、截断事务和损坏日志。
- directory/HTREE：覆盖长文件名、哈希冲突、目录扩展、删除后复用、`.`/`..`、目录 checksum 和 fallback。
- VFS mutation：覆盖并发 lookup/read/write 与单线程 mutation 的锁顺序、truncate 后读、rename 原子性和 fsync 后重启。

host 测试使用仓库已有 C fixture 编译方式（C11、ASAN、UBSAN、section GC、现有 include path），新增 `tools/tests/ext4_*.c` 和 Python 驱动；不依赖 root 权限即可运行的测试使用 loop image 和 e2fsprogs，必须 root 的 Linux mount 测试单独标记。

### 6.2 Linux 互操作测试

至少生成以下镜像组合：

```bash
mke2fs -t ext4 -F -b 4096 -I 256 -O extents,dir_index,metadata_csum,64bit,flex_bg,has_journal image.ext4
mke2fs -t ext4 -F -b 4096 -I 256 -O ^64bit,^metadata_csum image.ext4
mke2fs -t ext3 -F -b 4096 -I 256 image.ext3
mke2fs -t ext2 -F -b 4096 -I 128 -O none,filetype image.ext2
```

Linux 侧向镜像写入随机目录树、稀疏文件、硬链接、符号链接、长文件名、xattr 和大文件，再由 ReliefOS 读取并校验 sha256、stat、目录项和 extent 映射。ReliefOS 侧创建和修改同样的数据集，卸载后运行 `e2fsck -f -n`、`debugfs -R 'stat <ino>'` 和 Linux loop mount 读取。任何失败都保存 superblock、journal、出错 inode/block、QEMU console 和 e2fsck 输出。

### 6.3 QEMU 和崩溃测试

每个 rootfs 变体都运行现有 ReliefOS QEMU boot harness。测试在 journal transaction 的 descriptor、data、commit、checkpoint 四个窗口注入 reset/kill，重启后检查：文件要么保持旧版本，要么完整变成新版本；目录不会出现半个 entry；free block/inode count 与 e2fsck 结果一致。测试不得把“系统没有崩溃”当作成功条件，必须检查镜像语义。

## 7. 工具链和产品集成

`tools/make_image.py`、`tools/make_ext2_root.py`、`mk/images.mk`、root fstab 生成、installer root 生成、BusyBox/block storage 命令和 `docs/FILESYSTEM.md` 需要统一切换到 `ext4` 默认，同时保留显式 `--root-fs ext2` 作为兼容测试入口。镜像工具必须固定 `-I 256`、4 KiB block、Linux 可识别的 feature profile、root owner、时间戳和容量参数，并在输出阶段运行 `e2fsck -f -n`。

生成器不能把 `mkfs.ext4` 的默认 feature 当作稳定 ABI；命令行必须显式传入本项目支持的 feature 集，生成后读取 superblock 验证 feature mask。工具必须在缺少 `mke2fs` 或 `e2fsck` 时给出可执行安装提示并退出非零，而不是输出伪造镜像。

## 8. 迁移与回滚

迁移分为可独立回滚的阶段：格式/校验纯函数、只读挂载和 extent 读取、目录/HTREE、分配与 inode 写入、journal/recovery、VFS mutation、工具/默认 rootfs、性能优化。每阶段结束必须有独立 host/QEMU 测试和一个小提交。旧 ext2 代码在 ext4 backend 通过全部 ext2 compatibility tests 前不删除；出现回归时可以把 rootfs 默认值恢复为 ext2，而不回滚 block transport 或 VFS API。

禁止在单个提交中同时改动磁盘格式、transport 和所有 syscall 分支。任何变更 `storage_volume` 布局的提交必须同时更新 volume 初始化、mount copy、volume reset、node handle 序列化和测试 fixture。

## 9. 完成定义

只有同时满足以下条件才算本次 ext4 基本特性交付完成：

1. 支持矩阵中标为“本次必须支持”的 feature 在 Linux 生成的测试镜像上读写通过，且未知/未实现 feature 不会被静默忽略。
2. `e2fsck -f -n`、Linux loop mount、ReliefOS QEMU boot、host ASAN/UBSAN 和 crash-replay 全部通过。
3. ext2 现有测试和原有 FAT32/exFAT/ISO/TMPFS 测试无回归。
4. 性能门槛、测试环境和原始结果写入性能报告；不能只写“性能良好”。
5. 代码、工具、文档中的默认 rootfs、filesystem enum、错误码、feature mask 和测试命令一致，且 `git diff --check` 通过。
