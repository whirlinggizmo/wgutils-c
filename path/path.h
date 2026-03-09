#ifndef PATH_H
#define PATH_H

#include <stdlib.h>

size_t path_join(const char *base, const char *path, char *buffer, size_t buffer_size);

size_t path_normalize(const char *path, char *buffer, size_t buffer_size);

size_t path_length(const char *base, ...);

char *path_join_alloc(const char *base, ...);

// Macro that automatically adds NULL terminator.
// NOTE: _path_join_alloc allocates the returned combined path, requiring the caller to free it
char *_path_join_alloc(const char *base, ...);
#define path_join_alloc(...) _path_join_alloc(__VA_ARGS__, (char *)NULL)

// Macro that automatically adds NULL terminator
size_t _path_length(const char *base, ...);
#define path_length(...) _path_length(__VA_ARGS__, (char *)NULL)

// Macro that automatically adds NULL terminator
size_t _path_join(const char *base, const char *path, char *buffer, size_t buffer_size, ...);
#define path_join(base, path, buffer, buffer_size, ...) _path_join(base, path, buffer, buffer_size, ##__VA_ARGS__, (char *)NULL)

char* path_get_filename(const char *path);
 
char* path_get_extension(const char *path);

#endif // PATH_H
