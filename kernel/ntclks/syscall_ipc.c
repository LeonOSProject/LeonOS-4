/*
 * LeonOS IPC syscall support: anonymous pipes and descriptor endpoints.
 */
#include <ntclks/syscall.h>
#include <ntclks/syscall_internal.h>
#include <ntclks/sched.h>
#include <ntclks/usercopy.h>
#include <ntclks/object.h>
#include <ntclks/heap.h>
#include <ntclks/wait.h>
#include <ntclks/futex.h>
#include <leonos/fs_abi.h>

/* A 64-stage shell pipeline owns 63 pipes simultaneously.  Keep an extra
 * ring sentinel byte so the advertised 4096-byte capacity is usable. */
#define TASK_PIPE_MAX 256u
#define TASK_PIPE_CAP 4096u
#define TASK_PIPE_RING_CAP (TASK_PIPE_CAP + 1u)

struct task_pipe {
    uint8_t used;
    uint8_t reserved[3];
    uint32_t readers;
    uint32_t writers;
    uint32_t reader_generation, writer_generation;
    uint32_t handle;
    bool named;
    struct storage_node node;
    uint32_t head;
    uint32_t tail;
    uint8_t data[TASK_PIPE_RING_CAP];
    struct kernel_wait_queue wait_read;
    struct kernel_wait_queue wait_write;
};

static struct task_pipe *task_pipes[TASK_PIPE_MAX];

static struct task_pipe *task_pipe_for_file(const struct task_file *file)
{
    if (!file || !(file->flags & TASK_FILE_FLAG_PIPE)) {
        return NULL;
    }
    return (struct task_pipe *)kernel_object_lookup(kernel_objects(), file->aux,
                                                    KERNEL_OBJECT_PIPE);
}

/**
 * @brief Acquire read/write endpoint counts for a newly owned description.
 * @param file Pipe description under the kernel execution lock.
 */
void task_pipe_retain(struct task_file *file)
{
    struct task_pipe *pipe = task_pipe_for_file(file);
    if (!pipe) return;
    if ((file->flags & LEONOS_O_ACCMODE) != LEONOS_O_RDONLY) {
        ++pipe->writers;
        ++pipe->writer_generation;
    }
    if ((file->flags & LEONOS_O_ACCMODE) != LEONOS_O_WRONLY) {
        ++pipe->readers;
        ++pipe->reader_generation;
    }
}

/**
 * @brief Release endpoint counts, wake peers and reclaim the final pipe object.
 * @param file Owned pipe description under the kernel execution lock.
 */
void task_pipe_release(struct task_file *file)
{
    struct task_pipe *pipe = task_pipe_for_file(file);
    if (!pipe) return;
    if ((file->flags & LEONOS_O_ACCMODE) != LEONOS_O_RDONLY) {
        if (pipe->writers) --pipe->writers;
        if (!pipe->writers) (void)kernel_wait_queue_wake_all(&pipe->wait_read);
    }
    if ((file->flags & LEONOS_O_ACCMODE) != LEONOS_O_WRONLY && pipe->readers) {
        --pipe->readers;
        if (!pipe->readers) (void)kernel_wait_queue_wake_all(&pipe->wait_write);
    }
    if (!pipe->readers && !pipe->writers) {
        void *removed = NULL;
        (void)kernel_wait_queue_wake_all(&pipe->wait_read);
        (void)kernel_wait_queue_wake_all(&pipe->wait_write);
        kernel_object_remove(kernel_objects(), file->aux, KERNEL_OBJECT_PIPE, &removed);
        if (removed) {
            for (uint32_t i = 0; i < TASK_PIPE_MAX; ++i) {
                if (task_pipes[i] == (struct task_pipe *)removed) {
                    task_pipes[i] = NULL;
                    break;
                }
            }
            kernel_free(removed);
        }
    }
}

