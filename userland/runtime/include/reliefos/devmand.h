#ifndef RELIEFOS_DEVMAND_H
#define RELIEFOS_DEVMAND_H

#include <reliefos/device.h>
#include <reliefos/driver.h>
#include <stdint.h>

enum reliefos_devmand_msg {
    RELIEFOS_DEVMAND_MSG_HELLO = 10,
    RELIEFOS_DEVMAND_MSG_ACK = 11,
    RELIEFOS_DEVMAND_MSG_DEVICE_LIST = 20,
    RELIEFOS_DEVMAND_MSG_DRIVER_LIST = 21,
    RELIEFOS_DEVMAND_MSG_DRIVER_CONTROL = 22,
};

struct reliefos_devmand_hello {
    uint32_t pid;
    uint32_t uid;
    uint32_t reserved;
};

struct reliefos_devmand_ack {
    int32_t code;
    uint32_t count;
};

struct reliefos_devmand_list_request {
    uint32_t capacity;
    uint32_t reserved;
};

#endif
