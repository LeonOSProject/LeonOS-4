/*
 * LeonOS pseudo-terminal interface: declares PTY allocation and I/O helpers.
 * Connects terminal processes, shells, and the kernel's event streams.
 */
#ifndef NTCLKS_PTY_H
#define NTCLKS_PTY_H

#include <ntclks/types.h>
#include <ntclks/storage.h>
#include <leonos/pty.h>
#include <linux/tty.h>

int pty_lookup_path(const char *path, struct storage_node *node);
/** @brief Resolve one of the six fixed Linux virtual consoles.
 * @param path Device pathname, nullable.
 * @param node Optional output storage node.
 * @return Zero if resolved or negative ENOENT. */
int pty_lookup_vt_path(const char *path, struct storage_node *node);
int pty_get_node(uint32_t pty_id, struct storage_node *node);
int pty_inode_permissions(const struct storage_node *node,
                           struct leonos_permissions *value, bool write);

/**
 * @brief Initialize the pseudo-terminal subsystem and its backing storage.
 */
void pty_init(void);
/** @brief Reserve tty1 through tty6 before user processes start.
 * @return Zero on success, negative ENOMEM if allocation fails. */
int pty_vt_init(void);
/** @brief Resolve a fixed VT after initialization.
 * @param number Virtual console number.
 * @return Terminal identifier, or zero for an invalid/uninitialized VT. */
uint32_t pty_vt_id(uint32_t number);
/** @brief Resolve a terminal to a fixed VT.
 * @param pty_id Terminal identifier.
 * @return VT number or zero for a dynamic PTY. */
uint32_t pty_vt_number(uint32_t pty_id);
/** @brief Read the selected virtual console.
 * @return One through six, or zero before initialization. */
uint32_t pty_vt_active(void);
/** @brief Read display invalidation state under the execution transaction.
 * @return Generation incremented by visible VT switches and mode activations.
 */
uint64_t pty_vt_generation(void);
/** @brief Activate and repaint a VT under the execution transaction.
 * @param number Virtual console number.
 * @return Zero on success or negative EINVAL. */
int pty_vt_switch(uint32_t number);
/**
 * @brief Block until the selected VT is active; caller holds the execution lock.
 * @param number Fixed VT number, one through six.
 * @return Zero, negative EINVAL, or KERNEL_SYSCALL_BLOCKED for a pending wait.
 */
int64_t pty_vt_wait_active(uint32_t number);
/** @brief Change display mode under the execution transaction.
 * @param pty_id Fixed terminal identifier.
 * @param enabled One for graphics, zero for text.
 * @return Zero on success or negative EINVAL. */
int pty_vt_set_graphics(uint32_t pty_id, int enabled);
/** @brief Inspect the selected display mode.
 * @return Nonzero if the active VT is graphical. */
int pty_vt_graphical_active(void);
/** @brief Inspect one fixed virtual console.
 * @param pty_id Fixed terminal identifier.
 * @return Nonzero if graphical, zero if text or invalid. */
int pty_vt_graphical(uint32_t pty_id);
/**
 * @brief Offer one physical keyboard event to the console terminal.
 * @param keycode Set-1 make/break code after 0xe0 extension normalization.
 * @param pressed Non-zero for a make code, zero for a break code.
 *
 * Modifier tracking is unconditional; terminal bytes reach the active VT
 * only while its display mode is KD_TEXT.
 */
void pty_console_key_event(uint8_t keycode, uint8_t pressed);
/**
 * @brief Allocate a new PTY for owner_pid and return its id, or a negative error.
 */
int32_t pty_create(uint32_t owner_pid);
/**
 * @brief Tear down the PTY pty_id if it is owned by owner_pid; 0 on success.
 */
int pty_destroy(uint32_t owner_pid, uint32_t pty_id);
/**
 * @brief Return non-zero when owner_pid owns pty_id.
 */
int pty_is_owner(uint32_t pty_id, uint32_t owner_pid);
/**
 * @brief Return non-zero when pty_id refers to a live PTY.
 */
int pty_is_active(uint32_t pty_id);
/**
 * @brief Return non-zero when pty_id's master side has closed.
 */
