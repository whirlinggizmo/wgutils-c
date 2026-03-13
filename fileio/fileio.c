#include "fileio.h"
#include "fileio_common.h"

static fileio_sync_op_t *fileio_sync_op_done(wg_op_kind_t kind, int status_code)
{
    fileio_sync_op_t *op = (fileio_sync_op_t *)calloc(1, sizeof(fileio_sync_op_t));

    if (!op)
    {
        return NULL;
    }

    wg_op_init(&op->op, kind, NULL, NULL);
    wg_op_complete(&op->op, status_code);
    return op;
}

// Wrapper for desktop using the common implementation
int fileio_init(const char *mount_point)
{
    // strip the leading slash, if there is one.  This makes it relative to cwd
    
    if (mount_point[0] == '/')
    {
        mount_point++;
    }
    
    return fileio_init_common(mount_point);
}

void fileio_deinit(void)
{
    fileio_deinit_common();
}

fileio_sync_op_t *fileio_restore_begin(void)
{
    return fileio_sync_op_done(WG_OP_KIND_FILEIO_RESTORE, 0);
}

fileio_sync_op_t *fileio_flush_begin(void)
{
    return fileio_sync_op_done(WG_OP_KIND_FILEIO_FLUSH, 0);
}

bool fileio_sync_poll(fileio_sync_op_t *op)
{
    return wg_op_is_done(op ? &op->op : NULL);
}

int fileio_sync_finish(fileio_sync_op_t *op)
{
    if (!op)
    {
        return -1;
    }

    if (!wg_op_is_done(&op->op))
    {
        return 1;
    }

    return wg_op_status(&op->op);
}

void fileio_sync_op_free(fileio_sync_op_t *op)
{
    if (!op)
    {
        return;
    }

    wg_op_deinit(&op->op);
    free(op);
}

int fileio_write(const char *filename, void *data, size_t size)
{
    return fileio_write_common(filename, data, size);
}

fileio_read_result_t fileio_read(const char *filename)
{
    return fileio_read_common(filename);
}

bool fileio_exists(const char *filename)
{
    return fileio_exists_common(filename);
}

int fileio_rmfile(const char *filename)
{
    return fileio_rmfile_common(filename);
}

int fileio_rmdir(const char *path)
{
    return fileio_rmdir_common(path);
}

int fileio_mkdir(const char *path)
{
    return fileio_mkdir_common(path);
}
