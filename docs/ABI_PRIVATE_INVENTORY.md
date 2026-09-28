# Generated ReliefOS private ABI inventory
# Do not edit; regenerate with tools/check_abi_migration.py.

LEONOS_AUDIO_STATUS_NO_DEVICE:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
LEONOS_AUTHZ_INSTALL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/tests/legacy_authd/include/leonos/auth.h
LEONOS_AUTH_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_DEVICE_CLASS_AUDIO:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/device.h
  - include/leonos/devmgr_service.h
  - userland/runtime/include/leonos/devmand.h
  - userland/runtime/include/leonos/devmgr_service.h
LEONOS_DRIVER_CONTROL_IOCTL:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - include/leonos/devmgr_service.h
  - include/leonos/driver.h
  - userland/runtime/include/leonos/devmand.h
  - userland/runtime/include/leonos/devmgr_service.h
LEONOS_DRIVER_KIND_AUDIO:
  - docs/ABI_PRIVATE_INVENTORY.md
LEONOS_ENOTEMPTY:
  - docs/ABI_PRIVATE_INVENTORY.md
LEONOS_FDISK:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/busybox/block_storage.c
LEONOS_FS_TYPE_DEVICE:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/fs.h
  - include/leonos/http.h
  - userland/runtime/include/leonos/gui.h
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/include/leonos/pgl.h
  - userland/runtime/include/leonos/sudo.h
  - userland/runtime/include/leonos/syscall.h
  - userland/runtime/include/leonos/windowd.h
LEONOS_GUI_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_INPUTM_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_IOCTL_GPU_CREATE:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
LEONOS_IOCTL_GPU_DESTROY:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
LEONOS_IOCTL_GPU_DIAGNOSTICS:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
LEONOS_IOCTL_GPU_INFO:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
LEONOS_IOCTL_GPU_RENDER:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
LEONOS_IPC_SOCK_DEVICE:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/unix_ipc.h
LEONOS_KERNEL_DEBUG_BENCH_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
LEONOS_KERNEL_DEBUG_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_LAUNCH_ERR_EMPTY:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/launch.h
LEONOS_MOUNT_KIND_FAT32_RAMDISK:
  - docs/ABI_PRIVATE_INVENTORY.md
LEONOS_NET_AF_INET:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
LEONOS_NET_CONTROL_IOCTL:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/NETWORK_STATUS_2026-09-13.md
  - docs/SYSCALLS.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - docs/unix-ipc-protocol.md
LEONOS_NET_STATUS_NO_DEVICE:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
LEONOS_PTY_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/check_abi_migration.py
LEONOS_RAW_DEVICE_KIND_DISK:
  - docs/ABI_PRIVATE_INVENTORY.md
LEONOS_SIGNAL_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/check_abi_migration.py
LEONOS_STARTUP_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_TEXT_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_VFS_NODE_DEVICE:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_audio_configure:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
leonos_audio_format:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
leonos_audio_get_state:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
leonos_audio_state:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
leonos_audio_write:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
leonos_device_catalog_query:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/KERNEL_USERSPACE_BOUNDARIES.md
leonos_device_characteristics:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/sqlite/leonos_sqlite_vfs.c
leonos_device_info:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/KERNEL_USERSPACE_BOUNDARIES.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - docs/unix-ipc-protocol.md
  - include/leonos/device.h
  - include/leonos/devmgr_service.h
  - userland/runtime/include/leonos/devmand.h
  - userland/runtime/include/leonos/devmgr_service.h
leonos_device_list:
  - docs/ABI.md
  - docs/ABI_MIGRATION.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/KERNEL_USERSPACE_BOUNDARIES.md
  - docs/SYSCALLS.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - include/leonos/device.h
  - include/leonos/devmgr_service.h
  - userland/runtime/include/leonos/devmand.h
  - userland/runtime/include/leonos/devmgr_service.h
  - userland/runtime/src/devmand_client.c
leonos_disk_gpt_initialize:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_disk_partition:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_disk_partition_create:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_disk_partition_delete:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_disk_partition_edit:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_disk_partition_format:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_disk_partition_mount:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_disk_partition_unmount:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_driver_audio_ops:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_driver_control:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - include/leonos/devmgr_service.h
  - include/leonos/driver.h
  - userland/runtime/include/leonos/devmand.h
  - userland/runtime/include/leonos/devmgr_service.h
  - userland/runtime/src/devmand_client.c
leonos_driver_e1000_info:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_driver_e1000_ops:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_driver_info:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/OPENRC_MIGRATION_STATUS.md
  - docs/unix-ipc-protocol.md
  - include/leonos/devmgr_service.h
  - include/leonos/driver.h
  - userland/runtime/include/leonos/devmand.h
  - userland/runtime/include/leonos/devmgr_service.h
