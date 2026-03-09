#ifndef URI_H
#define URI_H

#include <stdbool.h>
#include <stddef.h>

// Returns true when input matches "<scheme>://...".
bool uri_is_url(const char *value);

// Normalizes only the URL path segment while preserving scheme, authority,
// query, and fragment. If input is not a URL, it is copied as-is.
size_t uri_normalize(const char *uri, char *buffer, size_t buffer_size);

#endif // URI_H