static int alloc_task_pipe_fd(struct task *task, uint32_t pipe_handle, int write_end)
{
    if (!task || !kernel_object_lookup(kernel_objects(), pipe_handle, KERNEL_OBJECT_PIPE))
        return -LEONOS_EINVAL;
    struct task_file *file;
    int fd = task_allocate_fd(task, 0, &file);
    if (fd < 0) return fd;
    file->flags = TASK_FILE_FLAG_PIPE | (write_end ? TASK_FILE_FLAG_PIPE_WRITE : 0) |
                  (write_end ? LEONOS_O_WRONLY : LEONOS_O_RDONLY);
    file->aux = pipe_handle;
    task_pipe_retain(file);
    return fd;
}

/**
 * @brief Read available pipe bytes, distinguishing EOF and a waiting writer.
 * @param file Pinned description under the kernel execution lock.
 * @param buffer Validated writable destination for length bytes.
 * @param length Maximum byte count.
 * @return Byte count, negative errno, or an interruptible retry.
 */
int task_pipe_read(struct task_file *file, void *buffer, uint32_t length)
{
    struct task_pipe *pipe = task_pipe_for_file(file);
    uint32_t count = 0;
    if (!pipe || (file->flags & LEONOS_O_ACCMODE) == LEONOS_O_WRONLY) return -LEONOS_EBADF;
    if (length == 0) return 0;
    kernel_wait_queue_remove(&pipe->wait_read, sched_current_task());
    while (pipe->tail != pipe->head && count < length) {
        ((uint8_t *)buffer)[count++] = pipe->data[pipe->tail];
        pipe->tail = (pipe->tail + 1U) % TASK_PIPE_RING_CAP;
    }
    if (count) {
        (void)kernel_wait_queue_wake_all(&pipe->wait_write);
        return (int)count;
    }
    if (!pipe->writers) return 0;
    if (file->flags & LEONOS_O_NONBLOCK) return -LEONOS_EAGAIN;
    kernel_wait_queue_block_current(&pipe->wait_read);
    return -LEONOS_EAGAIN;
}

/**
 * @brief Write pipe data atomically up to PIPE_BUF and signal a missing reader.
 * @param file Pinned description under the kernel execution lock.
 * @param buffer Validated source for length bytes.
 * @param length Requested byte count.
 * @return Byte count or negative errno, including EPIPE and EAGAIN.
 */
int task_pipe_write(struct task_file *file, const void *buffer, uint32_t length)
{
    struct task_pipe *pipe = task_pipe_for_file(file);
    uint32_t count = 0;
    if (!pipe || (file->flags & LEONOS_O_ACCMODE) == LEONOS_O_RDONLY) return -LEONOS_EBADF;
    if (length == 0) return 0;
    if (!pipe->readers) {
        (void)sched_signal_user_task(sched_current_pid(), 13); /* SIGPIPE */
        return -LEONOS_EPIPE;
    }
    kernel_wait_queue_remove(&pipe->wait_write, sched_current_task());
    {
        uint32_t used = (pipe->head + TASK_PIPE_RING_CAP - pipe->tail) % TASK_PIPE_RING_CAP;
        uint32_t free_bytes = TASK_PIPE_CAP - used;
        /* Linux guarantees that writes up to PIPE_BUF are atomic. */
        if (length <= TASK_PIPE_CAP && free_bytes < length) {
            if (file->flags & LEONOS_O_NONBLOCK) return -LEONOS_EAGAIN;
            kernel_wait_queue_block_current(&pipe->wait_write);
            return -LEONOS_EAGAIN;
        }
    }
    while (count < length) {
        uint32_t next = (pipe->head + 1U) % TASK_PIPE_RING_CAP;
        if (next == pipe->tail) {
            if (count) {
                (void)kernel_wait_queue_wake_all(&pipe->wait_read);
                return (int)count;
            }
            if (file->flags & LEONOS_O_NONBLOCK) return -LEONOS_EAGAIN;
            kernel_wait_queue_block_current(&pipe->wait_write);
            return -LEONOS_EAGAIN;
        }
        pipe->data[pipe->head] = ((const uint8_t *)buffer)[count++];
        pipe->head = next;
    }
    (void)kernel_wait_queue_wake_all(&pipe->wait_read);
    return (int)count;
}

