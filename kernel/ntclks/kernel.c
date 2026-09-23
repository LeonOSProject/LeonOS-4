/*
 * LeonOS kernel bootstrap: coordinates early platform initialization.
 * Starts memory, interrupts, drivers, storage and scheduling.
 */
#include <leonos/boot_handoff.h>
#include <ntclks/arch.h>
#include <ntclks/apic.h>
#include <ntclks/console.h>
#include <ntclks/driver_manager.h>
#include <ntclks/framebuffer.h>
#include <ntclks/input.h>
#include <ntclks/kernel_debug.h>
#include <ntclks/kernel.h>
#include <ntclks/mm.h>
#include <ntclks/heap.h>
#include <ntclks/multiboot2.h>
#include <ntclks/net.h>
#include <ntclks/platform.h>
#include <ntclks/power.h>
#include <ntclks/pty.h>
#include <ntclks/page_cache.h>
#include <ntclks/object.h>
#include <ntclks/sched.h>
#include <ntclks/smp.h>
#include <ntclks/storage.h>
#include <ntclks/syscall.h>
#include <ntclks/svga.h>
#include <ntclks/time.h>
#include <ntclks/usb.h>
#include <ntclks/userland.h>
#include <ntclks/version.h>

#include "arch/x86_64/idt.h"

static uint8_t kernel_ring0_stack[65536] __attribute__((aligned(16)));

static bool boot_handoff_is_current(const struct leonos_boot_handoff *handoff)
{
    return handoff && handoff->magic == LEONOS_BOOT_HANDOFF_MAGIC &&
           handoff->version == LEONOS_BOOT_HANDOFF_VERSION;
}

/**
 * @brief Halt the CPU forever; used when there is nothing left to run.
 */
