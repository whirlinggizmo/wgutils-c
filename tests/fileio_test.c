#include "fileio/fileio.h"
#include "fileio/fileio_common.h"
#include "test_common.h"

#include <string.h>
#include <unistd.h>

#define FILEIO_TEST_MOUNT "unit_fileio_guard_mount"
#define FILEIO_TEST_MOUNT_ABS "unit_fileio_guard_mount"

static int cleanup_is_safe(void)
{
    return strcmp(FILEIO_TEST_MOUNT_ABS, "unit_fileio_guard_mount") == 0;
}

static void cleanup_mount(void)
{
    if (!cleanup_is_safe())
    {
        return;
    }

    (void)remove(FILEIO_TEST_MOUNT_ABS "/safe/dir/file.bin");
    (void)rmdir(FILEIO_TEST_MOUNT_ABS "/safe/dir");
    (void)rmdir(FILEIO_TEST_MOUNT_ABS "/safe");
    (void)rmdir(FILEIO_TEST_MOUNT_ABS);
}

int fileio_test_run(void)
{
    const unsigned char payload[] = { 1, 2, 3, 4 };
    fileio_sync_op_t *restore_op = NULL;
    fileio_sync_op_t *flush_op = NULL;

    TEST_ASSERT(cleanup_is_safe());
    fileio_deinit();
    cleanup_mount();

    TEST_ASSERT(fileio_init(FILEIO_TEST_MOUNT) == 0);
    restore_op = fileio_restore_begin();
    TEST_ASSERT(restore_op != NULL);
    TEST_ASSERT(fileio_sync_poll(restore_op) == true);
    TEST_ASSERT(fileio_sync_finish(restore_op) == 0);
    fileio_sync_op_free(restore_op);
    restore_op = NULL;

    TEST_ASSERT(fileio_mkdir("safe/dir") == 0);
    TEST_ASSERT(fileio_write("safe/dir/file.bin", (void *)payload, sizeof(payload)) == 0);
    TEST_ASSERT(fileio_exists("safe/dir/file.bin") == true);

    // Reject traversal and absolute/drive-style paths.
    TEST_ASSERT(fileio_write("../escape.bin", (void *)payload, sizeof(payload)) == -1);
    TEST_ASSERT(fileio_write("/abs/escape.bin", (void *)payload, sizeof(payload)) == -1);
    TEST_ASSERT(fileio_write("C:/escape.bin", (void *)payload, sizeof(payload)) == -1);
    TEST_ASSERT(fileio_mkdir("../bad/dir") == -1);
    TEST_ASSERT(fileio_rmfile("../safe/dir/file.bin") == -1);
    TEST_ASSERT(fileio_rmdir("../safe/dir") == -1);
    TEST_ASSERT(fileio_exists("../safe/dir/file.bin") == false);

    // rmdir should fail while directory has contents.
    TEST_ASSERT(fileio_rmdir("safe/dir") == -1);

    // file + directory remove success path.
    TEST_ASSERT(fileio_rmfile("safe/dir/file.bin") == 0);
    TEST_ASSERT(fileio_exists("safe/dir/file.bin") == false);
    TEST_ASSERT(fileio_rmdir("safe/dir") == 0);
    TEST_ASSERT(fileio_rmdir("safe") == 0);

    // Missing targets are treated as success.
    TEST_ASSERT(fileio_rmfile("missing/file.bin") == 0);
    TEST_ASSERT(fileio_rmdir("missing/dir") == 0);

    flush_op = fileio_flush_begin();
    TEST_ASSERT(flush_op != NULL);
    TEST_ASSERT(fileio_sync_poll(flush_op) == true);
    TEST_ASSERT(fileio_sync_finish(flush_op) == 0);
    fileio_sync_op_free(flush_op);
    flush_op = NULL;

    fileio_deinit();
    cleanup_mount();
    return 0;
}
