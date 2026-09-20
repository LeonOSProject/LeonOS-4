/*
 * LeonOS x86_64 kernel linker script: fixes the kernel image layout.
 * Places boot, text, read-only data, writable data, and relocation sections.
 */
ENTRY(kernel_entry)

SECTIONS
{
    . = 128M;

    .multiboot ALIGN(8) : {
        KEEP(*(.multiboot))
    }

    .text ALIGN(4K) : {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss .bss.*)
    }

    /* The image is linked at 128M and paging.h keeps the identity hole from
     * 128M to 192M supervisor-only; growing past it would let a user page-table
     * write replace kernel mappings. */
    ASSERT(. <= 192M, "Kernel overflows the supervisor identity hole")

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
    }
}
