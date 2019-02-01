/*
 * Copyright (C) Advanced Micro Devices, Inc. 2019. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "rocm_ipc_util.h"
#include <pthread.h>

#define MAX_AGENTS 16
static struct agents {
    hsa_agent_t agents[MAX_AGENTS];
    int num;
    hsa_agent_t gpu_agents[MAX_AGENTS];
    int num_gpu;
} uct_rocm_ipc_agents = {0};

int uct_rocm_ipc_get_gpu_agents(hsa_agent_t **agents)
{
    *agents = uct_rocm_ipc_agents.gpu_agents;
    return uct_rocm_ipc_agents.num_gpu;
}

static hsa_status_t uct_rocm_hsa_agent_callback(hsa_agent_t agent, void* data)
{
    hsa_device_type_t device_type;

    assert(uct_rocm_ipc_agents.num < MAX_AGENTS);

    hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
    if (device_type == HSA_DEVICE_TYPE_CPU) {
        ucs_trace("%d found cpu agent %lu", getpid(), agent.handle);
    }
    else if (device_type == HSA_DEVICE_TYPE_GPU) {
        uint32_t bdfid = 0;
        uct_rocm_ipc_agents.gpu_agents[uct_rocm_ipc_agents.num_gpu++] = agent;
        hsa_agent_get_info(agent, HSA_AMD_AGENT_INFO_BDFID, &bdfid);
        ucs_trace("%d found gpu agent %lu bdfid %x", getpid(), agent.handle, bdfid);
    }
    else {
        ucs_trace("%d found unknown agent %lu", getpid(), agent.handle);
    }

    uct_rocm_ipc_agents.agents[uct_rocm_ipc_agents.num++] = agent;
    return HSA_STATUS_SUCCESS;
}

hsa_status_t uct_rocm_ipc_init(void)
{
    static pthread_mutex_t rocm_init_mutex = PTHREAD_MUTEX_INITIALIZER;
    static volatile int rocm_ucx_initialized = 0;
    hsa_status_t status;

    if (pthread_mutex_lock(&rocm_init_mutex) == 0) {
        if (rocm_ucx_initialized) {
            status =  HSA_STATUS_SUCCESS;
            goto end;
        }
    } else  {
        ucs_error("Could not take mutex");
        status = HSA_STATUS_ERROR;
        return status;
    }

    status = hsa_init();
    if (status != HSA_STATUS_SUCCESS) {
        ucs_debug("Failure to open HSA connection: 0x%x", status);
        goto end;
    }

    status = hsa_iterate_agents(uct_rocm_hsa_agent_callback, NULL);
    if (status != HSA_STATUS_SUCCESS && status != HSA_STATUS_INFO_BREAK) {
        ucs_debug("Failure to iterate HSA agents: 0x%x", status);
        goto end;
    }

    rocm_ucx_initialized = 1;

end:
    pthread_mutex_unlock(&rocm_init_mutex);
    return status;
}

hsa_agent_t uct_rocm_ipc_get_dev_agent(int dev_num)
{
    assert(dev_num < uct_rocm_ipc_agents.num);
    return uct_rocm_ipc_agents.agents[dev_num];
}

static int uct_rocm_ipc_get_dev_num(hsa_agent_t agent)
{
    int i;

    for (i = 0; i < uct_rocm_ipc_agents.num; i++) {
        if (uct_rocm_ipc_agents.agents[i].handle == agent.handle)
            return i;
    }
    assert(0);
    return -1;
}

int uct_rocm_ipc_is_gpu_agent(hsa_agent_t agent)
{
    int i;

    for (i = 0; i < uct_rocm_ipc_agents.num_gpu; i++) {
        if (uct_rocm_ipc_agents.gpu_agents[i].handle == agent.handle)
            return 1;
    }
    return 0;
}

hsa_status_t uct_rocm_ipc_lock_ptr(void *ptr, size_t size, void **lock_ptr,
                                   void **base_ptr, size_t *base_size,
                                   hsa_agent_t *agent)
{
    hsa_status_t status;
    hsa_amd_pointer_info_t info;

    info.size = sizeof(hsa_amd_pointer_info_t);
    status = hsa_amd_pointer_info(ptr, &info, NULL, NULL, NULL);
    if (status != HSA_STATUS_SUCCESS) {
        ucs_error("get pointer info fail %p", ptr);
        return status;
    }

    *agent = info.agentOwner;

    if (info.type != HSA_EXT_POINTER_TYPE_UNKNOWN &&
        info.type != HSA_EXT_POINTER_TYPE_LOCKED) {
        *lock_ptr = NULL;
        if (base_ptr)
            *base_ptr = info.agentBaseAddress;
        if (base_size)
            *base_size = info.sizeInBytes;
        return HSA_STATUS_SUCCESS;
    }

    status = hsa_amd_memory_lock(ptr, size, NULL, 0, lock_ptr);
    if (status != HSA_STATUS_SUCCESS)
        ucs_error("lock user mem fail");

    return status;
}

hsa_status_t uct_rocm_ipc_pack_key(void *address, size_t length,
                                   uct_rocm_ipc_key_t *key)
{
    hsa_status_t status;
    hsa_agent_t agent;
    void *lock_ptr, *base_ptr = NULL;
    size_t size = 0;

    status = uct_rocm_ipc_lock_ptr(address, length, &lock_ptr, &base_ptr, &size, &agent);
    if (status != HSA_STATUS_SUCCESS)
        return status;

    key->address = (uintptr_t)address;
    key->lock_address = (uintptr_t)lock_ptr;
    key->length = length;
    key->dev_num = uct_rocm_ipc_get_dev_num(agent);

    /* IPC does not support locked ptr yet */
    if (lock_ptr)
        return HSA_STATUS_SUCCESS;

    status = hsa_amd_ipc_memory_create(base_ptr, size, &key->ipc);
    if (status == HSA_STATUS_SUCCESS) {
        key->address = (uintptr_t)base_ptr;
        key->length = size;
        key->ipc_valid = 1;
    }
    else {
        static int once = 1;
        /* when HSA_USERPTR_FOR_PAGED_MEM=1, system bo is allocated with
         * userptr mem, but type is still HSA_EXT_POINTER_TYPE_HSA */
        key->ipc_valid = 0;
        if (once) {
            ucs_warn("Failed to create ipc for %p, fallback to CMA for P2D", address);
            once = 0;
        }
    }

    return HSA_STATUS_SUCCESS;
}
