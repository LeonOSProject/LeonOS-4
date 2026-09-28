#include <stddef.h>
#include <leonos/api.h>
#include <leonos/tar.h>

_Static_assert(LEONOS_API_PATH_MAX == 256U, "legacy API path limit changed");
_Static_assert(LEONOS_API_NAME_LEN == 64U, "legacy API name limit changed");
_Static_assert(sizeof(struct leonos_api_info) == 2072U,
               "legacy API info size changed");
_Static_assert(offsetof(struct leonos_api_info, requires_admin) == 976U,
               "legacy API flags offset changed");
_Static_assert(offsetof(struct leonos_api_info, input_method_settings) == 1560U,
               "legacy API settings offset changed");
_Static_assert(LEONOS_TAR_BLOCK_SIZE == 512U, "legacy tar block changed");

static int (*volatile legacy_entry)(const char *, char *, uint32_t) =
    leonos_tar_list;

int main(void)
{
    return legacy_entry == NULL;
}
