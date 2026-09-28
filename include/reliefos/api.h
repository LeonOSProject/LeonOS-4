#ifndef RELIEFOS_API_H
#define RELIEFOS_API_H

#include <stdint.h>
#include <reliefos/inputm.h>

#define RELIEFOS_API_PATH_MAX 256U
#define RELIEFOS_API_NAME_LEN 64U
#define RELIEFOS_API_VERSION_LEN 16U

struct reliefos_api_info {
    char id[64];
    char name[RELIEFOS_API_NAME_LEN];
    char version[RELIEFOS_API_VERSION_LEN];
    char category[64];
    char main_exe[RELIEFOS_API_PATH_MAX];
    char default_path[RELIEFOS_API_PATH_MAX];
    char icon[RELIEFOS_API_PATH_MAX];
    uint32_t requires_admin;
    uint32_t desktop_shortcut;
    uint32_t terminal;
    uint32_t hidden;
    uint32_t open_with;
    char commands[256];
    char extensions[256];
    uint32_t input_method;
    char input_method_id[RELIEFOS_INPUTM_ID_LEN];
    char input_method_abbreviation[RELIEFOS_INPUTM_ABBREV_LEN];
    uint32_t input_method_startup_mode;
    uint32_t input_method_launch_after_install;
    char input_method_settings[RELIEFOS_API_PATH_MAX];
    char input_method_settings_app[RELIEFOS_API_PATH_MAX];
};

typedef int (*reliefos_api_progress_fn)(uint32_t processed, uint32_t total,
                                      void *context);

int reliefos_api_parse_info(const char *api_path, struct reliefos_api_info *info);
int reliefos_api_extract_files(const char *api_path, const char *dest_dir);
int reliefos_api_install(const char *api_path, const char *dest_dir,
                       uint32_t create_shortcut);
int reliefos_api_install_with_progress(const char *api_path,
                                     const char *dest_dir,
                                     uint32_t create_shortcut,
                                     reliefos_api_progress_fn progress,
                                     void *context);

#endif
