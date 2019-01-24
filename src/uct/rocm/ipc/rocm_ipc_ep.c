/*
 * Copyright (C) Advanced Micro Devices, Inc. 2019. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "rocm_ipc_ep.h"
#include "rocm_ipc_iface.h"
#include "rocm_ipc_util.h"

#include <hsakmt.h>

static UCS_CLASS_INIT_FUNC(uct_rocm_ipc_ep_t, uct_iface_t *tl_iface,
                           const uct_device_addr_t *dev_addr,
                           const uct_iface_addr_t *iface_addr)
{
    uct_rocm_ipc_iface_t *iface = ucs_derived_of(tl_iface, uct_rocm_ipc_iface_t);

    UCS_CLASS_CALL_SUPER_INIT(uct_base_ep_t, &iface->super);

    self->remote_pid = *(const pid_t*)iface_addr;

    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(uct_rocm_ipc_ep_t)
{

}

UCS_CLASS_DEFINE(uct_rocm_ipc_ep_t, uct_base_ep_t);
UCS_CLASS_DEFINE_NEW_FUNC(uct_rocm_ipc_ep_t, uct_ep_t, uct_iface_t*,
                          const uct_device_addr_t *, const uct_iface_addr_t *);
UCS_CLASS_DEFINE_DELETE_FUNC(uct_rocm_ipc_ep_t, uct_ep_t);

#define uct_rocm_ipc_trace_data(_remote_addr, _rkey, _fmt, ...) \
    ucs_trace_data(_fmt " to %"PRIx64"(%+ld)", ## __VA_ARGS__, (_remote_addr), \
                   (_rkey))

ucs_status_t uct_rocm_ipc_ep_zcopy(uct_ep_h tl_ep,
                                   uint64_t remote_addr,
                                   const uct_iov_t *iov,
                                   uct_rocm_ipc_key_t *key,
                                   int is_put)
{
    hsa_status_t status;
    hsa_agent_t local_agent, remote_agent;
    size_t size = uct_iov_get_length(iov);
    ucs_status_t ret = UCS_OK;
    void *lock_addr, *base_addr, *local_addr;

    /* no data to deliver */
    if (!size)
        return UCS_OK;

    if ((remote_addr < key->address) ||
        (remote_addr + size > key->address + key->length)) {
        ucs_error("remote addr %lx/%lx out of range %lx/%lx",
                  remote_addr, size, key->address, key->length);
        return UCS_ERR_INVALID_PARAM;
    }

    status = uct_rocm_ipc_lock_ptr(iov->buffer, size, &lock_addr,
                                   &base_addr, &local_agent);
    if (status != HSA_STATUS_SUCCESS)
        return status;

    local_addr = lock_addr ? lock_addr : iov->buffer;

    if (!key->lock_address) {
        void *remote_base_addr, *remote_copy_addr;
        void *dst_addr, *src_addr;
        hsa_agent_t dst_agent, src_agent;
        hsa_signal_t signal;

        status = hsa_amd_ipc_memory_attach(&key->ipc, key->length, 0,
                                           NULL, &remote_base_addr);
        if (status != HSA_STATUS_SUCCESS) {
            ucs_error("fail to attach ipc mem %p %d\n", (void *)key->address, status);
            ret = UCS_ERR_NO_RESOURCE;
            goto out_unlock;
        }

        if (!lock_addr) {
            hsa_agent_t *gpu_agents;
            int num_gpu = uct_rocm_ipc_get_gpu_agents(&gpu_agents);
            status = hsa_amd_agents_allow_access(num_gpu, gpu_agents, NULL, base_addr);
            if (status != HSA_STATUS_SUCCESS) {
                ucs_error("fail to map local mem %p %p %d\n",
                          local_addr, base_addr, status);
                ret = UCS_ERR_INVALID_ADDR;
                goto out_detach;
            }
        }

        remote_copy_addr = remote_base_addr + (remote_addr - key->address);
        remote_agent = uct_rocm_ipc_get_dev_agent(key->dev_num);

        if (is_put) {
            dst_addr = remote_copy_addr;
            dst_agent = remote_agent;

            src_addr = local_addr;
            src_agent = local_agent;
        }
        else {
            dst_addr = local_addr;
            dst_agent = local_agent;

            src_addr = remote_copy_addr;
            src_agent = remote_agent;
        }

        status = hsa_signal_create(1, 0, NULL, &signal);
        assert(status == HSA_STATUS_SUCCESS);

        status = hsa_amd_memory_async_copy(dst_addr, dst_agent,
                                           src_addr, src_agent,
                                           size, 0, NULL, signal);

        if (status == HSA_STATUS_SUCCESS)
            while (hsa_signal_wait_acquire(signal, HSA_SIGNAL_CONDITION_LT, 1,
                                           UINT64_MAX, HSA_WAIT_STATE_ACTIVE));
        else {
            ucs_error("copy error");
            ret = UCS_ERR_IO_ERROR;
        }

        hsa_signal_destroy(signal);

    out_detach:
        hsa_amd_ipc_memory_detach(remote_base_addr);
    }
    else {
        /* fallback to cma when remote buffer has no ipc */
        uct_rocm_ipc_ep_t *ep = ucs_derived_of(tl_ep, uct_rocm_ipc_ep_t);
        HSAKMT_STATUS hsa_status;
        void *remote_copy_addr = key->lock_address ?
            (void *)key->lock_address + (remote_addr - key->address) :
            (void *)remote_addr;
        HsaMemoryRange local_mem = {
            .MemoryAddress = local_addr,
            .SizeInBytes = size,
        };
        HsaMemoryRange remote_mem = {
            .MemoryAddress = remote_copy_addr,
            .SizeInBytes = size,
        };
        HSAuint64 copied = 0;

        if (is_put)
            hsa_status = hsaKmtProcessVMWrite(ep->remote_pid, &local_mem, 1,
                                              &remote_mem, 1, &copied);
        else
            hsa_status = hsaKmtProcessVMRead(ep->remote_pid,  &local_mem, 1,
                                             &remote_mem, 1, &copied);

        if (hsa_status != HSAKMT_STATUS_SUCCESS) {
            ucs_error("cma copy fail %d %d", hsa_status, errno);
            ret = UCS_ERR_IO_ERROR;
        }
        else
            assert(copied == size);
    }

 out_unlock:
    if (lock_addr)
        hsa_amd_memory_unlock(lock_addr);

    return ret;
}

