/* Device syscall category boundary for non-GUI ioctl operations. */
#include <ntclks/syscall_internal.h>
#include <ntclks/syscall.h>
#include <ntclks/usercopy.h>
#include <ntclks/driver_manager.h>
#include <linux/capability.h>
#include <linux/errno.h>

int syscall_device_owns(uint64_t number, uint64_t a1)
{
    (void)a1;
    return number == LINUX_SYS_IOCTL;
}

int64_t syscall_device_dispatch(uint64_t number, uint64_t a0, uint64_t a1,
                                uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
    return syscall_dispatch_regs_legacy(number, a0, a1, a2, a3, a4, a5);
}

/** @brief Authorize and execute one fixed-size driver request on /dev/driverctl. */
int64_t syscall_driver_control(uint32_t request_number, uint64_t address)
{
    struct task *task = sched_current_task();
    if (request_number != LEONOS_DRIVER_CONTROL_IOCTL) return -LINUX_ENOTTY;
    if (!task || !(task->cap_effective & (1ULL << CAP_SYS_MODULE))) return -LINUX_EPERM;
    struct leonos_driver_control request;
    if (!user_range_ok(address, sizeof(request))) return -LINUX_EFAULT;
    __builtin_memcpy(&request, (void *)(uintptr_t)address, sizeof(request));
    unsigned length = 0;
    while (length < sizeof(request.file) && request.file[length]) ++length;
    if (request.flags || request.reserved || length == sizeof(request.file)) return -LINUX_EINVAL;
    int ret = driver_manager_control(&request);
    if (!user_range_writable(address, sizeof(request))) return -LINUX_EFAULT;
    __builtin_memcpy((void *)(uintptr_t)address, &request, sizeof(request));
    return ret;
}
