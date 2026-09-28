#ifndef RELIEFOS_APP_H
#define RELIEFOS_APP_H

#include <stdint.h>

/* The registry is intentionally bounded: a ReliefOS image currently exposes a
 * small number of application directories, while the API remains independent
 * of the build-time component list. */
#define RELIEFOS_APP_REGISTRY_MAX 128U
#define RELIEFOS_APP_ID_LEN 64U
#define RELIEFOS_APP_NAME_LEN 96U
#define RELIEFOS_APP_VERSION_LEN 32U
#define RELIEFOS_APP_CATEGORY_LEN 64U
#define RELIEFOS_APP_PATH_LEN 256U
#define RELIEFOS_APP_LIST_LEN 256U

#define RELIEFOS_APP_FLAG_ENTRY 0x00000001U
#define RELIEFOS_APP_FLAG_TERMINAL 0x00000002U
#define RELIEFOS_APP_FLAG_SYSTEM 0x00000004U
#define RELIEFOS_APP_FLAG_HIDDEN 0x00000008U
#define RELIEFOS_APP_FLAG_OPEN_WITH 0x00000010U

struct reliefos_app_info {
    char id[RELIEFOS_APP_ID_LEN];
    char name[RELIEFOS_APP_NAME_LEN];
    char version[RELIEFOS_APP_VERSION_LEN];
    char category[RELIEFOS_APP_CATEGORY_LEN];
    char exec[RELIEFOS_APP_PATH_LEN];
    char icon[RELIEFOS_APP_PATH_LEN];
    char commands[RELIEFOS_APP_LIST_LEN];
    char extensions[RELIEFOS_APP_LIST_LEN];
    uint32_t flags;
};

int reliefos_app_registry_refresh(void);
int reliefos_app_registry_begin_refresh(void);
int reliefos_app_registry_refresh_step(uint32_t budget);
int reliefos_app_registry_is_loading(void);
int reliefos_app_registry_is_loaded(void);
uint32_t reliefos_app_registry_count(void);
int reliefos_app_registry_get(uint32_t index, struct reliefos_app_info *info);
int reliefos_app_registry_find(const char *id_or_path,
                             struct reliefos_app_info *info);
int reliefos_app_registry_resolve(const char *name_or_path,
                                char *path, uint32_t capacity);
int reliefos_app_registry_label(const char *path, char *label, uint32_t capacity);
int reliefos_app_registry_icon(const char *path, char *icon, uint32_t capacity);
int reliefos_app_registry_default_for_extension(const char *extension,
                                              char *path, uint32_t capacity);

#endif
