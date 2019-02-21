/*
 * Copyright (C) Advanced Micro Devices, Inc. 2019. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCT_ROCM_COPY_EP_H
#define UCT_ROCM_COPY_EP_H

#include <uct/api/uct.h>
#include <uct/base/uct_iface.h>
#include <ucs/type/class.h>


typedef struct uct_rocm_copy_ep_addr {
    int                ep_id;
} uct_rocm_copy_ep_addr_t;

typedef struct uct_rocm_copy_ep {
    uct_base_ep_t           super;
    struct uct_rocm_copy_ep *next;
} uct_rocm_copy_ep_t;

UCS_CLASS_DECLARE_NEW_FUNC(uct_rocm_copy_ep_t, uct_ep_t, uct_iface_t*,
                           const uct_device_addr_t *, const uct_iface_addr_t *);
UCS_CLASS_DECLARE_DELETE_FUNC(uct_rocm_copy_ep_t, uct_ep_t);

ucs_status_t uct_rocm_copy_ep_put_short(uct_ep_h tl_ep, const void *buffer,
                                        unsigned length, uint64_t remote_addr,
                                        uct_rkey_t rkey);

ucs_status_t uct_rocm_copy_ep_get_short(uct_ep_h tl_ep, void *buffer,
                                        unsigned length, uint64_t remote_addr,
                                        uct_rkey_t rkey);

#endif
