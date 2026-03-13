#ifndef FILEIO_H
#define FILEIO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "fileio_common.h"
#include "async/wg_op.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    wg_op_t op;
} fileio_sync_op_t;

int fileio_init(const char *mount_point);
void fileio_deinit(void);

fileio_sync_op_t *fileio_restore_async(void);
fileio_sync_op_t *fileio_flush_async(void);
bool fileio_sync_poll(fileio_sync_op_t *op);
int fileio_sync_finish(fileio_sync_op_t *op);
void fileio_sync_op_free(fileio_sync_op_t *op);

/**
 * Writes data to a file, ensuring that directories in the path exist.
 *
 * @param filename The name of the file to write to.
 * @param data The data to write to the file.
 * @return 0 on success, or -1 on failure.
 */
// int fileio_write(const char *filename, const char *data);
int fileio_write(const char *filename, void *data, size_t size);

/**
 * Reads data from a file.
 *
 * @param filename The name of the file to read from.
 * @return A dynamically allocated buffer containing the file contents, or NULL on failure.
 *         The caller is responsible for freeing the returned buffer.
 */
fileio_read_result_t fileio_read(const char *filename);

/**
 * Checks if a file exists.
 *
 * @param filename The name of the file to check.
 * @return true if the file exists, false otherwise.
 */
bool fileio_exists(const char *filename);

/**
 * Removes a file.
 *
 * @param filename The name of the file to remove.
 * @return 0 on success, or -1 on failure.
 */
int fileio_rmfile(const char *filename);

/**
 * Removes a directory.
 *
 * @param path The directory path to remove.
 * @return 0 on success, or -1 on failure.
 */
int fileio_rmdir(const char *path);

/**
 * Creates directories recursively.
 *
 * @param path The directory path to create.
 * @return 0 on success, or -1 on failure.
 */
int fileio_mkdir(const char *path);

#ifdef __cplusplus
}
#endif

#endif // FILEIO_H
