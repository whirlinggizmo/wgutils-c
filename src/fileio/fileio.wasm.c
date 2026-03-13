#ifdef EMSCRIPTEN

#include "fileio.h"
#include "fileio_common.h"
#include "logger/logger.h"
#include <emscripten.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
//#include <errno.h>
#include <sys/stat.h>

static void sync_to_idbfs(bool populate_from_idbfs, bool set_ready_on_success);
static fileio_sync_op_t *fileio_sync_begin_internal(bool populate_from_idbfs);

int fileio_init(const char *mount_point)
{
    // create a local copy of the mount point so we can modify it
    char _mount_point[FILEIO_MAX_PATH_LENGTH];

    // add a preceding slash if the mount point doesn't start with a slash (for IDBFS)
    if (mount_point[0] != '/')
    {
        snprintf(_mount_point, sizeof(_mount_point), "/%s", mount_point);
    }
    else
    {
        snprintf(_mount_point, sizeof(_mount_point), "%s", mount_point);
    }

    int ok = fileio_init_common(_mount_point);
    if (ok != 0)
    {
        return ok;
    }

    // Mount IDBFS at the specified path
    EM_ASM({
        Module.fileio_idbfs_ready = false;
        if (typeof Module.fileio_idbfs_syncing === "undefined") {
            Module.fileio_idbfs_syncing = false;
        }
        FS.mount(IDBFS, { autoPersist: true }, UTF8ToString($0));
    }, fileio_mount_point);

    return 0;
}

void fileio_deinit(void)
{
    // Mark not-ready immediately so callers do not assume cache restore state
    // remains valid after teardown starts.
    EM_ASM({
        Module.fileio_idbfs_ready = false;
    });

    fileio_deinit_common();
}

fileio_sync_op_t *fileio_restore_async(void)
{
    return fileio_sync_begin_internal(true);
}

fileio_sync_op_t *fileio_flush_async(void)
{
    return fileio_sync_begin_internal(false);
}

bool fileio_sync_poll(fileio_sync_op_t *op)
{
    if (!op)
    {
        return false;
    }

    if (wg_op_is_done(&op->op))
    {
        return true;
    }

    if (EM_ASM_INT({
        return !!(Module && Module.fileio_idbfs_syncing);
    }) == 0)
    {
        int sync_ok = EM_ASM_INT({
            return !!(Module && Module.fileio_idbfs_last_sync_ok);
        });
        wg_op_complete(&op->op, sync_ok ? 0 : -1);
    }

    return wg_op_is_done(&op->op);
}

int fileio_sync_finish(fileio_sync_op_t *op)
{
    if (!op)
    {
        return -1;
    }

    if (!fileio_sync_poll(op))
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

/**
 * Checks if the given path is a mount point.
 *
 * @param path The directory path to check.
 * @return true if the path is a mount point, false otherwise.
 */
bool is_mount_point(const char *path)
{
    // ensure we have initialized the mount point
    if (!fileio_mount_point_initialized)
    {
        log_error("FILEIO: Mount point not initialized.");
        return false;
    }

    if (strcmp(path, fileio_mount_point) == 0)
    {
        return true;
    }

    return false;

    /*
      return EM_ASM_INT({
          try {
              return FS.lookupPath(UTF8ToString($0), { follow_mount: false }).node.isMountpoint ? 1 : 0;
          } catch (e) {
              return 0;
          } }, path);
          */
}

/**
 * Syncs the current state of the file system to persistent storage.
 */
static void sync_to_idbfs(bool populate_from_idbfs, bool set_ready_on_success)
{
    int started = EM_ASM_INT({
        if (Module.fileio_idbfs_syncing) return 0;
        Module.fileio_idbfs_syncing = true;
        return 1;
    });

    if (!started)
    {
        return;
    }

    EM_ASM({
        var populate = ($0 != 0);
        var set_ready = ($1 != 0);
        FS.syncfs(populate, function(err) {
            if (err) {
                Module.fileio_idbfs_last_sync_ok = false;
                console.error((populate ? "Error restoring from IDBFS: " : "Error syncing to IDBFS: ") + err);
                if (set_ready) {
                    Module.fileio_idbfs_ready = false;
                }
            } else {
                Module.fileio_idbfs_last_sync_ok = true;
                if (set_ready) {
                    Module.fileio_idbfs_ready = true;
                }
            }
            Module.fileio_idbfs_syncing = false;
        });
    }, populate_from_idbfs ? 1 : 0, set_ready_on_success ? 1 : 0);
}

static fileio_sync_op_t *fileio_sync_begin_internal(bool populate_from_idbfs)
{
    fileio_sync_op_t *op = (fileio_sync_op_t *)calloc(1, sizeof(fileio_sync_op_t));

    if (!op)
    {
        return NULL;
    }

    wg_op_init(&op->op,
               populate_from_idbfs ? WG_OP_KIND_FILEIO_RESTORE : WG_OP_KIND_FILEIO_FLUSH,
               NULL,
               NULL);
    EM_ASM({
        Module.fileio_idbfs_last_sync_ok = true;
    });
    sync_to_idbfs(populate_from_idbfs, populate_from_idbfs);
    return op;
}

/**
 * Creates directories recursively. If the first component is not already mounted,
 * automatically mounts IDBFS at that location.
 *
 * @param path The full directory path to create.
 * @return 0 on success, or -1 on failure.
 */
int fileio_mkdir(const char *path)
{
    return fileio_mkdir_common(path);
}

/**
 * Writes data to a file, ensuring that directories in the path exist and
 * that the first component is mounted if necessary.
 *
 * @param filename The name of the file to write to.
 * @param data The data to write to the file.
 * @return 0 on success, or -1 on failure.
 */
int fileio_write(const char *filename, void *data, size_t size)
{
    return fileio_write_common(filename, data, size);
}

/**
 * Reads data from a file, ensuring the latest data is synced from persistent storage.
 *
 * @param filename The name of the file to read from.
 * @return A dynamically allocated buffer containing the file contents, or NULL on failure.
 */
fileio_read_result_t fileio_read(const char *filename)
{
    // Use the shared file_read_common implementation
    return fileio_read_common(filename);
}

/**
 * Checks if a file exists.
 *
 * @param filename The name of the file to check.
 * @return true if the file exists, false otherwise.
 */
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

#endif
