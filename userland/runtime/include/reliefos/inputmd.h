#ifndef RELIEFOS_INPUTMD_H
#define RELIEFOS_INPUTMD_H

#include <reliefos/inputm.h>
#include <stdint.h>

enum reliefos_imd_msg {
    RELIEFOS_IMD_MSG_HELLO = 10,
    RELIEFOS_IMD_MSG_ACK = 11,
    RELIEFOS_IMD_MSG_REGISTER = 20,
    RELIEFOS_IMD_MSG_UNREGISTER = 21,
    RELIEFOS_IMD_MSG_KEY_EVENT = 22,
    RELIEFOS_IMD_MSG_RESULT = 23,
    RELIEFOS_IMD_MSG_SUBMIT_KEY = 24,
    RELIEFOS_IMD_MSG_SET_CONTEXT = 25,
    RELIEFOS_IMD_MSG_SET_ACTIVE = 26,
    RELIEFOS_IMD_MSG_LIST = 27,
    RELIEFOS_IMD_MSG_LIST_ACK = 28,
    RELIEFOS_IMD_MSG_GET_STATE = 29,
    RELIEFOS_IMD_MSG_STATE_ACK = 30,
    RELIEFOS_IMD_MSG_NOTIFY_CONFIG = 31,
};

#define RELIEFOS_IMD_ROLE_APP 1u
#define RELIEFOS_IMD_ROLE_PROVIDER 2u

struct reliefos_imd_hello {
    uint32_t pid;
    uint32_t role;
};

struct reliefos_imd_ack {
    int32_t code;
    uint32_t reserved;
};

struct reliefos_imd_list {
    uint32_t uid;
    uint32_t capacity;
};

struct reliefos_imd_list_ack {
    uint32_t uid;
    uint32_t count;
    uint32_t reserved;
    /* followed by count * struct reliefos_inputm_provider */
};

struct reliefos_imd_get_state {
    uint32_t uid;
    uint32_t reserved;
};

#endif
