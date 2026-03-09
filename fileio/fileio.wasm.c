#ifdef EMSCRIPTEN

#include "fileio.h"
#include "fileio_common.h"
#include "logger/log.h"
#include <emscripten.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
//#include <errno.h>
#include <sys/stat.h>

static void sync_to_idbfs(bool populate_from_idbfs, bool set_ready_on_success);

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

    // Restore the in-memory FS view from persisted IDBFS state.
    sync_to_idbfs(true, true);

    return 0;
}

void fileio_deinit(void)
{
    // Mark not-ready immediately so callers do not assume cache restore state
    // remains valid after teardown starts.
    EM_ASM({
        Module.fileio_idbfs_ready = false;
    });

    // Best-effort flush of in-memory FS changes to IDBFS.
    sync_to_idbfs(false, false);
    fileio_deinit_common();
}

bool fileio_wait_for_ready(int timeout_ms)
{
    int waited_ms = 0;
    const int poll_ms = 16;
    if (timeout_ms < 0) {
        timeout_ms = 0;
    }

    while (waited_ms <= timeout_ms)
    {
        int ready = EM_ASM_INT({
            return !!(Module && Module.fileio_idbfs_ready);
        });
        if (ready != 0) {
            return true;
        }

        if (waited_ms == timeout_ms) {
            break;
        }

        emscripten_sleep(poll_ms);
        waited_ms += poll_ms;
        if (waited_ms > timeout_ms) {
            waited_ms = timeout_ms;
        }
    }

    return false;
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
                console.error((populate ? "Error restoring from IDBFS: " : "Error syncing to IDBFS: ") + err);
                if (set_ready) {
                    Module.fileio_idbfs_ready = false;
                }
            } else if (set_ready) {
                Module.fileio_idbfs_ready = true;
            }
            Module.fileio_idbfs_syncing = false;
        });
    }, populate_from_idbfs ? 1 : 0, set_ready_on_success ? 1 : 0);
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

fileio_read_result_t fileio_read_url(const char *host, const char *path, int timeout_ms) {
    return fileio_read_url_common(host, path, timeout_ms);
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

#endif
