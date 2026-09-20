# Generated LeonOS private ABI inventory
# Do not edit; regenerate with tools/check_abi_migration.py.

LEONOS_AUDIO_STATUS_NO_DEVICE:
  - devtools/components/tcc/runtime/include/leonos/audio.h
  - devtools/include/leonos/audio.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
  - kernel/ntclks/driver_manager.c
LEONOS_AUTHZ_INSTALL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - tools/tests/legacy_authd/include/leonos/auth.h
LEONOS_AUTH_IOCTL:
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_DEVICE_CLASS_AUDIO:
  - devtools/components/tcc/runtime/include/leonos/device.h
  - devtools/components/tcc/runtime/include/leonos/devmgr_service.h
  - devtools/include/leonos/device.h
  - devtools/include/leonos/devmgr_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/device.h
  - include/leonos/devmgr_service.h
  - userland/apps/device-agent/main.c
  - userland/libc/include/leonos/devmgr_service.h
LEONOS_DRIVER_CONTROL_IOCTL:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - include/leonos/driver.h
  - kernel/ntclks/syscall_device.c
  - tools/tests/driver_control_test.c
  - userland/apps/device-agent/main.c
LEONOS_DRIVER_KIND_AUDIO:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/ac97/ac97.c
  - drivers/es1371/es1371.c
  - include/leonos/driver.h
LEONOS_ENOTEMPTY:
  - docs/ABI_PRIVATE_INVENTORY.md
  - kernel/ntclks/include/ntclks/syscall.h
  - kernel/ntclks/syscall.c
LEONOS_FDISK:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/busybox/block_storage.c
LEONOS_FS_TYPE_DEVICE:
  - devtools/components/tcc/runtime/include/leonos/fs_abi.h
  - devtools/docs/SYSCALLS.md
  - devtools/include/leonos/fs_abi.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_vfs.c
  - include/uapi/leonos/fs_abi.h
  - kernel/ntclks/net_packet.c
  - kernel/ntclks/net_udp.c
  - kernel/ntclks/pty.c
  - kernel/ntclks/signalfd.c
  - kernel/ntclks/syscall.c
  - kernel/ntclks/syscall_socket.c
  - tools/tests/ioctl_cloexec_table_test.c
  - tools/tests/linux_permissions_test.c
  - userland/apps/device-agent/main.c
  - userland/apps/installer/installer_directory.h
  - userland/apps/installer/main.c
  - userland/libc/src/libc.c
LEONOS_GUI_IOCTL:
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_INPUTM_IOCTL:
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_IOCTL_GPU_CREATE:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/include/leonos/gpu.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - kernel/ntclks/gpu.c
  - tools/tests/gpu_syscall_test.c
  - userland/libc/src/gpu.c
  - userland/libc/src/gpu_sdk.c
LEONOS_IOCTL_GPU_DESTROY:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/include/leonos/gpu.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - kernel/ntclks/gpu.c
  - tools/tests/gpu_syscall_test.c
  - userland/libc/src/gpu.c
  - userland/libc/src/gpu_sdk.c
LEONOS_IOCTL_GPU_DIAGNOSTICS:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/include/leonos/gpu.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - kernel/ntclks/gpu.c
  - tools/tests/gpu_syscall_test.c
  - userland/libc/src/gpu.c
  - userland/libc/src/gpu_sdk.c
LEONOS_IOCTL_GPU_INFO:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/include/leonos/gpu.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/gpu.h
  - kernel/ntclks/gpu.c
  - tools/tests/gpu_syscall_test.c
  - userland/libc/src/gpu.c
  - userland/libc/src/gpu_sdk.c
LEONOS_IOCTL_GPU_RENDER:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/include/leonos/gpu.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - tools/tests/gpu_syscall_test.c
  - userland/libc/src/gpu.c
  - userland/libc/src/gpu_sdk.c
