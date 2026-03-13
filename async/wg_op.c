#include "wg_op.h"

void wg_op_init(wg_op_t *op, wg_op_kind_t kind, void *impl, void (*destroy_impl)(void *impl))
{
    if (!op)
    {
        return;
    }

    op->kind = kind;
    op->state = WG_OP_PENDING;
    op->status_code = 0;
    op->impl = impl;
    op->destroy_impl = destroy_impl;
}

bool wg_op_is_done(const wg_op_t *op)
{
    return op != NULL && op->state == WG_OP_DONE;
}

int wg_op_status(const wg_op_t *op)
{
    if (!op)
    {
        return -1;
    }

    return op->status_code;
}

void wg_op_complete(wg_op_t *op, int status_code)
{
    if (!op)
    {
        return;
    }

    op->state = WG_OP_DONE;
    op->status_code = status_code;
}

void *wg_op_impl(const wg_op_t *op)
{
    if (!op)
    {
        return NULL;
    }

    return op->impl;
}

void wg_op_reset(wg_op_t *op)
{
    if (!op)
    {
        return;
    }

    op->state = WG_OP_PENDING;
    op->status_code = 0;
}

void wg_op_deinit(wg_op_t *op)
{
    if (!op)
    {
        return;
    }

    if (op->destroy_impl != NULL && op->impl != NULL)
    {
        op->destroy_impl(op->impl);
    }

    op->kind = WG_OP_KIND_NONE;
    op->state = WG_OP_DONE;
    op->status_code = 0;
    op->impl = NULL;
    op->destroy_impl = NULL;
}
