#ifndef WGUTILS_LOGGER_FORWARD_H
#define WGUTILS_LOGGER_FORWARD_H

/* Compatibility shim: preserve includes of "logger/log.h" while
 * the implementation lives under wgutils/vendor/logger-c.
 */
// IWYU pragma: begin_exports
#include "../vendor/logger-c/log.h"
// IWYU pragma: end_exports

#endif