LEONOS_IPC_SOCK_DEVICE:
  - devtools/components/tcc/runtime/include/leonos/unix_ipc.h
  - devtools/include/leonos/unix_ipc.h
  - userland/apps/device-agent/main.c
  - userland/libc/include/leonos/unix_ipc.h
  - userland/libc/src/devmand_client.c
LEONOS_KERNEL_DEBUG_BENCH_IOCTL:
  - docs/ABI_PRIVATE_INVENTORY.md
  - kernel/kerneldebug/kerneldebug.c
  - kernel/ntclks/include/ntclks/kernel_debug.h
  - kernel/ntclks/kernel_debug.c
LEONOS_KERNEL_DEBUG_IOCTL:
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_LAUNCH_ERR_EMPTY:
  - devtools/components/tcc/runtime/include/leonos/launch.h
  - devtools/include/leonos/launch.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/launch.h
  - userland/libc/src/launch.c
LEONOS_MOUNT_KIND_FAT32_RAMDISK:
  - docs/ABI_PRIVATE_INVENTORY.md
LEONOS_NET_AF_INET:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - kernel/ntclks/net.c
  - userland/libc/include/leonos/net_service.h
LEONOS_NET_CONTROL_IOCTL:
  - docs/NETWORK_STATUS_2026-09-13.md
  - include/uapi/leonos/net_control.h
  - kernel/ntclks/net_control.c
  - tools/tests/netmand_client_test.c
  - tools/tests/network_guest_test.c
  - userland/libc/src/netsock.c
LEONOS_NET_STATUS_NO_DEVICE:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - kernel/ntclks/net.c
  - userland/libc/include/leonos/net_service.h
LEONOS_PTY_IOCTL:
  - tools/check_abi_migration.py
LEONOS_RAW_DEVICE_KIND_DISK:
  - docs/ABI_PRIVATE_INVENTORY.md
LEONOS_SIGNAL_IOCTL:
  - tools/check_abi_migration.py
LEONOS_STARTUP_IOCTL:
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_TEXT_IOCTL:
  - tools/check_abi_migration.py
  - tools/test_security_regressions.py
LEONOS_VFS_NODE_DEVICE:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_audio_configure:
  - devtools/components/tcc/runtime/include/leonos/audio.h
  - devtools/include/leonos/audio.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
leonos_audio_format:
  - devtools/components/tcc/runtime/include/leonos/audio.h
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/audio.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/ac97/ac97.c
  - drivers/es1371/es1371.c
  - include/leonos/audio.h
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
  - kernel/ntclks/include/ntclks/driver_manager.h
  - kernel/ntclks/syscall.c
leonos_audio_get_state:
  - devtools/components/tcc/runtime/include/leonos/audio.h
  - devtools/include/leonos/audio.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
leonos_audio_state:
  - devtools/components/tcc/runtime/include/leonos/audio.h
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/audio.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/ac97/ac97.c
  - drivers/es1371/es1371.c
  - include/leonos/audio.h
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
  - kernel/ntclks/include/ntclks/driver_manager.h
  - kernel/ntclks/syscall.c
leonos_audio_write:
  - devtools/components/tcc/runtime/include/leonos/audio.h
  - devtools/include/leonos/audio.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/audio.h
leonos_device_catalog_query:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_device_characteristics:
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/sqlite/leonos_sqlite_vfs.c
leonos_device_info:
  - devtools/components/tcc/runtime/include/leonos/device.h
  - devtools/components/tcc/runtime/include/leonos/devmgr_service.h
  - devtools/include/leonos/device.h
  - devtools/include/leonos/devmgr_service.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/device.h
  - include/leonos/devmgr_service.h
  - userland/apps/device-agent/main.c
  - userland/libc/include/leonos/devmgr_service.h
  - userland/libc/src/devmand_client.c
