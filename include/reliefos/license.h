#ifndef RELIEFOS_LICENSE_H
#define RELIEFOS_LICENSE_H

#include <stdint.h>

#define RELIEFOS_LICENSE_EMAIL_LEN 96U
#define RELIEFOS_LICENSE_KEY_LEN 64U
#define RELIEFOS_LICENSE_INSTALL_ID_LEN 48U
#define RELIEFOS_LICENSE_SERVER_URL_LEN 128U
#define RELIEFOS_LICENSE_STATUS_LEN 96U

#define RELIEFOS_LICENSE_STATUS_OK 0
#define RELIEFOS_LICENSE_STATUS_MISSING 1
#define RELIEFOS_LICENSE_STATUS_INVALID 2
#define RELIEFOS_LICENSE_STATUS_NETWORK 3
#define RELIEFOS_LICENSE_STATUS_CLOCK 4
#define RELIEFOS_LICENSE_STATUS_DENIED 5

struct reliefos_license_info {
    uint32_t status;
    char mode[16];
    char email_hash[24];
    char install_id[RELIEFOS_LICENSE_INSTALL_ID_LEN];
    char detail[RELIEFOS_LICENSE_STATUS_LEN];
};

int reliefos_license_status(struct reliefos_license_info *info);
int reliefos_license_required(void);
int reliefos_license_default_server(char *out, uint32_t cap);
int reliefos_license_install_id(char *out, uint32_t cap);
int reliefos_license_activate_online(const char *email, const char *key,
                                   char *detail, uint32_t detail_cap);
int reliefos_license_activate_offline(const char *email, const char *offline_key,
                                    char *detail, uint32_t detail_cap);

#endif