void kernel_idle_loop(void)
{
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/**
 * @brief Return 1 if the boot command line contains the substring needle.
 */
static int cmdline_has(const struct boot_info *boot, const char *needle)
{
    if (!boot || !boot->cmdline || !needle) {
        return 0;
    }
    for (const char *p = boot->cmdline; *p; ++p) {
        const char *a = p;
        const char *b = needle;
        while (*a && *b && *a == *b) {
            ++a;
            ++b;
        }
        if (*b == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Return 1 if the two NUL-terminated strings are identical.
 */
static int boot_text_eq(const char *a, const char *b)
{
    if (!a || !b) {
        return 0;
    }
    while (*a && *b && *a == *b) {
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

/**
 * @brief Hold the boot console until Enter is pressed after discarding queued input.
 * Called with interrupts disabled after IRQ/USB initialization and before user
 * tasks or APs are started. Keyboard and timer IRQs run during each halt; returns
 * with interrupts disabled. Pointer events and key releases do not resume boot.
 */
static void boot_log_wait_for_enter(void)
{
    struct input_raw_event event;
    while (input_pop(&event)) {
    }
    console_printf("[boot] Boot log paused. Press Enter to continue.\n");
    for (;;) {
        while (input_pop(&event)) {
            if (event.type == INPUT_EVENT_KEYBOARD && event.pressed &&
                event.keycode == 28u) {
                return;
            }
        }
        __asm__ volatile("sti; hlt; cli" ::: "memory");
    }
}

/**
 * @brief Replace or append the installer-root module in boot from the loader handoff.
 */
static void boot_import_handoff_modules(struct boot_info *boot,
                                        const struct leonos_boot_handoff *handoff)
{
    if (!boot || !boot_handoff_is_current(handoff) ||
        handoff->installer_root.end <= handoff->installer_root.start) {
        return;
    }

    for (uint32_t i = 0; i < boot->module_count && i < 16; ++i) {
        if (boot_text_eq(boot->modules[i].name, "leonos-installer-root")) {
            boot->modules[i].start = handoff->installer_root.start;
            boot->modules[i].end = handoff->installer_root.end;
            return;
        }
    }

    if (boot->module_count < 16) {
        struct boot_module *module = &boot->modules[boot->module_count++];
        module->start = handoff->installer_root.start;
        module->end = handoff->installer_root.end;
        module->name = handoff->installer_root.path;
        console_printf("[ntclks] imported installer root module start=%p bytes=%llu\n",
                       (void *)(uintptr_t)module->start,
                       (unsigned long long)(module->end - module->start));
    }
}

/**
 * @brief Full boot sequence: parse Multiboot2/EFI info, then initialize every kernel subsystem in order and enter userland.
 */
static void kernel_start(uint32_t magic, uint32_t multiboot_info,
                         const struct leonos_boot_handoff *handoff)
{
    bool boot_log_pause;

    __asm__ volatile("cli");
    console_init();
    console_set_boot_uptime_us(handoff && handoff->magic == LEONOS_BOOT_HANDOFF_MAGIC
                                   ? handoff->boot_uptime_us
                                   : 0ULL);
    const struct leonos_system_info *system = ntclks_system_info();
    console_printf("LeonOS 4 %s %s booting\n",
                   system->kernel_name,
                   system->kernel_version);
    if (handoff && handoff->magic == LEONOS_BOOT_HANDOFF_MAGIC) {
        console_printf("[ntclks] loader handoff kernel=%p-%p\n",
                       (void *)(uintptr_t)handoff->kernel.start,
                       (void *)(uintptr_t)handoff->kernel.end);
    }

    struct boot_info boot;
    multiboot2_parse(magic, (uintptr_t)multiboot_info, &boot);
    /**
 * @brief UEFI GRUB keeps boot services active for the second-stage loader and may therefore omit Multiboot2 memory-map tags. The loader captures a stable EFI map after loading all images; use it before the allocator falls back to the legacy 512 MiB estimate.
 */
    if (!boot.mmap_entry_count && !boot.efi_mmap_entry_count &&
        handoff && handoff->magic == LEONOS_BOOT_HANDOFF_MAGIC &&
        handoff->efi_mmap_addr && handoff->efi_mmap_entry_count) {
        boot.efi_mmap_addr = handoff->efi_mmap_addr;
        boot.efi_mmap_entry_size = handoff->efi_mmap_entry_size;
        boot.efi_mmap_entry_count = handoff->efi_mmap_entry_count;
        console_printf("[ntclks] using loader-captured EFI memory map entries=%u descriptor=%u\n",
                       boot.efi_mmap_entry_count, boot.efi_mmap_entry_size);
    }
    boot_log_pause = cmdline_has(&boot, "bootlog-pause=1");
    if (!boot.rsdp_addr && handoff && handoff->magic == LEONOS_BOOT_HANDOFF_MAGIC) {
        boot.rsdp_addr = handoff->rsdp_addr;
    }
    /**
 * @brief Parse ACPI before the physical allocator can reclaim ACPI memory.
 */
    power_init(&boot);
    boot_import_handoff_modules(&boot, handoff);
    platform_identity_init(&boot);

    arch_init();
    apic_init();
    ioapic_init();
    smp_init();
    framebuffer_init(&boot);
    mm_init(&boot, handoff);
    kernel_heap_init();
    page_cache_init();
    kernel_objects_init();
    time_init();
    input_init();
    pty_init();
    console_set_ui_theme(handoff && handoff->magic == LEONOS_BOOT_HANDOFF_MAGIC
                              ? handoff->ui_theme
                              : 1u);
    console_enable_framebuffer(handoff && handoff->magic == LEONOS_BOOT_HANDOFF_MAGIC
                                   ? &handoff->boot_log
                                   : 0);
    console_enable_vga_fallback();
    sched_init();
    sched_create_idle_task();
    syscall_init();
    syscall_trace_configure(boot.cmdline);
    arch_userland_init(kernel_ring0_stack + sizeof(kernel_ring0_stack));
    /* The bootstrap page tables are complete now, so SVGA BARs can be marked
     * UC before any 3D FIFO or guest-memory command is issued. */
    int svga_ret = svga3d_init();
    struct svga_info boot_svga_info;
    svga_get_info(&boot_svga_info);
    int triangle_ret = svga_ret;
    if (cmdline_has(&boot, "svga3d-triangle=1")) {
        triangle_ret = svga_ret == 0 ? svga3d_triangle_test() : svga_ret;
        console_printf("[svga3d] triangle-test=%d\n", triangle_ret);
    }
    idt_init();
    irq_init();
    {
        bool ramdisk_root = cmdline_has(&boot, "mode=installer") ||
                            cmdline_has(&boot, "mode=live");
        storage_mount_boot_root(&boot, ramdisk_root);
        if (ramdisk_root && !storage_ready()) {
            console_printf("[ntclks] installer ramdisk root not ready, retrying handoff module\n");
            storage_init_installer_root(&boot);
        }
    }
    driver_manager_init();
    driver_manager_autoload();
    usb_init();
    net_init();
    if (kernel_debug_boot_requested(handoff)) {
        console_printf("[ntclks] entering kernel debug tool before userland\n");
        (void)kernel_debug_run_module();
        console_printf("[ntclks] kernel debug tool finished; continuing normal startup\n");
    }
    if (boot_log_pause) {
        console_printf("[svga3d] init=%d available=%u fifo-ready=%u\n",
                       svga_ret, (unsigned)boot_svga_info.available,
                       (unsigned)boot_svga_info.fifo_ready);
        console_printf("[svga3d] caps=0x%x fifo=0x%x\n",
                       boot_svga_info.device_caps, boot_svga_info.fifo_caps);
        console_printf("[svga3d] host=0x%x guest=0x%x\n",
                       boot_svga_info.host_version, boot_svga_info.guest_version);
        const struct svga_probe_info *probe = &boot_svga_info.probe;
        console_printf("[svga3d] probe=%s status=%d\n",
                       probe->stage ? probe->stage : "not-run", probe->status);
        console_printf("[svga3d] probe-fifo=0x%x min=%u mem-regs=%u\n",
                       probe->fifo_caps, probe->fifo_min, probe->mem_regs);
        console_printf("[svga3d] raw-hw=0x%x revised=0x%x enable=0x%x\n",
                       probe->host_legacy, probe->host_revised, probe->enable);
        console_printf("[svga3d] gb=%u devcap3d=%u\n",
                       (unsigned)probe->gb_objects, probe->devcap_3d);
        if (cmdline_has(&boot, "svga3d-triangle=1")) {
            console_printf("[svga3d] triangle-test=%d\n", triangle_ret);
        }
        boot_log_wait_for_enter();
    }
    if (pty_vt_init() < 0) {
        console_printf("[ntclks] unable to initialize six virtual terminals\n");
        kernel_idle_loop();
    }
    userland_init(&boot);
    /* All initial task objects are now present. APs may enter the shared
     * scheduler without racing the bootstrap task construction above. */
    smp_start_aps();
    sched_dump();
    console_printf("[ntclks] boot complete: version=%s root=/ fs=%s vt=tty1\n",
                   system->kernel_version, storage_root_filesystem_name());

    userland_enter_first();
    kernel_idle_loop();
}

/**
 * @brief Validate the loader handoff and start the kernel, or idle-loop on a mismatch.
 */
void kernel_entry(const struct leonos_boot_handoff *handoff)
{
    if (!boot_handoff_is_current(handoff)) {
        console_init();
        console_printf("[ntclks] rejected loader handoff abi=%u expected=%u\n",
                       handoff ? handoff->version : 0u,
                       LEONOS_BOOT_HANDOFF_VERSION);
        kernel_idle_loop();
    }
    kernel_start(handoff->multiboot_magic, (uint32_t)handoff->multiboot_info, handoff);
}

/**
 * @brief Legacy Multiboot2 entry point; starts the kernel without a loader handoff.
 */
void kernel_main(uint32_t magic, uint32_t multiboot_info)
{
    kernel_start(magic, multiboot_info, 0);
}