leonos_device_list:
  - devtools/components/tcc/runtime/include/leonos/device.h
  - devtools/components/tcc/runtime/include/leonos/devmgr_service.h
  - devtools/include/leonos/device.h
  - devtools/include/leonos/devmgr_service.h
  - docs/ABI.md
  - docs/ABI_MIGRATION.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/device.h
  - include/leonos/devmgr_service.h
  - userland/libc/include/leonos/devmgr_service.h
  - userland/libc/src/devmand_client.c
  - userland/libc/src/devmgr_service.c
leonos_disk_gpt_initialize:
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_disk.c
  - kernel/ntclks/include/ntclks/storage.h
leonos_disk_partition:
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_disk.c
  - kernel/ntclks/include/ntclks/storage.h
leonos_disk_partition_create:
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_disk.c
  - kernel/ntclks/include/ntclks/storage.h
leonos_disk_partition_delete:
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_disk.c
  - kernel/ntclks/include/ntclks/storage.h
leonos_disk_partition_edit:
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_disk.c
  - kernel/ntclks/include/ntclks/storage.h
leonos_disk_partition_format:
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_disk.c
  - kernel/ntclks/include/ntclks/storage.h
leonos_disk_partition_mount:
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_disk.c
  - kernel/ntclks/include/ntclks/storage.h
leonos_disk_partition_unmount:
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_disk.c
  - kernel/ntclks/include/ntclks/storage.h
leonos_driver_audio_ops:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/ac97/ac97.c
  - drivers/es1371/es1371.c
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
leonos_driver_control:
  - devtools/components/tcc/runtime/include/leonos/devmgr_service.h
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/devmgr_service.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/devmgr_service.h
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
  - kernel/ntclks/include/ntclks/driver_manager.h
  - kernel/ntclks/syscall_device.c
  - tools/tests/driver_control_test.c
  - userland/apps/device-agent/main.c
  - userland/libc/include/leonos/devmgr_service.h
  - userland/libc/src/devmand_client.c
  - userland/libc/src/devmgr_service.c
leonos_driver_e1000_info:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/e1000/e1000.c
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
leonos_driver_e1000_ops:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/e1000/e1000.c
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
  - tools/tests/e1000_init_test.c
leonos_driver_info:
  - devtools/components/tcc/runtime/include/leonos/devmgr_service.h
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/devmgr_service.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/OPENRC_MIGRATION_STATUS.md
  - docs/unix-ipc-protocol.md
  - include/leonos/devmgr_service.h
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
  - kernel/ntclks/procfs.c
  - userland/apps/device-agent/main.c
  - userland/libc/include/leonos/devmgr_service.h
  - userland/libc/src/devmand_client.c
leonos_driver_kernel_api:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/DRIVERS.md
  - drivers/ac97/ac97.c
  - drivers/e1000/e1000.c
  - drivers/es1371/es1371.c
  - drivers/mouse/mouse.c
  - drivers/serial/serial.c
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
  - tools/tests/e1000_init_test.c
  - tools/tests/mouse_init_test.c
leonos_driver_list:
  - devtools/components/tcc/runtime/include/leonos/devmgr_service.h
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/devmgr_service.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/devmgr_service.h
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
  - kernel/ntclks/include/ntclks/driver_manager.h
  - kernel/ntclks/procfs.c
  - tools/tests/procfs_directories_test.c
  - userland/libc/include/leonos/devmgr_service.h
  - userland/libc/src/devmand_client.c
  - userland/libc/src/devmgr_service.c
leonos_driver_module:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/DRIVERS.md
  - drivers/ac97/ac97.c
  - drivers/e1000/e1000.c
  - drivers/es1371/es1371.c
  - drivers/mouse/mouse.c
  - drivers/serial/serial.c
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
leonos_driver_mouse_ops:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/mouse/mouse.c
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
leonos_driver_mouse_state:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/mouse/mouse.c
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
leonos_driver_pci_device:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/ac97/ac97.c
  - drivers/e1000/e1000.c
  - drivers/es1371/es1371.c
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
  - tools/tests/e1000_init_test.c