leonos_driver_kernel_api:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/devmgr_service.h
  - include/leonos/driver.h
  - userland/runtime/include/leonos/devmand.h
  - userland/runtime/include/leonos/devmgr_service.h
leonos_driver_list:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - include/leonos/devmgr_service.h
  - include/leonos/driver.h
  - userland/runtime/include/leonos/devmand.h
  - userland/runtime/include/leonos/devmgr_service.h
  - userland/runtime/src/devmand_client.c
leonos_driver_module:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/DRIVERS.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - include/leonos/devmgr_service.h
  - include/leonos/driver.h
  - userland/runtime/include/leonos/devmand.h
  - userland/runtime/include/leonos/devmgr_service.h
leonos_driver_mouse_ops:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_driver_mouse_state:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_driver_pci_device:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_driver_serial_ops:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_gpu_context:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
leonos_gpu_create:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
  - userland/runtime/src/gpu.c
leonos_gpu_destroy:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
  - userland/runtime/src/gpu.c
leonos_gpu_diagnostics:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
  - userland/runtime/src/gpu.c
leonos_gpu_draw:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
leonos_gpu_frame:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
leonos_gpu_info:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
  - userland/runtime/src/gpu.c
leonos_gpu_render:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
  - userland/runtime/src/gpu.c
leonos_gpu_vertex:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - userland/runtime/include/leonos/gpu_sdk.h
leonos_inputm_active_request:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
leonos_inputm_config_request:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
leonos_inputm_context:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
leonos_inputm_get_state:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_key_event:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
leonos_inputm_list:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_note_gui_window:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_notify_config:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_observe_gui_key:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_poll_gui_commit:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_poll_result:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_provider:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - docs/unix-ipc-protocol.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
leonos_inputm_provider_list:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
leonos_inputm_provider_next:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_provider_result:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_register:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_result:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
leonos_inputm_set_active:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_set_context:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_set_current_context:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_state:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
leonos_inputm_submit_key:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_take_key:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_take_text:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_inputm_unregister:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/api.h
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/runtime/include/leonos/inputm.h
  - userland/runtime/include/leonos/inputmd.h
  - userland/runtime/include/leonos/text_input.h
  - userland/runtime/src/inputm.c
leonos_install_disk:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_mouse_clear_regions:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/src/wind.c
leonos_mouse_get_position:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/src/wind.c
leonos_mouse_get_state:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/gui.h
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/include/leonos/pgl.h
  - userland/runtime/include/leonos/windowd.h
  - userland/runtime/src/wind.c
leonos_mouse_hide:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/src/wind.c
leonos_mouse_is_visible:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/src/wind.c
leonos_mouse_set_auto:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/src/wind.c
leonos_mouse_set_position:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/src/wind.c
leonos_mouse_set_region:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/src/wind.c
leonos_mouse_set_style:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/src/wind.c
leonos_mouse_show:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/src/wind.c
leonos_mouse_state:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/runtime/include/leonos/gui.h
  - userland/runtime/include/leonos/mouse.h
  - userland/runtime/include/leonos/pgl.h
  - userland/runtime/include/leonos/windowd.h
leonos_net_config:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_net_connection_info:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
leonos_net_connection_list:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
leonos_net_connections:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_net_control:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
leonos_net_dhcp:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
leonos_net_dhcp_renew:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - docs/unix-ipc-protocol.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_net_dns:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
leonos_net_dns_policy:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
leonos_net_dns_resolve:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_net_get_dns_policy:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_net_http_get:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_net_ping:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_net_set_dns_policy:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_net_socket_close:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
leonos_net_socket_connect:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
leonos_net_socket_io:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
leonos_net_socket_open:
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
leonos_pty_create:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_destroy:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_error:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_get_termios:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_get_winsize:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_input_available:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_io:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_read_output:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_self:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_set_termios:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_set_winsize:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_spawn:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_spawn_argv:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_spawn_argv_with_fds:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_termios:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/LINUX_ABI_AUDIT_2026-09-07.md
  - include/leonos/pty.h
leonos_pty_termios_io:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_termios_request:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_winsize:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/superpowers/ntclks-separation/01-header-classification.md
  - include/leonos/pty.h
leonos_pty_winsize_io:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_write_input:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_socket_close:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_socket_connect:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_socket_recv:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_socket_send:
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
leonos_socket_tcp:
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/http.h
  - include/leonos/net.h
  - include/leonos/net_service.h
  - userland/runtime/include/leonos/net_service.h
  - userland/runtime/src/netsock.c
