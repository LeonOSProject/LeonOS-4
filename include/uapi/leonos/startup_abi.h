#ifndef LEONOS_UAPI_STARTUP_ABI_H
#define LEONOS_UAPI_STARTUP_ABI_H
/*
 * Startup-approval IPC wire ABI between sessiond and userland.
 * Userland wrappers live in <leonos/startup.h>.
 * UAPI only: nothing here may include a non-UAPI header.
 */

#include <stdint.h>


#define LEONOS_STARTUP_MAX_ENTRIES 16U
#define LEONOS_STARTUP_MAX_ARGS 7U
#define LEONOS_STARTUP_ARG_LEN 64U

#define LEONOS_STARTUP_STATUS_PENDING 1U
#define LEONOS_STARTUP_STATUS_APPROVED 2U
#define LEONOS_STARTUP_STATUS_DENIED 3U
#define LEONOS_STARTUP_STATUS_DENIED_REMEMBERED 4U
#define LEONOS_STARTUP_STATUS_EXISTS 5U
#define LEONOS_STARTUP_STATUS_CANCELLED 6U
#define LEONOS_STARTUP_STATUS_FAILED 7U

#define LEONOS_STARTUP_DECISION_ALLOW 1U
#define LEONOS_STARTUP_DECISION_DENY 2U
#define LEONOS_STARTUP_DECISION_DENY_REMEMBERED 3U

/* args excludes argv[0]; the executable path is always argv[0]. */
struct leonos_startup_command {
    uint32_t argc;
    uint32_t reserved;
    char path[256];
    char args[LEONOS_STARTUP_MAX_ARGS][LEONOS_STARTUP_ARG_LEN];
};

struct leonos_startup_request {
    struct leonos_startup_command command;
    uint32_t request_id;
    uint32_t status;
};

struct leonos_startup_request_status {
    uint32_t request_id;
    uint32_t status;
};

struct leonos_startup_dialog_request {
    uint32_t request_id;
    uint32_t uid;
    char requester_path[256];
    struct leonos_startup_command command;
};

struct leonos_startup_dialog_resolution {
    uint32_t request_id;
    uint32_t decision;
};

struct leonos_startup_entry {
    uint32_t id;
    uint32_t enabled;
    struct leonos_startup_command command;
};

struct leonos_startup_list {
    uint32_t uid;
    uint32_t capacity;
    uint32_t count;
    uint32_t reserved;
    struct leonos_startup_entry *entries;
};

struct leonos_startup_update {
    uint32_t uid;
    uint32_t entry_id;
    uint32_t enabled;
    uint32_t reserved;
};

#endif /* LEONOS_UAPI_STARTUP_ABI_H */
