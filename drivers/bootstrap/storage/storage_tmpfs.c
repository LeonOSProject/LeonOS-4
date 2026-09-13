int storage_tmpfs_get_page(const struct storage_node *node, uint64_t offset, uint64_t *phys)
{
    if (!node || !(node->flags & STORAGE_NODE_FLAG_TMPFS) || node->volume_id >= STORAGE_MAX_VOLUMES)
        return -22;
    uint64_t flags;
    kernel_execution_lock_irqsave(&flags);
    struct storage_volume *volume = &g_volumes[node->volume_id];
    int ret = volume->ready && volume->tmpfs
                  ? tmpfs_get_page(volume->tmpfs, node->first_cluster, offset, phys)
                  : -2;
    kernel_execution_unlock_irqrestore(flags);
    return ret;
}

int storage_mount_tmpfs(const char *source, const char *target, uint64_t mount_flags, const char *options,
                        uint32_t uid, uint32_t gid)
{
    if (!target || !source || storage_strlen(source) >= LEONOS_FS_PATH_LEN)
        return -22;
    uint64_t flags;
    kernel_execution_lock_irqsave(&flags);
    struct storage_node node;
    struct storage_volume *previous = g_active_volume;
    int ret = storage_lookup_path(target, &node);
    if (ret < 0)
        goto out;
    if (node.type != LEONOS_FS_TYPE_DIR) {
        ret = -20;
        goto out;
    }
    for (uint32_t i = 0; i < STORAGE_MAX_VOLUMES; ++i)
        if (g_volumes[i].ready && storage_text_eq(g_volumes[i].mount_path, target)) {
            ret = -16;
            goto out;
        }
    uint32_t slot = STORAGE_VOLUME_DYNAMIC_FIRST;
    while (slot < STORAGE_MAX_VOLUMES && g_volumes[slot].ready)
        ++slot;
    if (slot == STORAGE_MAX_VOLUMES) {
        ret = -28;
        goto out;
    }
    struct tmpfs_super *fs = NULL;
    ret = tmpfs_new(slot, mount_flags, options, uid, gid, &fs);
    if (ret < 0)
        goto out;
    struct storage_volume *volume = &g_volumes[slot];
    storage_memzero(volume, sizeof(*volume));
    volume->volume_id = slot;
    volume->kind = STORAGE_VOLUME_RAM;
    volume->filesystem = STORAGE_FILESYSTEM_TMPFS;
    volume->mount_flags = mount_flags;
    volume->tmpfs = fs;
    storage_copy_text(volume->mount_path, sizeof(volume->mount_path), target);
    storage_copy_text(volume->tmpfs_source, sizeof(volume->tmpfs_source), source[0] ? source : "none");
    volume->ready = true;
    storage_cache_invalidate();
out:
    storage_restore_volume(previous);
    kernel_execution_unlock_irqrestore(flags);
    return ret;
}
