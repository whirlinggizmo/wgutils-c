#ifndef FILEIO_COMMON_H
#define FILEIO_COMMON_H

#include <stdbool.h>
#include <stdlib.h>
#define FILEIO_MAX_PATH_LENGTH 256

extern char fileio_mount_point[FILEIO_MAX_PATH_LENGTH];
extern bool fileio_mount_point_initialized;

typedef struct
{
    unsigned char *data;
    size_t size;
    int error;
} fileio_read_result_t;

int fileio_init_common(const char *mount_point);
void fileio_deinit_common(void);

/**
 * Writes data to a file.
 *
 * @param filename The name of the file to write to.
 * @param data The data to write to the file.
 * @return 0 on success, or -1 on failure.
 */
int fileio_write_common(const char *filename, void *data, size_t size);

/**
 * Reads data from a file.
 *
 * @param filename The name of the file to read from.
 * @return A dynamically allocated buffer containing the file contents, or NULL on failure.
 *         The caller is responsible for freeing the returned buffer.
 */
fileio_read_result_t fileio_read_common(const char *filename);

/**
 * Reads data from a file.
 *
 * @param host The host to fetch the file from. (e.g. "https://example.com")
 * @param path The path to the file on the host. (e.g. "/path/to/file.txt")
 * @param timeout_ms The timeout in milliseconds for the fetch operation.
 * @return A dynamically allocated buffer containing the file contents, or NULL on failure.
 *         The caller is responsible for freeing the returned buffer.
 */
fileio_read_result_t fileio_read_url_common(const char *host, const char *path, int timeout_ms);

/**
 * Checks if a file exists.
 *
 * @param filename The name of the file to check.
 * @return true if the file exists, false otherwise.
 */
bool fileio_exists_common(const char *filename);

/**
 * Creates directories recursively.
 *
 * @param path The directory path to create.
 * @return 0 on success, or -1 on failure.
 */
int fileio_mkdir_common(const char *path);

#endif // FILEIO_COMMON_H