/**
 * @brief Report endpoint readiness and FIFO writer-generation HUP semantics.
 * @param file Pinned description under the kernel execution lock.
 * @param events Requested poll events.
 * @return Ready bits and unconditional HUP/ERR/NVAL notifications.
 */
short task_pipe_poll(const struct task_file *file, short events)
{
    const struct task_pipe *pipe = task_pipe_for_file(file);
    short result = 0;
    if (!pipe) {
        return POLLNVAL;
    }
    if ((file->flags & LEONOS_O_ACCMODE) != LEONOS_O_RDONLY) {
        if (pipe->readers == 0) {
            result |= POLLERR;
        } else if (events & POLLOUT) {
            uint32_t next = (pipe->head + 1U) % TASK_PIPE_RING_CAP;
            if (next != pipe->tail) {
                result |= POLLOUT;
            }
        }
    }
    if ((file->flags & LEONOS_O_ACCMODE) != LEONOS_O_WRONLY) {
        if (pipe->tail != pipe->head) result |= events & POLLIN;
        if (pipe->writers == 0 && (!pipe->named || file->aux2 != pipe->writer_generation)) {
            result |= POLLHUP;
        }
    }
    return result;
}

/**
 * @brief Open or resume a FIFO using Linux partner-generation rendezvous.
 * @param task Current task with the execution lock held.
 * @param node FIFO identity, NULL only on retry of an existing open.
 * @param flags Linux open flags; O_PATH must be handled by the caller.
 * @param path Canonical path for descriptor diagnostics.
 * @return Published descriptor, negative errno, or KERNEL_SYSCALL_BLOCKED while waiting for a partner.
 */
