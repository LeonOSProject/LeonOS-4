#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    void *library = dlopen("libz.so.1", RTLD_NOW);
    if (!library) { puts(dlerror()); return 1; }
    int (*compress)(unsigned char *, unsigned long *, const unsigned char *, unsigned long, int) = dlsym(library, "compress2");
    int (*expand)(unsigned char *, unsigned long *, const unsigned char *, unsigned long) = dlsym(library, "uncompress");
    const unsigned char source[] = "Alpine APK library running on NTCLKS";
    unsigned char packed[256], restored[256];
    unsigned long packed_size = sizeof(packed), restored_size = sizeof(restored);
    if (!compress || !expand || compress(packed, &packed_size, source, sizeof(source), 6) ||
        expand(restored, &restored_size, packed, packed_size) || restored_size != sizeof(source) ||
        memcmp(source, restored, sizeof(source))) return 1;
    puts("APK_ZLIB_ROUNDTRIP_OK");
    return dlclose(library) != 0;
}
