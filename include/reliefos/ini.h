#ifndef RELIEFOS_INI_H
#define RELIEFOS_INI_H

#include <stdint.h>

#define RELIEFOS_INI_MAX_SIZE (64U * 1024U)
#define RELIEFOS_INI_MAX_SECTIONS 32U
#define RELIEFOS_INI_MAX_KEYS_PER_SECTION 64U
#define RELIEFOS_INI_NAME_LEN 64U
#define RELIEFOS_INI_VALUE_LEN 256U

int reliefos_ini_load(const char *path);
int reliefos_ini_load_strict(const char *path);
int reliefos_ini_get(const char *section, const char *key,
                   char *value, uint32_t capacity);
int reliefos_ini_section_count(void);
int reliefos_ini_section_name(uint32_t index, char *name, uint32_t capacity);
int reliefos_ini_key_count(const char *section);
int reliefos_ini_key_name(const char *section, uint32_t index,
                        char *name, uint32_t capacity);

#endif