int task_fifo_open(struct task *task, const struct storage_node *node,
                   uint32_t flags, const char *path)
{
    if (!task) return -LEONOS_ESRCH;
    struct task_file *file = task->fifo_open_file;
    struct task_pipe *pipe = file ? task_pipe_for_file(file) : NULL;
    if (!file) {
        if (!node) return -LEONOS_EINVAL;
        uint32_t index = TASK_PIPE_MAX;
        for (uint32_t i = 0; i < TASK_PIPE_MAX; ++i) {
            if (!task_pipes[i]) { if (index == TASK_PIPE_MAX) index = i; continue; }
            struct task_pipe *candidate = task_pipes[i];
            if (candidate->named && candidate->node.volume_id == node->volume_id &&
                candidate->node.first_cluster == node->first_cluster && candidate->node.flags == node->flags) {
                pipe = candidate;
                break;
            }
        }
        uint32_t access = flags & LEONOS_O_ACCMODE;
        if (access == LEONOS_O_ACCMODE) return -LEONOS_EINVAL;
        if (access == LEONOS_O_WRONLY && (flags & LEONOS_O_NONBLOCK) && (!pipe || !pipe->readers))
            return -6; /* Linux ENXIO, without publishing any endpoint. */
        if (!pipe && index == TASK_PIPE_MAX) return -LEONOS_ENFILE;
        file = kernel_malloc(sizeof(*file));
        if (!file) return -LEONOS_ENOMEM;
        *file = (struct task_file){.used = 1, .node = *node, .flags = flags | TASK_FILE_FLAG_PIPE,
            .fd_flags = (flags & LEONOS_O_CLOEXEC) ? LEONOS_FD_CLOEXEC : 0};
        if (access != LEONOS_O_RDONLY) file->flags |= TASK_FILE_FLAG_PIPE_WRITE;
        int ret = storage_inode_get(node, &file->inode);
        if (ret < 0) { kernel_free(file); return ret; }
        if (!pipe) {
            pipe = kernel_malloc(sizeof(*pipe));
            if (!pipe) { (void)storage_inode_put(file->inode); kernel_free(file); return -LEONOS_ENOMEM; }
            *pipe = (struct task_pipe){.used = 1, .named = true, .node = *node};
            kernel_wait_queue_init(&pipe->wait_read);
            kernel_wait_queue_init(&pipe->wait_write);
            pipe->handle = kernel_object_insert(kernel_objects(), pipe, KERNEL_OBJECT_PIPE);
            if (!pipe->handle) {
                kernel_free(pipe); (void)storage_inode_put(file->inode); kernel_free(file);
                return -LEONOS_ENFILE;
            }
            task_pipes[index] = pipe;
        }
        file->aux = pipe->handle;
        /* A nonblocking reader suppresses HUP until a writer has existed. */
        file->aux2 = access == LEONOS_O_WRONLY ? pipe->reader_generation : pipe->writer_generation;
        if (access == LEONOS_O_RDONLY && pipe->writers) file->aux2 = pipe->writer_generation - 1u;
        if (path) {
            uint32_t i = 0;
            for (; path[i] && i + 1 < sizeof(file->path); ++i) file->path[i] = path[i];
            file->path[i] = 0;
        }
        task_pipe_retain(file);
        task->fifo_open_file = file;
        (void)kernel_wait_queue_wake_all(&pipe->wait_read);
    }
    uint32_t access = file->flags & LEONOS_O_ACCMODE;
    if (!(file->flags & LEONOS_O_NONBLOCK) &&
        ((access == LEONOS_O_RDONLY && !pipe->writers && file->aux2 == pipe->writer_generation) ||
         (access == LEONOS_O_WRONLY && !pipe->readers && file->aux2 == pipe->reader_generation))) {
        /* This is a pipe rendezvous, not pending disk I/O. The blocked
         * sentinel releases the storage transaction before scheduling. */
        kernel_wait_queue_block_current(&pipe->wait_read);
        return KERNEL_SYSCALL_BLOCKED;
    }
    kernel_wait_queue_remove(&pipe->wait_read, task);
    struct task_file *destination;
    int fd = task_allocate_fd(task, 0, &destination);
    if (fd < 0) { task_fifo_cancel(task); return fd; }
    *destination = *file;
    task->fifo_open_file = NULL;
    kernel_free(file);
    return fd;
}

int syscall_ipc_pipe(uint64_t user_ptr)
{
    struct task *task = sched_current_task();
    int read_fd, write_fd;
    uint32_t pipe_index;
    uint32_t pipe_handle;
    if (!task || !user_range_writable(user_ptr, sizeof(int) * 2U)) {
        return -LEONOS_EFAULT;
    }
    for (pipe_index = 0; pipe_index < TASK_PIPE_MAX; ++pipe_index) {
        if (!task_pipes[pipe_index]) {
            break;
        }
    }
    if (pipe_index == TASK_PIPE_MAX) {
        return -LEONOS_EMFILE;
    }
    task_pipes[pipe_index] = (struct task_pipe *)kernel_malloc(sizeof(struct task_pipe));
    if (!task_pipes[pipe_index]) {
        return -LEONOS_ENOMEM;
    }
    *task_pipes[pipe_index] = (struct task_pipe){.used = 1};
    kernel_wait_queue_init(&task_pipes[pipe_index]->wait_read);
    kernel_wait_queue_init(&task_pipes[pipe_index]->wait_write);
    pipe_handle = kernel_object_insert(kernel_objects(), task_pipes[pipe_index],
                                       KERNEL_OBJECT_PIPE);
    if (!pipe_handle) {
        kernel_free(task_pipes[pipe_index]);
        task_pipes[pipe_index] = NULL;
        return -LEONOS_EMFILE;
    }
    read_fd = alloc_task_pipe_fd(task, pipe_handle, 0);
    write_fd = alloc_task_pipe_fd(task, pipe_handle, 1);
    if (read_fd < 0 || write_fd < 0) {
        if (read_fd >= 0) {
            task_discard_file_fd(task, read_fd);
        }
        if (write_fd >= 0) {
            task_discard_file_fd(task, write_fd);
        }
        kernel_object_remove(kernel_objects(), pipe_handle, KERNEL_OBJECT_PIPE, NULL);
        kernel_free(task_pipes[pipe_index]);
        task_pipes[pipe_index] = NULL;
        return -LEONOS_EMFILE;
    }
    ((int *)(uintptr_t)user_ptr)[0] = read_fd;
    ((int *)(uintptr_t)user_ptr)[1] = write_fd;
    return 0;
}

