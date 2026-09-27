/* Transitional compatibility forwarder (ReliefOS / ReliefNT rename).
 * Canonical declarations: <reliefos/ini.h>.
 * No second layout definition exists behind these names. */
#ifndef LEONOS_INI_H
#define LEONOS_INI_H
#include <reliefos/ini.h>

/* Old names alias the single canonical declaration. */
#define LEONOS_INI_MAX_KEYS_PER_SECTION RELIEFOS_INI_MAX_KEYS_PER_SECTION
#define LEONOS_INI_MAX_SECTIONS RELIEFOS_INI_MAX_SECTIONS
#define LEONOS_INI_MAX_SIZE RELIEFOS_INI_MAX_SIZE
#define LEONOS_INI_NAME_LEN RELIEFOS_INI_NAME_LEN
#define LEONOS_INI_VALUE_LEN RELIEFOS_INI_VALUE_LEN
#define leonos_ini_get reliefos_ini_get
#define leonos_ini_key_count reliefos_ini_key_count
#define leonos_ini_key_name reliefos_ini_key_name
#define leonos_ini_load reliefos_ini_load
#define leonos_ini_load_strict reliefos_ini_load_strict
#define leonos_ini_section_count reliefos_ini_section_count
#define leonos_ini_section_name reliefos_ini_section_name
#endif /* LEONOS_INI_H */
