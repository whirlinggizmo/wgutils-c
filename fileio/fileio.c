#include "fileio.h"
#include "fileio_common.h"

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

bool fileio_wait_for_ready(int timeout_ms)
{
    (void)timeout_ms;
    return true;
}

int fileio_write(const char *filename, void *data, size_t size)
{
    return fileio_write_common(filename, data, size);
}

fileio_read_result_t fileio_read(const char *filename)
{
    return fileio_read_common(filename);
}

fileio_read_result_t fileio_read_url(const char *host, const char *path, int timeout_ms) {
    return fileio_read_url_common(host, path, timeout_ms);
}

bool fileio_exists(const char *filename)
{
    return fileio_exists_common(filename);
}

int fileio_mkdir(const char *path)
{
    return fileio_mkdir_common(path);
}