leonos_driver_serial_ops:
  - devtools/components/tcc/runtime/include/leonos/driver.h
  - devtools/include/leonos/driver.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/serial/serial.c
  - include/leonos/driver.h
  - kernel/ntclks/driver_manager.c
leonos_gpu_context:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/components/tcc/runtime/include/leonos/gpu_sdk.h
  - devtools/include/leonos/gpu.h
  - devtools/include/leonos/gpu_sdk.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/svga/render.c
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - kernel/ntclks/gpu.c
  - kernel/ntclks/include/ntclks/svga.h
  - tools/tests/gpu_syscall_test.c
  - tools/tests/svga_test.c
  - userland/libc/include/leonos/gpu_sdk.h
  - userland/libc/src/gpu.c
leonos_gpu_create:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/include/leonos/gpu.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - userland/libc/src/gpu.c
leonos_gpu_destroy:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/components/tcc/runtime/include/leonos/gpu_sdk.h
  - devtools/include/leonos/gpu.h
  - devtools/include/leonos/gpu_sdk.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - kernel/ntclks/gpu.c
  - tools/tests/gpu_syscall_test.c
  - userland/libc/include/leonos/gpu_sdk.h
  - userland/libc/src/gpu.c
leonos_gpu_diagnostics:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/components/tcc/runtime/include/leonos/gpu_sdk.h
  - devtools/include/leonos/gpu.h
  - devtools/include/leonos/gpu_sdk.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/svga/device.h
  - drivers/bootstrap/svga/render.c
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - kernel/ntclks/gpu.c
  - kernel/ntclks/include/ntclks/svga.h
  - tools/tests/gpu_syscall_test.c
  - tools/tests/svga_test.c
  - userland/libc/include/leonos/gpu_sdk.h
  - userland/libc/src/gpu.c
leonos_gpu_draw:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/components/tcc/runtime/include/leonos/gpu_sdk.h
  - devtools/include/leonos/gpu.h
  - devtools/include/leonos/gpu_sdk.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/svga/render.c
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - kernel/ntclks/gpu.c
  - kernel/ntclks/include/ntclks/svga.h
  - tools/tests/gpu_syscall_test.c
  - tools/tests/svga_test.c
  - userland/libc/include/leonos/gpu_sdk.h
leonos_gpu_frame:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/components/tcc/runtime/include/leonos/gpu_sdk.h
  - devtools/include/leonos/gpu.h
  - devtools/include/leonos/gpu_sdk.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/svga/render.c
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - kernel/ntclks/gpu.c
  - kernel/ntclks/include/ntclks/svga.h
  - tools/tests/gpu_syscall_test.c
  - tools/tests/svga_test.c
  - userland/libc/include/leonos/gpu_sdk.h
  - userland/libc/src/gpu.c
leonos_gpu_info:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/components/tcc/runtime/include/leonos/gpu_sdk.h
  - devtools/include/leonos/gpu.h
  - devtools/include/leonos/gpu_sdk.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/svga/render.c
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - kernel/ntclks/gpu.c
  - kernel/ntclks/include/ntclks/svga.h
  - tools/tests/gpu_syscall_test.c
  - tools/tests/svga_test.c
  - tools/tests/taskmgr_gpu_sample_test.c
  - userland/libc/include/leonos/gpu_sdk.h
  - userland/libc/src/gpu.c
leonos_gpu_render:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/include/leonos/gpu.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/gpu.h
  - userland/libc/src/gpu.c
leonos_gpu_vertex:
  - devtools/components/tcc/runtime/include/leonos/gpu.h
  - devtools/components/tcc/runtime/include/leonos/gpu_sdk.h
  - devtools/include/leonos/gpu.h
  - devtools/include/leonos/gpu_sdk.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/svga/render.c
  - include/leonos/gpu.h
  - include/leonos/gpu_sdk.h
  - kernel/ntclks/gpu.c
  - kernel/ntclks/include/ntclks/svga.h
  - tools/tests/gpu_syscall_test.c
  - tools/tests/svga_test.c
  - userland/libc/include/leonos/gpu_sdk.h