int syscall_ipc_pipe2(uint64_t user_ptr, uint64_t flags)
{
    flags = (uint32_t)flags;
    struct task *task = sched_current_task();
    int fds[2];
    struct task_file *read_file;
    struct task_file *write_file;
    if (flags & ~(uint32_t)(LEONOS_O_NONBLOCK | LEONOS_O_CLOEXEC)) {
        return -LEONOS_EINVAL;
    }
    if (!task || !user_range_writable(user_ptr, sizeof(fds))) return -LEONOS_EFAULT;
    int result = syscall_ipc_pipe(user_ptr);
    if (result < 0 || !task) return result;
    fds[0] = ((const int *)(uintptr_t)user_ptr)[0];
    fds[1] = ((const int *)(uintptr_t)user_ptr)[1];
    read_file = task_file_for_fd(task, fds[0]);
    write_file = task_file_for_fd(task, fds[1]);
    if (!read_file || !write_file) return -LEONOS_EBADF;
    if (flags & LEONOS_O_NONBLOCK) {
        read_file->flags |= LEONOS_O_NONBLOCK;
        write_file->flags |= LEONOS_O_NONBLOCK;
    }
    if (flags & LEONOS_O_CLOEXEC) {
        read_file->fd_flags |= LEONOS_FD_CLOEXEC;
        write_file->fd_flags |= LEONOS_FD_CLOEXEC;
    }
    return 0;
}

int syscall_ipc_owns(uint64_t number)
{
    switch (number) {
    case LINUX_SYS_MSGGET:
    case LINUX_SYS_SEMGET:
    case LINUX_SYS_SEMOP:
    case LINUX_SYS_SEMCTL:
    case LINUX_SYS_SEMTIMEDOP:
    case LINUX_SYS_MSGSND:
    case LINUX_SYS_MSGRCV:
    case LINUX_SYS_MSGCTL:
    case LINUX_SYS_PIPE:
    case LINUX_SYS_PIPE2:
    case LINUX_SYS_DUP:
    case LINUX_SYS_DUP2:
    case LINUX_SYS_FORK:
    case LINUX_SYS_VFORK:
    case LINUX_SYS_EXECVE:
    case LINUX_SYS_EXIT:
    case LINUX_SYS_WAIT4:
        return 1;
    default:
        return 0;
    }
}

int64_t syscall_ipc_dispatch(uint64_t number, uint64_t a0, uint64_t a1,
                             uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
    if (number == LINUX_SYS_PIPE2) return syscall_ipc_pipe2(a0, a1);
    if ((number >= LINUX_SYS_SEMGET && number <= LINUX_SYS_SEMCTL) || number == LINUX_SYS_SEMTIMEDOP)
        return syscall_sysv_sem(number, a0, a1, a2, a3);
    if (number >= LINUX_SYS_MSGGET && number <= LINUX_SYS_MSGCTL)
        return syscall_sysv_msg(number, a0, a1, a2, a3, a4);
    return syscall_dispatch_regs_legacy(number, a0, a1, a2, a3, a4, a5);
}
