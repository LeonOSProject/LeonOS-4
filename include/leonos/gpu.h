#ifndef LEONOS_GPU_H
#define LEONOS_GPU_H

/*
 * Userland GPU API. The wire types and constants moved to the kernel UAPI
 * (<leonos/gpu_abi.h>); this header re-exports them so existing
 * `#include <leonos/gpu.h>` callers keep working.
 */
#include <leonos/gpu_abi.h>
#include <stdint.h>

int leonos_gpu_diagnostics(struct leonos_gpu_diagnostics *diagnostics);
int leonos_gpu_info(struct leonos_gpu_info *info);
int leonos_gpu_create(struct leonos_gpu_context *context);
int leonos_gpu_render(const struct leonos_gpu_frame *frame);
int leonos_gpu_destroy(uint64_t handle);

#endif