leonos_inputm_active_request:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/components/tcc/runtime/include/leonos/text_input.h
  - devtools/include/leonos/inputm.h
  - devtools/include/leonos/text_input.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/apps/imd/main.c
  - userland/libc/include/leonos/inputm.h
  - userland/libc/include/leonos/text_input.h
  - userland/libc/src/inputm.c
leonos_inputm_config_request:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/components/tcc/runtime/include/leonos/text_input.h
  - devtools/include/leonos/inputm.h
  - devtools/include/leonos/text_input.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/apps/imd/main.c
  - userland/libc/include/leonos/inputm.h
  - userland/libc/include/leonos/text_input.h
  - userland/libc/src/inputm.c
leonos_inputm_context:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/components/tcc/runtime/include/leonos/text_input.h
  - devtools/include/leonos/inputm.h
  - devtools/include/leonos/text_input.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - tools/tests/oobe_inputm_test.c
  - userland/apps/imd/main.c
  - userland/libc/include/leonos/inputm.h
  - userland/libc/include/leonos/text_input.h
  - userland/libc/src/inputm.c
leonos_inputm_get_state:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_key_event:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/components/tcc/runtime/include/leonos/text_input.h
  - devtools/docs/INPUTM.md
  - devtools/examples/inputm_provider/main.c
  - devtools/include/leonos/inputm.h
  - devtools/include/leonos/text_input.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/apps/imd/main.c
  - userland/libc/include/leonos/inputm.h
  - userland/libc/include/leonos/text_input.h
  - userland/libc/src/inputm.c
leonos_inputm_list:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_note_gui_window:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
  - userland/libc/src/wind.c
leonos_inputm_notify_config:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/api.c
  - userland/libc/src/inputm.c
leonos_inputm_observe_gui_key:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_poll_gui_commit:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_poll_result:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_provider:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/components/tcc/runtime/include/leonos/inputmd.h
  - devtools/components/tcc/runtime/include/leonos/text_input.h
  - devtools/docs/INPUTM.md
  - devtools/examples/inputm_provider/main.c
  - devtools/include/leonos/inputm.h
  - devtools/include/leonos/inputmd.h
  - devtools/include/leonos/text_input.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - tools/tests/oobe_inputm_test.c
  - userland/apps/imd/main.c
  - userland/libc/include/leonos/inputm.h
  - userland/libc/include/leonos/inputmd.h
  - userland/libc/include/leonos/text_input.h
  - userland/libc/src/inputm.c
leonos_inputm_provider_list:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/components/tcc/runtime/include/leonos/text_input.h
  - devtools/include/leonos/inputm.h
  - devtools/include/leonos/text_input.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/include/leonos/text_input.h
leonos_inputm_provider_next:
  - devtools/README.md
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/examples/inputm_provider/main.c
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_provider_result:
  - devtools/README.md
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/examples/inputm_provider/main.c
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_register:
  - devtools/README.md
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/examples/inputm_provider/main.c
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_result:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/components/tcc/runtime/include/leonos/text_input.h
  - devtools/docs/INPUTM.md
  - devtools/examples/inputm_provider/main.c
  - devtools/include/leonos/inputm.h
  - devtools/include/leonos/text_input.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - userland/apps/imd/main.c
  - userland/libc/include/leonos/inputm.h
  - userland/libc/include/leonos/text_input.h
  - userland/libc/src/inputm.c
leonos_inputm_set_active:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_set_context:
  - devtools/README.md
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_set_current_context:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/docs/UI.md
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/apps/installer/main.c
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
  - userland/libc/src/ui_edit.c
leonos_inputm_state:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/components/tcc/runtime/include/leonos/text_input.h
  - devtools/include/leonos/inputm.h
  - devtools/include/leonos/text_input.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - include/leonos/text_input.h
  - tools/tests/oobe_inputm_test.c
  - userland/apps/imd/main.c
  - userland/libc/include/leonos/inputm.h
  - userland/libc/include/leonos/text_input.h
  - userland/libc/src/inputm.c
