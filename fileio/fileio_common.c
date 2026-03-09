#include "fileio_common.h"
#include "fileio.h"
#include "logger/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include "fetch_url/fetch_url.h"

char fileio_mount_point[FILEIO_MAX_PATH_LENGTH];

bool fileio_mount_point_initialized = false;
static const int fileio_ready_wait_timeout_ms = 2000;

/**
 * Initializes the file I/O system.
 *
 * @param mount_point The directory to mount IDBFS at.
 * @return 0 on success, or -1 on failure.
 */

int fileio_init_common(const char *mount_point)
{
    // check to see if we've already initialized the mount point
    if (fileio_mount_point_initialized)
    {
        log_error("FILEIO: Mount point already initialized (%s).", fileio_mount_point);
        return -1;
    }

    // null check
    if (!mount_point)
    {
        log_error("FILEIO: Mount point is NULL.");
        return -1;
    }

    // the mount point can't be the root directory
    if (strcmp(mount_point, "/") == 0)
    {
        log_error("FILEIO: Mount point '%s' cannot be the root directory.", mount_point);
        return -1;
    }

    // make sure the mount point isn't too long (including the null terminator)
    if (strlen(mount_point) >= FILEIO_MAX_PATH_LENGTH - 1)
    {
        log_error("FILEIO: Mount point '%s' is too long.", mount_point);
        return -1;
    }

    // copy the mount point
    strncpy(fileio_mount_point, mount_point, FILEIO_MAX_PATH_LENGTH);
    fileio_mount_point[FILEIO_MAX_PATH_LENGTH - 1] = '\0';

    // mark the mount point as initialized
    fileio_mount_point_initialized = true;

    //
    bool ok = (mkdir(mount_point, 0777) == 0) || (errno == EEXIST);
    if (!ok)
    {
        log_error("FILEIO: Failed to create mount point directory '%s'.", mount_point);
        return -1;
    }

    log_info("FILEIO: Initialized file I/O with mount point '%s'.", mount_point);
    return 0;
}

void fileio_deinit_common(void)
{
    fileio_mount_point[0] = '\0';
    fileio_mount_point_initialized = false;
}

/**
 * Writes data to a file, ensuring that directories in the path exist.
 *
 * @param filename The name of the file to write to.
 * @param data The data to write to the file.
 * @return 0 on success, or -1 on failure.
 */
int fileio_write_common(const char *filename, void *data, size_t size)
{
    // make sure we've initialized the mount point
    if (!fileio_mount_point_initialized)
    {
        log_error("FILEIO: Mount point not initialized.");
        return -1;
    }
    if (!fileio_wait_for_ready(fileio_ready_wait_timeout_ms))
    {
        log_error("FILEIO: Timed out waiting for fileio readiness before write.");
        return -1;
    }
    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    // prepend the mount point to the filename
    snprintf(full_path, sizeof(full_path), "%s/%s", fileio_mount_point, filename);
    full_path[sizeof(full_path) - 1] = '\0';

    // Ensure the directory exists
    if (fileio_mkdir(filename) != 0) // fileio_mkdir() will prepend the mount point
    {
        return -1; // Failed to create directory
    }

    // Open the file for writing
    FILE *file = fopen(full_path, "wb");
    if (!file)
    {
        log_error("FILEIO: Failed to open file for writing: %s", full_path);
        return -1;
    }

    // Write data to the file
    if (fwrite(data, 1, size, file) != size)
    {
        log_error("FILEIO: Failed to write file: %s", full_path);
        fclose(file);
        return -1;
    }

    fclose(file);

    return 0;
}

/**
 * Reads data from a file.
 *
 * @param filename The name of the file to read from.
 * @return A dynamically allocated buffer containing the file contents, or NULL on failure.
 */