ucs_status_t uct_rocm_ipc_ep_put_zcopy(uct_ep_h tl_ep, const uct_iov_t *iov, size_t iovcnt,
                                       uint64_t remote_addr, uct_rkey_t rkey,
                                       uct_completion_t *comp)
{
    ucs_status_t ret;
    uct_rocm_ipc_key_t *key = (uct_rocm_ipc_key_t *)rkey;

    ret = uct_rocm_ipc_ep_zcopy(tl_ep, remote_addr, iov, key, 1);

    UCT_TL_EP_STAT_OP(ucs_derived_of(tl_ep, uct_base_ep_t), PUT, ZCOPY,
                      uct_iov_total_length(iov, iovcnt));
    uct_rocm_ipc_trace_data(remote_addr, rkey, "PUT_ZCOPY [length %zu]",
                            uct_iov_total_length(iov, iovcnt));

    return ret;
}

ucs_status_t uct_rocm_ipc_ep_get_zcopy(uct_ep_h tl_ep, const uct_iov_t *iov, size_t iovcnt,
                                       uint64_t remote_addr, uct_rkey_t rkey,
                                       uct_completion_t *comp)
{
    ucs_status_t ret;
    uct_rocm_ipc_key_t *key = (uct_rocm_ipc_key_t *)rkey;

    ret = uct_rocm_ipc_ep_zcopy(tl_ep, remote_addr, iov, key, 0);

    UCT_TL_EP_STAT_OP(ucs_derived_of(tl_ep, uct_base_ep_t), GET, ZCOPY,
                      uct_iov_total_length(iov, iovcnt));
    uct_rocm_ipc_trace_data(remote_addr, rkey, "GET_ZCOPY [length %zu]",
                            uct_iov_total_length(iov, iovcnt));

    return ret;
}