leonos_inputm_submit_key:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_take_key:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_inputm_take_text:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
  - userland/libc/src/ui_edit.c
leonos_inputm_unregister:
  - devtools/components/tcc/runtime/include/leonos/inputm.h
  - devtools/docs/INPUTM.md
  - devtools/include/leonos/inputm.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/inputm.h
  - userland/libc/include/leonos/inputm.h
  - userland/libc/src/inputm.c
leonos_install_disk:
  - docs/ABI_PRIVATE_INVENTORY.md
  - drivers/bootstrap/storage/storage_disk.c
  - kernel/ntclks/include/ntclks/storage.h
  - kernel/ntclks/sysfs.c
  - tools/tests/linux_inventory_test.c
leonos_mouse_clear_regions:
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/docs/GUI.md
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/ui_surface.c
  - userland/libc/src/wind.c
leonos_mouse_get_position:
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/docs/GUI.md
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/wind.c
leonos_mouse_get_state:
  - devtools/components/tcc/runtime/include/leonos/gui.h
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/docs/GUI.md
  - devtools/include/leonos/gui.h
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/gui.h
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/wind.c
leonos_mouse_hide:
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/docs/GUI.md
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/wind.c
leonos_mouse_is_visible:
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/wind.c
leonos_mouse_set_auto:
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/docs/GUI.md
  - devtools/docs/UI.md
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/wind.c
leonos_mouse_set_position:
  - devtools/README.md
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/docs/GUI.md
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/wind.c
leonos_mouse_set_region:
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/docs/GUI.md
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/ui_surface.c
  - userland/libc/src/wind.c
leonos_mouse_set_style:
  - devtools/README.md
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/docs/GUI.md
  - devtools/docs/UI.md
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/wind.c
leonos_mouse_show:
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/docs/GUI.md
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/wind.c
leonos_mouse_state:
  - devtools/components/tcc/runtime/include/leonos/gui.h
  - devtools/components/tcc/runtime/include/leonos/mouse.h
  - devtools/include/leonos/gui.h
  - devtools/include/leonos/mouse.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - userland/libc/include/leonos/gui.h
  - userland/libc/include/leonos/mouse.h
  - userland/libc/src/wind.c
leonos_net_config:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - docs/unix-ipc-protocol.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - include/uapi/leonos/net_control.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - kernel/ntclks/net_packet.c
  - kernel/ntclks/net_udp.c
  - tools/tests/net_packet_test.c
  - tools/tests/netmand_client_test.c
  - tools/tests/network_guest_test.c
  - tools/tests/tcp_state_test.c
  - tools/tests/udp_socket_test.c
  - userland/libc/include/leonos/net_service.h
  - userland/libc/src/license.c
  - userland/libc/src/net_service.c
  - userland/libc/src/netsock.c
leonos_net_connection_info:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - include/uapi/leonos/net_control.h
  - kernel/ntclks/net.c
  - userland/libc/include/leonos/net_service.h
  - userland/libc/src/netsock.c
leonos_net_connection_list:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - kernel/ntclks/net_control.c
  - userland/libc/include/leonos/net_service.h
leonos_net_connections:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/net.h
  - userland/libc/src/net_service.c
  - userland/libc/src/netsock.c
leonos_net_control:
  - include/uapi/leonos/net_control.h
  - kernel/ntclks/net_control.c
  - tools/tests/netmand_client_test.c
  - tools/tests/network_guest_test.c
  - userland/libc/src/netsock.c
leonos_net_dhcp:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - include/uapi/leonos/net_control.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - tools/tests/netmand_client_test.c
  - tools/tests/network_guest_test.c
  - userland/libc/include/leonos/net_service.h
  - userland/libc/src/netsock.c
