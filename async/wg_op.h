#ifndef WG_OP_H
#define WG_OP_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    WG_OP_PENDING = 0,
    WG_OP_DONE = 1
} wg_op_state_t;

typedef enum
{
    WG_OP_KIND_NONE = 0,
    WG_OP_KIND_FETCH_URL = 1,
    WG_OP_KIND_FILEIO_RESTORE = 2,
    WG_OP_KIND_FILEIO_FLUSH = 3
} wg_op_kind_t;

typedef struct wg_op
{
    wg_op_kind_t kind;
    wg_op_state_t state;
    int status_code;
    void *impl;
    void (*destroy_impl)(void *impl);
} wg_op_t;

void wg_op_init(wg_op_t *op, wg_op_kind_t kind, void *impl, void (*destroy_impl)(void *impl));
bool wg_op_is_done(const wg_op_t *op);
int wg_op_status(const wg_op_t *op);
void wg_op_complete(wg_op_t *op, int status_code);
void *wg_op_impl(const wg_op_t *op);
void wg_op_reset(wg_op_t *op);
void wg_op_deinit(wg_op_t *op);

#ifdef __cplusplus
}
#endif

#endif // WG_OP_H
