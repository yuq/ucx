/*
 * Copyright (C) Advanced Micro Devices, Inc. 2019. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "rocm_copy_ep.h"
#include "rocm_copy_iface.h"

#include <uct/base/uct_log.h>
#include <ucs/debug/memtrack.h>
#include <ucs/type/class.h>

#ifdef USE_GDR_COPY
#include <gdrapi.h>
#endif

static UCS_CLASS_INIT_FUNC(uct_rocm_copy_ep_t, uct_iface_t *tl_iface,
                           const uct_device_addr_t *dev_addr,
                           const uct_iface_addr_t *iface_addr)
{
    uct_rocm_copy_iface_t *iface = ucs_derived_of(tl_iface, uct_rocm_copy_iface_t);

    UCS_CLASS_CALL_SUPER_INIT(uct_base_ep_t, &iface->super);

    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(uct_rocm_copy_ep_t)
{
}

UCS_CLASS_DEFINE(uct_rocm_copy_ep_t, uct_base_ep_t)
UCS_CLASS_DEFINE_NEW_FUNC(uct_rocm_copy_ep_t, uct_ep_t, uct_iface_t*,
                          const uct_device_addr_t *, const uct_iface_addr_t *);
UCS_CLASS_DEFINE_DELETE_FUNC(uct_rocm_copy_ep_t, uct_ep_t);

#define uct_rocm_copy_trace_data(_remote_addr, _rkey, _fmt, ...) \
     ucs_trace_data(_fmt " to %"PRIx64"(%+ld)", ## __VA_ARGS__, (_remote_addr), \
                    (_rkey))

ucs_status_t uct_rocm_copy_ep_put_short(uct_ep_h tl_ep, const void *buffer,
                                        unsigned length, uint64_t remote_addr,
                                        uct_rkey_t rkey)
{
#ifdef USE_GDR_COPY
    int ret;

    if (ucs_likely(length)) {
        ret = gdr_copy_to_bar((void *)remote_addr, buffer, length);
        if (ret) {
            ucs_error("gdr_copy_to_bar failed. ret:%d", ret);
            return UCS_ERR_IO_ERROR;
        }
    }
#else
    memcpy((void *)remote_addr, buffer, length);
#endif

    UCT_TL_EP_STAT_OP(ucs_derived_of(tl_ep, uct_base_ep_t), PUT, SHORT, length);
    ucs_trace_data("PUT_SHORT size %d from %p to %p",
                   length, buffer, (void *)remote_addr);
    return UCS_OK;
}

ucs_status_t uct_rocm_copy_ep_get_short(uct_ep_h tl_ep, void *buffer,
                                        unsigned length, uint64_t remote_addr,
                                        uct_rkey_t rkey)
{
#ifdef USE_GDR_COPY
    int ret;

    if (ucs_likely(length)) {
        ret = gdr_copy_from_bar(buffer, (void *)remote_addr, length);
        if (ret) {
            ucs_error("gdr_copy_from_bar failed. ret:%d", ret);
            return UCS_ERR_IO_ERROR;
        }
    }
#else
    memcpy(buffer, (void *)remote_addr, length);
#endif

    UCT_TL_EP_STAT_OP(ucs_derived_of(tl_ep, uct_base_ep_t), GET, SHORT, length);
    ucs_trace_data("GET_SHORT size %d from %p to %p",
                   length, (void *)remote_addr, buffer);
    return UCS_OK;
}
