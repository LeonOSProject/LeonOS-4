#include <ntclks/syscall_internal.h>
#include <ntclks/syscall.h>
#include <ntclks/time.h>
#include <leonos/fs.h>
#include <linux/errno.h>

/* Linux caps a transfer at MAX_RW_COUNT, not at the transport's DMA size.
 * Keep the OFD and progress across the dispatcher's asynchronous retries. */
int64_t syscall_regular_io(struct task *task, struct task_file *file, uint64_t buffer, uint64_t count,
                           uint64_t position, bool writing, bool positional)
{
    uint64_t done = 0;
    uint64_t started = time_uptime_us();
    bool block = file->flags & TASK_FILE_FLAG_DEV_BLOCK;
    bool resumable =
        task->syscall_file == file &&
        (task->syscall_file_number == LINUX_SYS_READ || task->syscall_file_number == LINUX_SYS_WRITE ||
         task->syscall_file_number == LINUX_SYS_PREAD64 || task->syscall_file_number == LINUX_SYS_PWRITE64);
    count = count > 0x7ffff000u ? 0x7ffff000u : count;
    if (file->io_owner && file->io_owner != task->pid)
        return -LINUX_EAGAIN;
    if (resumable && task->regular_io.active) {
        buffer = task->regular_io.buffer;
        count = task->regular_io.count;
        position = task->regular_io.position;
        done = task->regular_io.done;
    } else {
        if (!positional)
            position = file->offset;
        if (!block && writing && (file->flags & LEONOS_O_APPEND)) {
            int ret = storage_inode_refresh(&file->node);
            if (ret < 0)
                return ret;
            position = file->node.size;
        }
        if (position > INT64_MAX || count > (uint64_t)INT64_MAX - position)
            return -LINUX_EINVAL;
        if (resumable) {
            task->regular_io.buffer = buffer;
            task->regular_io.count = count;
            task->regular_io.position = position;
            task->regular_io.done = 0;
            task->regular_io.active = true;
        }
    }
    if (!positional)
        file->io_owner = task->pid;
    int error = 0;
    while (done < count) {
        uint32_t length = count - done > 32768 ? 32768 : (uint32_t)(count - done);
        uint32_t transferred = 0;
        int ret;
        if (block) {
            uint32_t disk = STORAGE_BLOCK_DISK_ID(file->node.volume_id);
            int32_t part = STORAGE_BLOCK_PARTITION(file->node.volume_id);
            ret = writing ? storage_disk_block_write(disk, part, position + done,
                                                     (const void *)(uintptr_t)(buffer + done), length,
                                                     &transferred)
                          : storage_disk_block_read(disk, part, position + done,
                                                    (void *)(uintptr_t)(buffer + done), length, &transferred);
        } else if (writing) {
            file->read_cursor.valid = 0;
            ret =
                file->inode
                    ? storage_write_held_node(&file->node, position + done,
                                              (const void *)(uintptr_t)(buffer + done), length, &transferred)
                    : storage_write_node(file->path, position + done,
                                         (const void *)(uintptr_t)(buffer + done), length, &transferred);
        } else {
            ret = storage_read_node_cursor(&file->node, position + done, (void *)(uintptr_t)(buffer + done),
                                           length, &transferred, &file->read_cursor);
        }
        if (transferred > length) {
            error = -LINUX_EIO;
            break;
        }
        done += transferred;
        if (!positional)
            file->offset = position + done;
        if (!block && writing && transferred) {
            if (file->node.first_cluster < 2) {
                struct storage_node updated;
                if (!storage_lookup_path(file->path, &updated))
                    file->node = updated;
            }
            if (position + done > file->node.size)
                file->node.size = position + done;
        }
        if (resumable)
            task->regular_io.done = done;
        if (ret == -LINUX_EAGAIN && resumable)
            return ret;
        if (ret < 0) {
            error = ret;
            break;
        }
        if (transferred < length)
            break;
        /* Yield only after a completed storage operation, with no DMA pending.
         * The dispatcher resumes this blocking syscall with its saved progress. */
        if (resumable && done < count && time_uptime_us() - started >= 2000) {
            storage_release_task_io(task->pid);
            return -LINUX_EAGAIN;
        }
    }
    if (file->io_owner == task->pid)
        file->io_owner = 0;
    if (resumable)
        task->regular_io.active = false;
    return done ? (int64_t)done : error;
}
