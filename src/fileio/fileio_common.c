#include "fileio_common.h"
#include "fileio.h"
#include "logger/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <ctype.h>

char fileio_mount_point[FILEIO_MAX_PATH_LENGTH];

bool fileio_mount_point_initialized = false;

static int fileio_normalize_relpath(const char *input, char *output, size_t output_size, bool allow_empty)
{
    char temp[FILEIO_MAX_PATH_LENGTH * 2];
    size_t marks[FILEIO_MAX_PATH_LENGTH];
    size_t depth = 0;
    size_t out_len = 0;
    const char *p = NULL;

    if (!input || !output || output_size == 0)
    {
        return -1;
    }

    if (strlen(input) >= sizeof(temp))
    {
        return -1;
    }

    strcpy(temp, input);

    // Absolute paths are not allowed in fileio relative-path APIs.
    if (temp[0] == '/' || temp[0] == '\\')
    {
        return -1;
    }

    // Block Windows drive forms (e.g. C:\, C:/, C:foo) and colon usage.
    if ((isalpha((unsigned char)temp[0]) && temp[1] == ':') || strchr(temp, ':') != NULL)
    {
        return -1;
    }

    // Normalize separators to a single style before lexical normalization.
    for (size_t i = 0; temp[i] != '\0'; i++)
    {
        if (temp[i] == '\\')
        {
            temp[i] = '/';
        }
    }

    output[0] = '\0';
    p = temp;

    while (*p != '\0')
    {
        const char *start = NULL;
        size_t seg_len = 0;

        while (*p == '/')
        {
            p++;
        }
        if (*p == '\0')
        {
            break;
        }

        start = p;
        while (*p != '\0' && *p != '/')
        {
            p++;
        }
        seg_len = (size_t)(p - start);

        if (seg_len == 0 || (seg_len == 1 && start[0] == '.'))
        {
            continue;
        }

        if (seg_len == 2 && start[0] == '.' && start[1] == '.')
        {
            if (depth == 0)
            {
                return -1;
            }
            depth--;
            out_len = marks[depth];
            output[out_len] = '\0';
            continue;
        }

        if (depth >= FILEIO_MAX_PATH_LENGTH)
        {
            return -1;
        }

        marks[depth] = out_len;
        depth++;

        if (out_len != 0)
        {
            if (out_len + 1 >= output_size)
            {
                return -1;
            }
            output[out_len++] = '/';
        }

        if (out_len + seg_len >= output_size)
        {
            return -1;
        }

        memcpy(output + out_len, start, seg_len);
        out_len += seg_len;
        output[out_len] = '\0';
    }

    if (!allow_empty && out_len == 0)
    {
        return -1;
    }

    return 0;
}

static int fileio_build_full_path(const char *path, char *full_path, size_t full_path_size, bool allow_empty_relpath)
{
    char normalized[FILEIO_MAX_PATH_LENGTH * 2];

    if (fileio_normalize_relpath(path, normalized, sizeof(normalized), allow_empty_relpath) != 0)
    {
        log_error("FILEIO: Invalid relative path: '%s'.", path ? path : "(null)");
        return -1;
    }

    if (normalized[0] == '\0')
    {
        snprintf(full_path, full_path_size, "%s", fileio_mount_point);
    }
    else
    {
        snprintf(full_path, full_path_size, "%s/%s", fileio_mount_point, normalized);
    }
    full_path[full_path_size - 1] = '\0';

    return 0;
}

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
    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    if (fileio_build_full_path(filename, full_path, sizeof(full_path), false) != 0)
    {
        return -1;
    }

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
    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    if (fileio_build_full_path(filename, full_path, sizeof(full_path), false) != 0)
    {
        result.error = -1;
        return result;
    }

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

    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    if (fileio_build_full_path(filename, full_path, sizeof(full_path), false) != 0)
    {
        return false;
    }

    struct stat buffer;
    return (stat(full_path, &buffer) == 0);
}

int fileio_rmfile_common(const char *filename)
{
    if (!fileio_mount_point_initialized)
    {
        log_error("FILEIO: Mount point not initialized.");
        return -1;
    }
    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    if (fileio_build_full_path(filename, full_path, sizeof(full_path), false) != 0)
    {
        return -1;
    }

    if (remove(full_path) != 0)
    {
        if (errno == ENOENT)
        {
            return 0;
        }
        log_error("FILEIO: Failed to remove file: %s", full_path);
        return -1;
    }

    return 0;
}

int fileio_rmdir_common(const char *path)
{
    if (!fileio_mount_point_initialized)
    {
        log_error("FILEIO: Mount point not initialized.");
        return -1;
    }
    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    if (fileio_build_full_path(path, full_path, sizeof(full_path), false) != 0)
    {
        return -1;
    }

    if (rmdir(full_path) != 0)
    {
        if (errno == ENOENT)
        {
            return 0;
        }
        log_error("FILEIO: Failed to remove directory: %s", full_path);
        return -1;
    }

    return 0;
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

    char full_path[FILEIO_MAX_PATH_LENGTH * 2];
    if (fileio_build_full_path(path, full_path, sizeof(full_path), true) != 0)
    {
        return -1;
    }

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
