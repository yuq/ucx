/*
 * Copyright (C) Advanced Micro Devices, Inc. 2019. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCS_ROCM_H
#define UCS_ROCM_H

#include <hsa.h>
#include <hsa_ext_amd.h>

namespace ucs {

class rocm {
public:
    rocm();
    ~rocm();

    bool present() { return m_present; }

    void *alloc_mem(size_t size);
    void free_mem(void *buff);

private:
    static hsa_status_t probe_agent_callback(hsa_agent_t agent, void* data);
    static hsa_status_t probe_memory_pool_callback(hsa_amd_memory_pool_t memory_pool,
                                                   void* data);

    bool m_present;
    hsa_agent_t m_agent;
    hsa_amd_memory_pool_t m_pool;
};

extern rocm rocm_helper;

}

#endif
