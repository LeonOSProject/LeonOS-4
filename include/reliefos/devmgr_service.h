#ifndef RELIEFOS_DEVMGR_SERVICE_H
#define RELIEFOS_DEVMGR_SERVICE_H

/* Versioned device/driver-management service SDK.
 *
 * All requests go to devmand over /run/leonos/devman.sock; /dev/hwinfo,
 * /dev/driverctl and fd 3 are gone.
 */
#include <reliefos/device.h>
#include <reliefos/driver.h>
#include <stdint.h>

#define DEVMGR_SERVICE_ABI_VERSION 1U

typedef struct reliefos_device_info system_device_info_t;
typedef struct reliefos_device_list system_device_list_t;
typedef struct reliefos_driver_info system_driver_info_t;
typedef struct reliefos_driver_list system_driver_list_t;
typedef struct reliefos_driver_control system_driver_control_t;

#define SYSTEM_DEVICE_MAX RELIEFOS_DEVICE_MAX
#define SYSTEM_DEVICE_DETAIL_LEN RELIEFOS_DEVICE_DETAIL_LEN

#define SYSTEM_DEVICE_CLASS_SYSTEM RELIEFOS_DEVICE_CLASS_SYSTEM
#define SYSTEM_DEVICE_CLASS_INPUT RELIEFOS_DEVICE_CLASS_INPUT
#define SYSTEM_DEVICE_CLASS_DISPLAY RELIEFOS_DEVICE_CLASS_DISPLAY
#define SYSTEM_DEVICE_CLASS_STORAGE RELIEFOS_DEVICE_CLASS_STORAGE
#define SYSTEM_DEVICE_CLASS_SERIAL RELIEFOS_DEVICE_CLASS_SERIAL
#define SYSTEM_DEVICE_CLASS_NETWORK RELIEFOS_DEVICE_CLASS_NETWORK
#define SYSTEM_DEVICE_CLASS_AUDIO RELIEFOS_DEVICE_CLASS_AUDIO

#define SYSTEM_DEVICE_FLAG_PRESENT RELIEFOS_DEVICE_FLAG_PRESENT
#define SYSTEM_DEVICE_FLAG_ACTIVE RELIEFOS_DEVICE_FLAG_ACTIVE
#define SYSTEM_DEVICE_FLAG_BOOT RELIEFOS_DEVICE_FLAG_BOOT

#define SYSTEM_DRIVER_MAX RELIEFOS_DRIVER_MAX
#define SYSTEM_DRIVER_STATE_LOADING RELIEFOS_DRIVER_STATE_LOADING
#define SYSTEM_DRIVER_STATE_LOADED RELIEFOS_DRIVER_STATE_LOADED
#define SYSTEM_DRIVER_STATE_DISABLED RELIEFOS_DRIVER_STATE_DISABLED
#define SYSTEM_DRIVER_STATE_FAILED RELIEFOS_DRIVER_STATE_FAILED
#define SYSTEM_DRIVER_FLAG_DISABLED RELIEFOS_DRIVER_FLAG_DISABLED

#define SYSTEM_DRIVER_CONTROL_RESCAN RELIEFOS_DRIVER_CONTROL_RESCAN
#define SYSTEM_DRIVER_CONTROL_LOAD RELIEFOS_DRIVER_CONTROL_LOAD
#define SYSTEM_DRIVER_CONTROL_UNLOAD RELIEFOS_DRIVER_CONTROL_UNLOAD
#define SYSTEM_DRIVER_CONTROL_FORCE_UNLOAD RELIEFOS_DRIVER_CONTROL_FORCE_UNLOAD
#define SYSTEM_DRIVER_CONTROL_ENABLE_BOOT RELIEFOS_DRIVER_CONTROL_ENABLE_BOOT
#define SYSTEM_DRIVER_CONTROL_DISABLE_BOOT RELIEFOS_DRIVER_CONTROL_DISABLE_BOOT

int system_device_list(system_device_info_t *devices, uint32_t capacity,
                       uint32_t *out_count);
int system_driver_list(system_driver_info_t *drivers, uint32_t capacity,
                       uint32_t *out_count);
int system_driver_control(uint32_t action, const char *file);

#endif
