# LeonOS EEVDF 调度

本实现参考本地 Linux 6.12 的 `kernel/sched/fair.c` 中 EEVDF 的调度规则，
以 LeonOS 的任务、锁和时钟接口独立实现，替换原来按严格优先级轮转的任务选择。
所有现有可调度用户任务，包括桌面和服务，使用相同的公平调度策略。
进程生命周期、信号、地址空间、上下文切换及 idle 机制仍由原有内核接口负责。

## 已实现的规则

- 根据实际运行时间和 Linux nice 权重累计虚拟运行时间；保留整数除法余数，
  避免频繁短系统调用在记账时丢失 CPU 用量。
- 计算运行队列加权平均虚拟时间，只有 `vruntime <= V` 的实体有资格运行，
  从中选择最早虚拟截止时间；时间片用完后续订请求。
- 系统调用、缺页和时钟调度入口都进入同一选择策略，不在每次系统调用时重置时间片。
- 初次加入使用半个请求长度；唤醒时保留有界 lag，并补偿新实体对平均值的影响。
- 睡眠时负 lag 实体延迟出队，待债务衰减后再移除，防止短暂睡眠重置欠账。
- `nice` 改变时按旧权重结算，再调整 lag 与剩余虚拟请求；`sched_yield` 放弃当前截止时间。
- `fork` 子进程重新初始化调度实体，不继承父实体的队列链接和运行记账。
- 每 CPU 公平域、亲和性约束、空闲 CPU 拉取工作，以及迁移时按 lag 放置。
  任务帧发布和运行 CPU 的所有权仍由 `scheduler_lock` 原子维护。

## 与 Linux 6.12 的差异

LeonOS 当前时钟抢占为 100 Hz，所以请求长度选用 10 ms；没有宣称实现 Linux
亚毫秒 hrtick 精度。队列使用链表扫描，选择复杂度 O(n)，未移植 Linux 的增强
红黑树、PELT、调度组、CPU capacity/NUMA、CFS bandwidth、实时调度类或
`RUN_TO_PARITY` 特性。现有 Linux ABI 对不支持的调度策略继续拒绝。

当前生产代码在 `arch/x86_64/smp.c` 设置 `SMP_USER_SCHEDULER_ENABLED=0`，
原因是此前 AP 用户态并发存在任务所有权和回收崩溃风险。本次保留该限制。
**配置 2/4 个虚拟 CPU 不等于 LeonOS 已启用 2/4 个调度 CPU。**
每 CPU 选择和迁移有确定性的主机测试，但本次来宾测试实际 active CPU 为 1，
不能据此声称 AP 并发已验证；启用 AP 需要单独完成上下文切换/回收同步审计。

## 验证

```sh
python3 tools/test_eevdf.py
python3 tools/test_linux_threads.py
python3 tools/test_linux_memory.py
python3 tools/test_linux_rootfs.py
python3 build.py run kernel
python3 tools/test_execution_lock_qemu.py --scheduler --out build/eevdf-scheduler-verified
python3 tools/test_chroot_qemu.py
python3 tools/test_uinxed_guest.py --source /path/to/clean/Uinxed-Kernel --out build/uinxed-eevdf
```

确定性测试运行真实 EEVDF 策略与 `sched.c` 的选择/帧发布路径，覆盖公平性、
nice 比例、资格与截止时间、睡眠欠账、唤醒、yield、动态改权重、多 CPU 所有权、
亲和性迁移、阻塞、工作窃取和退出回收。使用 ASan/UBSan 检查内存及整数错误。

来宾 CPU 密集测试把两个进程固定到同一实际 CPU，单轮竞争 4 秒：
同 nice 工作量比例为 0.953，nice 0:5 为 2.941（理论权重比约 3.057）；
二者都持续取得进展。连续 16 轮 fork/亲和性/yield/wait 检查通过。
短测量还包含桌面和系统服务负载，不把单次比例当作精确性能保证。

审查补充了已出队休眠任务修改 nice 的测试：保存的 lag 按旧/新权重换算，
修改优先级不会唤醒任务或凭空增加其服务补偿。策略与实际调度入口测试均通过。
最终内核 SHA-256 为 `d3e1fae6326a04763dc1479d7d0ffd770ee6e361f949d6d9fc3a9a94d3b137ae`，
与上述来宾复测加载的内核一致。`build/images/leonos4-installer.iso` 已重新生成。
此机器的默认 PATH 优先找到缺少 compiler-rt 的自定义 Clang，构建使用
`PATH=/usr/bin:/bin python3 build.py run installer-image`，结果为 0 errors。

Uinxed 提交 `a9786eb8e147389ba960810537a4e35c579dcc18` 在 LeonOS 来宾中执行
`make -j2 UxImage` 成功，耗时 1056.981 秒，产物为 `build/uinxed-eevdf/UxImage`
（9,807,888 字节），SHA-256 为
`ae044ccde9d4bec71292cded9e7d27409a0059b76e42a760cb796aca8c69e42e`。
构建使用的 EEVDF 内核哈希为
`cdd3bee2899ee6b9cea0a71036d168c3f650563e0c70b76404a0ea0e7ab6b663`，
早于审查补充的休眠改权重修正；未在最终内核上重复完整 Uinxed 构建。
最终内核另外通过公平性、内存/线程和 chroot/exec 来宾回归。
此前运行耗时 1335.853 秒，本次约减少 20.9%，但仅为单次观测，
两次均只有一个活跃调度 CPU，宿主负载及构建工具环境也未严格控制，不能归因为稳定调度提速。
