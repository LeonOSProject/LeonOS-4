#ifndef RELIEFOS_GPU_H
#define RELIEFOS_GPU_H

/*
 * Userland GPU API. The wire types and constants moved to the kernel UAPI
 * (<reliefos/gpu_abi.h>); this header re-exports them so existing
 * `#include <reliefos/gpu.h>` callers keep working.
 */
#include <reliefos/gpu_abi.h>
#include <stdint.h>

int reliefos_gpu_diagnostics(struct reliefos_gpu_diagnostics *diagnostics);
int reliefos_gpu_info(struct reliefos_gpu_info *info);
int reliefos_gpu_create(struct reliefos_gpu_context *context);
int reliefos_gpu_render(const struct reliefos_gpu_frame *frame);
int reliefos_gpu_destroy(uint64_t handle);

#endif
