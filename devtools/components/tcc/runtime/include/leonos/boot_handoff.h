#ifndef LEONOS_BOOT_HANDOFF_H
#define LEONOS_BOOT_HANDOFF_H

#include <stdint.h>

#define LEONOS_BOOT_HANDOFF_MAGIC 0x4c424f54u
/* Version 7 removes the middle-layer module range and the loader-side API
 * tables; a version-6 handoff describes a boot image this kernel cannot run,
 * so the mismatch is rejected rather than partially honoured. */
#define LEONOS_BOOT_HANDOFF_VERSION 7u

struct leonos_boot_module_info {
    uint64_t start;
    uint64_t end;
    uint64_t entry;
    const char *path;
};

struct leonos_boot_log_state {
    uint32_t log_x;
    uint32_t log_y;
    uint32_t columns;
    uint32_t rows;
    uint32_t column;
    uint32_t row;
    uint32_t line_count;
};

struct leonos_boot_handoff {
    uint32_t magic;
    uint32_t version;
    uint32_t multiboot_magic;
    uint32_t ui_theme;
    uint64_t multiboot_info;
    const char *cmdline;
    const char *bootloader;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t framebuffer_red_field_position;
    uint8_t framebuffer_red_mask_size;
    uint8_t framebuffer_green_field_position;
    uint8_t framebuffer_green_mask_size;
    uint8_t framebuffer_blue_field_position;
    uint8_t framebuffer_blue_mask_size;
    uint64_t mmap_addr;
    uint32_t mmap_entry_size;
    uint32_t mmap_entry_count;
    uint64_t efi_mmap_addr;
    uint32_t efi_mmap_entry_size;
    uint32_t efi_mmap_entry_count;
    uint64_t rsdp_addr;
    uint64_t efi_system_table;
    struct leonos_boot_module_info loader;
    struct leonos_boot_module_info kernel;
    /* The installer root is a raw Multiboot module, not an ELF image.  Keep
     * its range in the loader handoff so the kernel does not have to trust
     * module tags that may be overwritten while the second-stage images are
     * loaded. */
    struct leonos_boot_module_info installer_root;
    struct leonos_boot_log_state boot_log;
    uint64_t boot_uptime_us;
    /* Set only after the loader consumes a valid one-shot ESP marker. */
    uint32_t kernel_debug_mode;
    uint32_t kernel_debug_reserved;
};

#endif
