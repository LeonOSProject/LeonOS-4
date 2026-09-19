# 镜像格式复现检查

同一固定 epoch 1700000000、相同 ESP 和 root fixture，连续生成两份产物。

- ext2：`tests/build/test-image-adapters.sh` 两次字节一致，目录权限、软链接、test 用户 1000:1000 验证通过；在断网 user namespace 中同样通过。
- GPT raw：两次 SHA-256 都为 `011abbcb71d5638a91985b8e93ca19dbb78233f074ed79a1272f81c75c717e10`。
- ISO：修复 Rock Ridge 目录 ctime/mtime 和 uid/gid 后，两次 SHA-256 都为 `9781b221d4bc3b1701f0e515273fbad0bb04c6fb69f4fcbdbf08e92c490e09de`。
- VMDK：仅偏移 549–556（1-based）的 8 字节 CID 不同。QEMU 的 monolithicSparse 输出会生成随机 CID，工具没有公开固定 CID 参数。本项目不手改镜像格式；`qemu-img compare -f vmdk -F vmdk a.vmdk b.vmdk` 返回 0，显示 Images are identical；内含 GPT/raw 字节一致。

证据产物位于 `out/repro-images/`。这是格式适配器的重复生成验证，不冒充两个干净全量 O 的全部产物比较。
