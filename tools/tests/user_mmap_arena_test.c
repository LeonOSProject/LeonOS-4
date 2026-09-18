#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <ntclks/paging.h>

/* Regression fixture for the Alpine Clang dynamic-link failure.
 *
 * musl's ldso reserves one whole PT_LOAD span per shared object with a single
 * mmap (map_library in third_party/musl/ldso/dynlink.c), so a program's
 * resident set of file mappings has to fit in the kernel's read-only file mmap
 * arena.  Clang's libLLVM.so.22.1 alone needs a 183 MiB span and its complete
 * DT_NEEDED closure needs 278 MiB.  Before NTCLKS_USER_TOP moved to 768 MiB the
 * arena was 255 MiB, mmap returned ENOMEM, and every LLVM symbol afterwards
 * failed to resolve.
 *
 * The arena derivation mirrors task_mmap_top() and the KERNEL_HOLE skip in
 * syscall_mm.c plus the USER_STACK_TOP definition in user/userland.c.
 */
#define MIB (1024ULL * 1024ULL)
#define PAGE_BYTES 4096ULL
/* Largest single shared-object span in the measured Clang closure. */
#define LARGEST_OBJECT_SPAN (183ULL * MIB)
/* Sum of the PT_LOAD spans of every library Clang keeps mapped. */
#define CLANG_CLOSURE_SPAN (278ULL * MIB)
/* Slab/malloc arenas and thread stacks share the arena with the closure. */
#define REQUIRED_HEADROOM (64ULL * MIB)

/* Mirrors task_mmap_top(): the first page below the native stack growth
 * window is the highest address the file-mmap scanner will consider. */
static uint64_t mmap_top(void)
{
    return NTCLKS_USER_TOP - PAGE_BYTES -
           (uint64_t)NTCLKS_USER_STACK_MAX_PAGES * PAGE_BYTES - PAGE_BYTES;
}

static uint64_t mmap_arena(void)
{
    return mmap_top() - NTCLKS_KERNEL_HOLE_END;
}

int main(void)
{
    const uint64_t arena = mmap_arena();

    if (NTCLKS_USER_PD_START + NTCLKS_USER_PD_COUNT > 512u) {
        printf("FAIL user window out of one page directory: %u entries\n",
               (unsigned)(NTCLKS_USER_PD_START + NTCLKS_USER_PD_COUNT));
        return 1;
    }
    if (arena < LARGEST_OBJECT_SPAN) {
        printf("FAIL mmap arena %" PRIu64 " KiB is smaller than the largest single shared object span\n",
               arena / 1024ULL);
        return 1;
    }
    if (arena < CLANG_CLOSURE_SPAN + REQUIRED_HEADROOM) {
        printf("FAIL mmap arena %" PRIu64 " MiB cannot hold the %" PRIu64 " MiB Clang closure plus %" PRIu64
               " MiB headroom\n",
               arena / MIB, (CLANG_CLOSURE_SPAN + REQUIRED_HEADROOM) / MIB, REQUIRED_HEADROOM / MIB);
        return 1;
    }
    printf("PASS mmap arena %" PRIu64 " MiB covers a %" PRIu64 " MiB shared-object span and a %" PRIu64
           " MiB dynamic closure\n",
           arena / MIB, LARGEST_OBJECT_SPAN / MIB, CLANG_CLOSURE_SPAN / MIB);
    return 0;
}
