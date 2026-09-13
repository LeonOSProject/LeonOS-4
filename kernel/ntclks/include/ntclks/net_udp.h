#ifndef NTCLKS_NET_UDP_H
#define NTCLKS_NET_UDP_H
#include <ntclks/types.h>
struct task_file;
#define TASK_FILE_KIND_UDP 2u
int64_t syscall_udp(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2,
                    uint64_t a3, uint64_t a4, uint64_t a5);
int task_udp_send(struct task_file *file, const void *data, uint32_t length,
                   uint32_t flags, uint64_t address, uint32_t address_length);
int task_udp_recv(struct task_file *file, void *data, uint32_t length,
                   uint32_t flags, uint64_t address, uint64_t address_length);
short task_udp_poll(struct task_file *file, short events);
void task_udp_release(struct task_file *file);
int task_udp_available(struct task_file *file);
void net_udp_input(uint32_t source, uint32_t destination, uint16_t source_port,
                    uint16_t destination_port, const void *data, uint32_t length);
#endif
