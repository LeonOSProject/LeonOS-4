#ifndef RELIEFOS_SYSCALL_H
#define RELIEFOS_SYSCALL_H

#include <reliefos/fs.h>
#include <reliefos/auth.h>
#include <reliefos/startup.h>
#include <reliefos/syscall_abi.h>
#include <linux/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define SYS_send SYS_sendto
#define SYS_recv SYS_recvfrom
#define SYS_nice RELIEFOS_SYS_NICE

#define RELIEFOS_PROT_READ LINUX_PROT_READ
#define RELIEFOS_PROT_WRITE LINUX_PROT_WRITE
#define RELIEFOS_PROT_EXEC LINUX_PROT_EXEC

#define RELIEFOS_MAP_PRIVATE LINUX_MAP_PRIVATE
#define RELIEFOS_MAP_FIXED LINUX_MAP_FIXED
#define RELIEFOS_MAP_ANONYMOUS LINUX_MAP_ANONYMOUS
#define RELIEFOS_MAP_FAILED ((void *)-1)

#define RELIEFOS_EPERM 1
#define RELIEFOS_EACCES 13
#define RELIEFOS_EBUSY 16
#define RELIEFOS_EIO 5
#define RELIEFOS_EAGAIN 11
#define RELIEFOS_EEXIST 17
#define RELIEFOS_EPIPE 32

long syscall0(long n);
long syscall1(long n, long a0);
long syscall2(long n, long a0, long a1);
long syscall3(long n, long a0, long a1, long a2);
long syscall6(long n, long a0, long a1, long a2, long a3, long a4, long a5);


int sleep_ms(unsigned long ms);
int reliefos_stat_legacy(const char *path, struct reliefos_stat *st);
int reliefos_fstat_legacy(int fd, struct reliefos_stat *st);

#endif