int pty_is_hungup(uint32_t pty_id);
/** Return non-zero when the slave endpoint may be opened. */
int pty_slave_open_allowed(uint32_t pty_id);
/** Set or read the Unix98 PTY slave lock state. */
int pty_set_lock(uint32_t pty_id, int locked);
int pty_get_lock(uint32_t pty_id, int *locked);
/**
 * @brief Copy up to length bytes of pty_id's terminal output into buffer; returns bytes read.
 */
int64_t pty_read_output(uint32_t owner_pid, uint32_t pty_id, char *buffer, uint32_t length);
/**
 * @brief Feed up to length bytes of buffer into pty_id as terminal input; returns bytes accepted.
 */
int64_t pty_write_input(uint32_t owner_pid, uint32_t pty_id, const char *buffer, uint32_t length);
/**
 * @brief Copy up to length bytes of pty_id's pending input into buffer; returns bytes read.
 */
int64_t pty_read_input(uint32_t pty_id, char *buffer, uint32_t length);
/**
 * @brief Return how many bytes of input are buffered for pty_id.
 */
uint32_t pty_input_available(uint32_t pty_id);
/** Return bytes buffered from the PTY slave toward its master. */
uint32_t pty_output_available(uint32_t pty_id);
/**
 * @brief Write up to length bytes of buffer to pty_id's terminal output; returns bytes written.
 */
int64_t pty_write_output(uint32_t pty_id, const char *buffer, uint32_t length);
/**
 * @brief Copy pty_id's terminal mode settings into termios; 0 on success.
 */
int pty_get_termios(uint32_t pty_id, struct leonos_pty_termios *termios);
/**
 * @brief Apply the terminal mode settings in termios to pty_id; 0 on success.
 */
int pty_set_termios(uint32_t pty_id, const struct leonos_pty_termios *termios);
void pty_flush_input(uint32_t pty_id);
/**
 * @brief Copy pty_id's terminal window size into winsize; 0 on success.
 */
int pty_get_winsize(uint32_t pty_id, struct leonos_pty_winsize *winsize);
/**
 * @brief Set pty_id's terminal window size from winsize; 0 on success.
 */
int pty_set_winsize(uint32_t pty_id, const struct leonos_pty_winsize *winsize);
int pty_get_linux_winsize(uint32_t pty_id, struct linux_winsize *winsize);
int pty_set_linux_winsize(uint32_t pty_id, const struct linux_winsize *winsize);
/**
 * @brief Reads the foreground process group of a pseudo-terminal.
 * @param pty_id PTY identifier.
 * @param process_group Destination for the foreground process-group identifier.
 * @return Zero on success or a negative errno-style failure.
 */
int pty_get_foreground_pgid(uint32_t pty_id, uint32_t *process_group);
/**
 * @brief Changes the foreground process group of a pseudo-terminal.
 * @param pty_id PTY identifier.
 * @param caller_pid Attached process requesting the change.
 * @param process_group New foreground process-group identifier.
 * @return Zero on success or a negative errno-style failure.
 */
int pty_set_foreground_pgid(uint32_t pty_id, uint32_t caller_pid,
                            uint32_t process_group);
/**
 * @brief Apply Linux TIOCSCTTY attachment rules.
 * @param pty_id Active terminal ID.
 * @param caller_pid Calling thread ID.
 * @param steal Value 1 requests privileged terminal stealing.
 * @param readable Nonzero when the descriptor permits reads.
 * @return Zero on success or a negative Linux errno.
 */
int pty_acquire_controlling(uint32_t pty_id, uint32_t caller_pid, int steal, int readable);
int pty_get_session(uint32_t pty_id, uint32_t *session_id);
int pty_detach_controlling(uint32_t pty_id, uint32_t caller_pid);
void pty_open_controlling(uint32_t pty_id, uint32_t caller_pid, int readable);
/* signal_number 0 checks a write (TOSTOP), 21 a read, 22 a state change. */
int64_t pty_check_change(uint32_t pty_id, uint32_t caller_pid, int signal_number);
void pty_process_session_exit(uint32_t tgid);
/**
 * @brief Reclaim a hung-up PTY session when no descriptor still references it.
 */
void pty_reap_hungup(uint32_t pty_id);
int pty_transfer_get(uint32_t pty_id, uint32_t endpoint);
void pty_transfer_put(uint32_t pty_id, uint32_t endpoint);
uint32_t pty_transfer_count(uint32_t pty_id, int master_only);
/**
 * @brief Detach pid from any PTY it is attached to.
 */
void pty_process_exit(uint32_t pid);

#endif
