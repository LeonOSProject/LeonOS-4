#include <stddef.h>
#include <reliefos/api.h>
#include <reliefos/tar.h>

_Static_assert(RELIEFOS_API_PATH_MAX == 256U, "API path limit changed");
_Static_assert(RELIEFOS_API_NAME_LEN == 64U, "API name limit changed");
_Static_assert(sizeof(struct reliefos_api_info) == 2072U,
               "API info size changed");
_Static_assert(offsetof(struct reliefos_api_info, requires_admin) == 976U,
               "API flags offset changed");
_Static_assert(offsetof(struct reliefos_api_info, input_method_settings) == 1560U,
               "API settings offset changed");
_Static_assert(RELIEFOS_TAR_BLOCK_SIZE == 512U, "tar block changed");

static int (*volatile canonical_entry)(const char *, char *, uint32_t) =
    reliefos_tar_list;

int main(void)
{
    return canonical_entry == NULL;
}
