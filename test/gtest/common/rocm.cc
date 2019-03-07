/*
 * Copyright (C) Advanced Micro Devices, Inc. 2019. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "rocm.h"

#include <ucs/debug/log.h>

namespace ucs {

/* global variable for tests to call rocm memory */
rocm rocm_helper;

rocm::rocm() : m_present(false)
{
    hsa_status_t status = hsa_init();
    if (status != HSA_STATUS_SUCCESS) {
        ucs_debug("Fail to init ROCM hsa");
        return;
    }
        
    status = hsa_iterate_agents(probe_agent_callback, &m_agent);
    if (status != HSA_STATUS_INFO_BREAK) {
        ucs_debug("Fail to find GPU ROCM agent");
        hsa_shut_down();
        return;
    }

    status = hsa_amd_agent_iterate_memory_pools(m_agent,
                                                probe_memory_pool_callback,
                                                &m_pool);
    if (status != HSA_STATUS_INFO_BREAK) {
        ucs_debug("Fail to find ROCM device memory pool");
        hsa_shut_down();
        return;
    }

    m_present = true;
}

rocm::~rocm()
{
    if (m_present)
        hsa_shut_down();
}

hsa_status_t rocm::probe_agent_callback(hsa_agent_t agent, void* data)
{
    hsa_status_t status;
    hsa_device_type_t device_type;

    status = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
    if (status != HSA_STATUS_SUCCESS) {
        ucs_debug("Fail to get ROCM agent info");
        return status;
    }

    /* use first GPU found */
    if (device_type == HSA_DEVICE_TYPE_GPU) {
        hsa_agent_t *ret = (hsa_agent_t *)data;
        *ret = agent;
        return HSA_STATUS_INFO_BREAK;
    }

    return HSA_STATUS_SUCCESS;
}

hsa_status_t rocm::probe_memory_pool_callback(hsa_amd_memory_pool_t memory_pool,
                                              void* data)
{
    hsa_status_t status;
    hsa_amd_segment_t segment;

    status = hsa_amd_memory_pool_get_info(memory_pool,
                                          HSA_AMD_MEMORY_POOL_INFO_SEGMENT,
                                          &segment);
    if (status != HSA_STATUS_SUCCESS) {
        ucs_debug("Fail to get ROCM memory pool info");
        return status;
    }

    uint32_t flags;
    status = hsa_amd_memory_pool_get_info(memory_pool,
                                          HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS,
                                          &flags);
    if (status != HSA_STATUS_SUCCESS) {
        ucs_debug("Fail to get ROCM memory pool info");
        return status;
    }

    if (segment == HSA_AMD_SEGMENT_GLOBAL &&
        flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED) {
        hsa_amd_memory_pool_t *ret = (hsa_amd_memory_pool_t *)data;
        *ret = memory_pool;
        return HSA_STATUS_INFO_BREAK;
    }

    return HSA_STATUS_SUCCESS;
}

void *rocm::alloc_mem(size_t size)
{
    hsa_status_t status;
    void *ret = NULL;

    status = hsa_amd_memory_pool_allocate(m_pool, size, 0, &ret);
    if (status != HSA_STATUS_SUCCESS)
        ucs_debug("Fail to alloc ROCM memory");

    return ret;
}

void rocm::free_mem(void *buff)
{
    hsa_amd_memory_pool_free(buff);
}

}
