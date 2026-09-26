#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../kernel/ntclks/kernel/ntclks/user/elf.c"

int main(void)
{
    unsigned char image[4096] = {0};
    struct elf64_ehdr *eh = (void *)image;
    memcpy(eh->e_ident, "\177ELF\2\1\1", 7);
    eh->e_type = ET_EXEC;
    eh->e_machine = EM_X86_64;
    eh->e_version = 1;
    eh->e_entry = 0x400300;
    eh->e_ehsize = sizeof(*eh);
    eh->e_phoff = sizeof(*eh);
    eh->e_phentsize = sizeof(struct elf64_phdr);
    eh->e_phnum = 3;
    struct elf64_phdr *ph = (void *)(image + eh->e_phoff);
    ph[0] = (struct elf64_phdr){.p_type = PT_LOAD, .p_flags = 4u | PF_X,
        .p_vaddr = 0x400000, .p_filesz = sizeof(image), .p_memsz = sizeof(image), .p_align = 4096};
    const char path[] = "/lib/ld-musl-x86_64.so.1";
    memcpy(image + 512, path, sizeof(path));
    ph[1] = (struct elf64_phdr){.p_type = PT_INTERP, .p_offset = 512, .p_filesz = sizeof(path)};
    ph[2].p_type = PT_DYNAMIC;
    struct elf_image_info info;
    assert(elf64_probe(image, sizeof(image), &info));
    assert(!info.dynamic && !strcmp(info.interp, path));
    assert(info.phdr_vaddr == 0x400000 + sizeof(*eh));
    eh->e_type = ET_DYN;
    assert(elf64_probe(image, sizeof(image), &info));
    assert(info.dynamic && !strcmp(info.interp, path));
    eh->e_type = ET_EXEC;
    image[512 + sizeof(path) - 1] = 'x';
    assert(!elf64_probe(image, sizeof(image), &info));
    ph[1].p_type = 0;
    ph[2].p_type = 0;
    assert(elf64_probe(image, sizeof(image), &info) && !info.interp[0]);
    puts("PASS ET_EXEC/PT_INTERP, PIE, static ELF and malformed interpreter");
}
