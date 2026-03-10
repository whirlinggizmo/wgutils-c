#include "logger/log.h"

#undef log_trace
#undef log_debug
#undef log_info
#undef log_warn
#undef log_error
#undef log_fatal
#include "../vendor/logger-c/log.h"

void log_set_log_level(size_t level)
{
    log_set_level(ROOT_HANDLER_NAME, level);
}

void log_set_quiet_mode(bool quiet)
{
    log_set_quiet(ROOT_HANDLER_NAME, quiet);
}