fileio_read_result_t fileio_read_common(const char *filename)
{
    fileio_read_result_t result = {0};

    // make sure we've initialized the mount point
    if (!fileio_mount_point_initialized)
    {
        log_error("FILEIO: Mount point not initialized.");
        result.error = -1;
        return result;
    }
    if (!fileio_wait_for_ready(fileio_ready_wait_timeout_ms))
    {
        log_error("FILEIO: Timed out waiting for fileio readiness before read.");
        result.error = -1;
        return result;
    }

    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    // prepend the mount point to the filename
    snprintf(full_path, sizeof(full_path), "%s/%s", fileio_mount_point, filename);
    full_path[sizeof(full_path) - 1] = '\0';

    FILE *file = fopen(full_path, "rb");
    if (!file)
    {
        log_warn("FILEIO: Failed to open file: %s", full_path);
        result.error = -1;
        return result;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        log_error("FILEIO: Failed to seek file: %s", full_path);
        fclose(file);
        result.error = -1;
        return result;
    }

    long file_size_long = ftell(file);
    if (file_size_long < 0)
    {
        log_error("FILEIO: Failed to get file size: %s", full_path);
        fclose(file);
        result.error = -1;
        return result;
    }

    size_t filesize = (size_t)file_size_long;
    if (filesize > (SIZE_MAX - 1))
    {
        log_error("FILEIO: File too large to allocate safely: %s", full_path);
        fclose(file);
        result.error = -1;
        return result;
    }
    rewind(file);

    unsigned char *buffer = (unsigned char *)malloc(filesize + 1);
    if (!buffer)
    {
        log_error("FILEIO: Memory allocation failed.");
        fclose(file);
        result.error = -1;
        return result;
    }

    if (fread(buffer, 1, filesize, file) != filesize)
    {
        log_error("FILEIO: Failed to read file: %s", full_path);
        free(buffer);
        fclose(file);
        result.error = -2;
        return result;
    }

    buffer[filesize] = '\0';
    fclose(file);
    result.data = buffer;
    result.size = filesize;
    return result;
}

fileio_read_result_t fileio_read_url_common(const char *host, const char *path, int timeout_ms) {
    log_debug("FILEIO: Fetching %s/%s", host, path);
    fetch_url_result_t result = fetch_url_with_path(host, path, timeout_ms);

    // if unsuccessful, return an empty fileio_read_result_t
    if (result.code != 200)
    {
        log_error("FILEIO: Failed to fetch file from %s/%s: %d", host, path, result.code);
        if (result.data)
        {
            free(result.data);
        }
        // return an empty fileio_read_result_t
        fileio_read_result_t read_result = {0};
        read_result.error = result.code;
        read_result.size = 0;
        read_result.data = NULL;
        return read_result;
    }

    // success!
    // write the file to disk
    if (fileio_write(path, result.data, result.size) != 0)
    {
        log_error("FILEIO: Failed to persist fetched file to cache: %s", path);
    }
    free(result.data);

    return fileio_read(path);
}

/**
 * Checks if a file exists.
 *
 * @param filename The name of the file to check.
 * @return true if the file exists, false otherwise.
 */
bool fileio_exists_common(const char *filename)
{
    // make sure we've initialized the mount point
    if (!fileio_mount_point_initialized)
    {
        log_error("FILEIO: Mount point not initialized.");
        return false;
    }

    // prepend the mount point to the filename
    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    snprintf(full_path, sizeof(full_path), "%s/%s", fileio_mount_point, filename);
    full_path[sizeof(full_path) - 1] = '\0';

    struct stat buffer;
    return (stat(full_path, &buffer) == 0);
}

/**
 * Creates directories recursively.
 *
 * @param path The directory path to create.
 * @return 0 on success, or -1 on failure.
 */
int fileio_mkdir_common(const char *path)
{
    // make sure we've initialized the mount point
    if (!fileio_mount_point_initialized)
    {
        log_error("FILEIO: Mount point not initialized.");
        return -1;
    }

    // prepend the mount point to the path
    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    snprintf(full_path, sizeof(full_path), "%s/%s", fileio_mount_point, path);
    full_path[sizeof(full_path) - 1] = '\0';

    // step through the path, creating directories as needed
    // we start at 1 to skip the first slash
    char *p = full_path;
    while ((p = strchr(p + 1, '/')) != NULL)
    {
        *p = '\0';
        if (mkdir(full_path, 0777) && errno != EEXIST)
        {
            log_error("FILEIO: Failed to create directory: %s", full_path);
            return -1;
        }
        *p = '/';
    }

    return 0;
}
