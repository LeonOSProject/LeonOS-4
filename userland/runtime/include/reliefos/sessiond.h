#ifndef RELIEFOS_SESSIOND_H
#define RELIEFOS_SESSIOND_H

#include <reliefos/startup.h>
#include <stdint.h>

enum reliefos_sessiond_msg {
    RELIEFOS_SESSIOND_MSG_HELLO = 10,
    RELIEFOS_SESSIOND_MSG_ACK = 11,
    RELIEFOS_SESSIOND_MSG_REQUEST = 20,
    RELIEFOS_SESSIOND_MSG_REQUEST_STATUS = 21,
    RELIEFOS_SESSIOND_MSG_DIALOG_GET = 22,
    RELIEFOS_SESSIOND_MSG_DIALOG = 23,
    RELIEFOS_SESSIOND_MSG_DIALOG_RESOLVE = 24,
    RELIEFOS_SESSIOND_MSG_LIST = 25,
    RELIEFOS_SESSIOND_MSG_SET_ENABLED = 26,
    RELIEFOS_SESSIOND_MSG_REMOVE = 27,
    RELIEFOS_SESSIOND_MSG_LAUNCH_CURRENT = 28,
};

struct reliefos_sessiond_hello {
    uint32_t pid;
    uint32_t uid;
    uint32_t reserved;
};

struct reliefos_sessiond_ack {
    int32_t code;
    uint32_t value;
};

struct reliefos_sessiond_list_ack {
    uint32_t uid;
    uint32_t count;
    uint32_t reserved;
    /* followed by count * struct reliefos_startup_entry */
};

#endif
