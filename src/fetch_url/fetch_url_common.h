#ifndef FETCH_URL_COMMON_H
#define FETCH_URL_COMMON_H

#include "fetch_url.h"

#ifdef __cplusplus
extern "C" {
#endif

void fetch_url_result_reset_common(fetch_url_result_t *result);
int fetch_url_join_path_common(char *buffer,
                               size_t buffer_size,
                               const char *host_url,
                               const char *relative_path);
fetch_url_op_t *fetch_url_with_path_async_common(const char *host_url,
                                                 const char *relative_path,
                                                 int timeout_ms);
int fetch_url_with_path_sync_common(const char *host_url,
                                    const char *relative_path,
                                    int timeout_ms,
                                    fetch_url_result_t *result);
bool fetch_url_poll_common(fetch_url_op_t *op);

#ifdef __cplusplus
}
#endif

#endif // FETCH_URL_COMMON_H