leonos_net_dhcp_renew:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/net.h
  - tools/tests/netmand_client_test.c
  - tools/tests/network_guest_test.c
  - userland/libc/src/net_service.c
  - userland/libc/src/netsock.c
leonos_net_dns:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - userland/libc/include/leonos/net_service.h
  - userland/libc/src/netsock.c
leonos_net_dns_policy:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/unix-ipc-protocol.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - include/uapi/leonos/net_control.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - userland/libc/include/leonos/net_service.h
  - userland/libc/src/netsock.c
leonos_net_dns_resolve:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/net.h
  - userland/libc/src/net_service.c
  - userland/libc/src/netsock.c
leonos_net_get_dns_policy:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - userland/libc/src/net_service.c
  - userland/libc/src/netsock.c
leonos_net_http_get:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - userland/libc/include/leonos/net_service.h
  - userland/libc/src/netsock.c
leonos_net_ping:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - docs/unix-ipc-protocol.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - include/uapi/leonos/net_control.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - userland/libc/include/leonos/net_service.h
  - userland/libc/src/net_service.c
  - userland/libc/src/netsock.c
leonos_net_set_dns_policy:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - userland/libc/src/net_service.c
  - userland/libc/src/netsock.c
leonos_net_socket_close:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - kernel/ntclks/syscall_socket.c
  - userland/libc/include/leonos/net_service.h
leonos_net_socket_connect:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - userland/libc/include/leonos/net_service.h
  - userland/libc/src/libc.c
  - userland/libc/src/netsock.c
leonos_net_socket_io:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - kernel/ntclks/syscall_socket.c
  - tools/tests/socket_batch_unix_test.c
  - tools/tests/tcp_state_test.c
  - userland/libc/include/leonos/net_service.h
leonos_net_socket_open:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/components/tcc/runtime/include/leonos/net_service.h
  - devtools/include/leonos/net.h
  - devtools/include/leonos/net_service.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/net.h
  - include/leonos/net_service.h
  - kernel/ntclks/include/ntclks/net.h
  - kernel/ntclks/net.c
  - kernel/ntclks/syscall_socket.c
  - userland/libc/include/leonos/net_service.h
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
  - devtools/docs/PROGRAMS.md
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_spawn_argv_with_fds:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_termios:
  - devtools/components/tcc/runtime/include/leonos/pty.h
  - devtools/include/leonos/pty.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/LINUX_ABI_AUDIT_2026-09-07.md
  - include/leonos/pty.h
  - kernel/ntclks/include/ntclks/pty.h
  - kernel/ntclks/pty.c
  - tools/tests/linux_pty_test.c
leonos_pty_termios_io:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_termios_request:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_winsize:
  - devtools/components/tcc/runtime/include/leonos/pty.h
  - devtools/include/leonos/pty.h
  - docs/ABI_PRIVATE_INVENTORY.md
  - include/leonos/pty.h
  - kernel/ntclks/include/ntclks/pty.h
  - kernel/ntclks/pty.c
leonos_pty_winsize_io:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_pty_write_input:
  - docs/ABI_PRIVATE_INVENTORY.md
leonos_socket_close:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/net.h
  - userland/libc/src/libc.c
  - userland/libc/src/netsock.c
leonos_socket_connect:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/net.h
  - userland/libc/src/libc.c
  - userland/libc/src/netsock.c
leonos_socket_recv:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/net.h
  - userland/libc/src/libc.c
  - userland/libc/src/netsock.c
  - userland/libc/src/tls.c
leonos_socket_send:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/net.h
  - userland/libc/src/libc.c
  - userland/libc/src/netsock.c
  - userland/libc/src/tls.c
leonos_socket_tcp:
  - devtools/components/tcc/runtime/include/leonos/net.h
  - devtools/include/leonos/net.h
  - docs/ABI.md
  - docs/ABI_PRIVATE_INVENTORY.md
  - docs/SYSCALLS.md
  - include/leonos/net.h
  - userland/libc/src/libc.c
  - userland/libc/src/netsock.c
