# 全局执行锁与匿名缺页并发

全局执行锁仍保护系统调用、文件缺页及依赖共享缓冲区的 VFS/设备操作。
2026-09-18 的优化增加了读侧事务，允许不同进程的私有匿名缺页同时处理，
并在修复后返回原指令，不再为每个匿名页强制切换任务。时钟抢占继续有效。

## 同步边界

- 写侧保留可重入的 FIFO ticket lock；写方排队后关闭新的读侧准入，
  等待已有读方退出再操作共享状态。
- 读侧使用“注册后再次检查”的原子协议，与写侧互斥。读侧不能嵌套、
  升级为写锁、访问用户指针或进入调度；失败时释放后走原写侧路径。
- 只有 Ring 3 缺页且 `task->shared_mm == NULL`，才可能在读侧映射页面。
  地址必须属于私有匿名 VMA，权限检查必须通过，且不存在已映射的用户页。
- 页表仅由当前任务修改；物理页分配仍由分配器自己的锁保护。
  写侧排除 `fork`、`exec`、`munmap` 和远程地址空间操作，确保 VMA/页表生命周期。
- `CLONE_VM`/`vfork`、共享映射、文件映射、COW、栈增长和异常信号保留原路径。
  内核态 usercopy 缺页也保留可重入写锁，避免读锁升级死锁。
- 两侧事务都关闭本 CPU 中断，不跨任务切换持锁。写侧等待仍轮询
  membarrier 请求，保持跨 CPU rendezvous 进度。

`read` 表示对全局服务状态的共享访问，不表示完全禁止写内存：读方只能
修改自己独占地址空间的页表，并通过已有同步的物理分配器获取页面。
这不是将所有系统调用改为并行，也没有删除文件系统保护。

## 验证与复现

```sh
python3 tools/test_execution_lock.py
python3 tools/test_linux_memory.py
python3 tools/test_linux_threads.py
python3 build.py run kernel
python3 tools/test_execution_lock_qemu.py --out build/execution-lock-after
python3 tools/test_uinxed_guest.py --source /path/to/clean/Uinxed-Kernel --out build/uinxed-execution-lock
```

锁测试使用真实 `lock.c`，仅将特权 IRQ 指令替换为线程本地状态，覆盖读方重叠、
写方排他、写方等待时禁止新增读方、写方递归与 IRQ 状态恢复。
内存测试使用真实页表及缺页实现，验证零填充、权限、共享状态回退和失败清理。
QEMU 测试校验独立进程及共享地址空间线程的内存内容，并记录独立进程缺页耗时。

首次前后对照使用 QEMU/KVM、2 vCPU、4096 MiB；每个进程分四轮触碰 8192 个
4 KiB 匿名页。单进程为 3.20 → 0.06 秒，双进程共 16384 页为 1.73 → 0.12 秒。
这是包含取消逐页强制调度在内的微基准结果，会受启动时后台负载影响；
不能据此推算完整 GCC 构建的提速比例，也没有单独归因读侧并发的收益。
每次运行的 `result.json` 保存内核 SHA-256；Uinxed 测试另外记录源码提交、
构建时间、产物大小和 SHA-256，并核对来宾与宿主接收的 `UxImage` 一致。

本次完整 `make -j2 UxImage` 在来宾中成功，构建耗时 1335.853 秒，产物
9,807,888 字节，SHA-256 为
`602f211dfc97c6d60ea2a59f5cf181c16a187b0cb470bf3fca1f9c05a252ea40`。
测试内核 SHA-256 为
`fdd4706381ae5f42521db4aa01239b1b7f91de6f2bd19fe918619b2a26b9a266`。
此前成功构建约 1468 秒，但本次后段同时生成安装 ISO，且旧记录计时范围不同，
所以不将两者视为严格的提速百分比测量。
