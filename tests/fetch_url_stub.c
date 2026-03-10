#include "fetch_url/fetch_url.h"

fetch_url_result_t fetch_url(const char *url, int timeout_ms)
{
    (void)url;
    (void)timeout_ms;

    fetch_url_result_t result = {0};
    result.code = 501;
    return result;
}

fetch_url_result_t fetch_url_with_path(const char *host_url,
                                       const char *relative_path,
                                       int timeout_ms)
{
    (void)host_url;
    (void)relative_path;
    (void)timeout_ms;

    fetch_url_result_t result = {0};
    result.code = 503;
    return result;
}
