/* Legacy leonos_gpu_* compatibility entry points. All requests target the
 * /dev/gpu device node; fd 3 is no longer special. */
#include <reliefos/device.h>
#include <reliefos/gpu.h>
#include <reliefos/syscall.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int gpu_fd(void)
{
    static int fd = -1;
    if (fd < 0) fd = open(RELIEFOS_DEV_GPU, RELIEFOS_O_RDWR, 0);
    return fd;
}

static int gpu_ioctl(unsigned long request, void *arg)
{
    long result = syscall3(SYS_ioctl, gpu_fd(), (long)request, (long)arg);
    if (result < 0) { errno = (int)-result; return -1; }
    return (int)result;
}

int reliefos_gpu_diagnostics(struct reliefos_gpu_diagnostics *diagnostic)
{
    if (!diagnostic) return -22;
    diagnostic->size = sizeof(*diagnostic);
    diagnostic->version = RELIEFOS_GPU_ABI_VERSION;
    return gpu_ioctl(RELIEFOS_IOCTL_GPU_DIAGNOSTICS, diagnostic);
}

int reliefos_gpu_info(struct reliefos_gpu_info *info)
{
    if (!info) return -22;
    info->size = sizeof(*info);
    info->version = RELIEFOS_GPU_ABI_VERSION;
    return gpu_ioctl(RELIEFOS_IOCTL_GPU_INFO, info);
}

int reliefos_gpu_create(struct reliefos_gpu_context *context)
{
    if (!context) return -22;
    context->size = sizeof(*context);
    context->version = RELIEFOS_GPU_ABI_VERSION;
    return gpu_ioctl(RELIEFOS_IOCTL_GPU_CREATE, context);
}

int reliefos_gpu_render(const struct reliefos_gpu_frame *frame)
{
    struct reliefos_gpu_frame request;
    if (!frame) return -22;
    request = *frame;
    request.size = sizeof(request);
    request.version = RELIEFOS_GPU_ABI_VERSION;
    return gpu_ioctl(RELIEFOS_IOCTL_GPU_RENDER, &request);
}

int reliefos_gpu_destroy(uint64_t handle)
{
    struct reliefos_gpu_destroy request = {sizeof(request), RELIEFOS_GPU_ABI_VERSION, handle};
    return gpu_ioctl(RELIEFOS_IOCTL_GPU_DESTROY, &request);
}
/* Published libleonos.so.2 aliases; keep these in the defining translation unit. */
extern __typeof__(reliefos_gpu_create) leonos_gpu_create __attribute__((alias("reliefos_gpu_create")));
extern __typeof__(reliefos_gpu_destroy) leonos_gpu_destroy __attribute__((alias("reliefos_gpu_destroy")));
extern __typeof__(reliefos_gpu_diagnostics) leonos_gpu_diagnostics __attribute__((alias("reliefos_gpu_diagnostics")));
extern __typeof__(reliefos_gpu_info) leonos_gpu_info __attribute__((alias("reliefos_gpu_info")));
extern __typeof__(reliefos_gpu_render) leonos_gpu_render __attribute__((alias("reliefos_gpu_render")));
