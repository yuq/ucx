/*
 * Copyright (C) Advanced Micro Devices, Inc. 2019. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */


#ifndef ROCM_IPC_UTIL_H
#define ROCM_IPC_UTIL_H

#include <hsa.h>
#include "rocm_ipc_md.h"

hsa_status_t uct_rocm_ipc_init(void);
hsa_agent_t uct_rocm_ipc_get_dev_agent(int dev_num);
int uct_rocm_ipc_is_gpu_agent(hsa_agent_t agent);
int uct_rocm_ipc_get_gpu_agents(hsa_agent_t **agents);
hsa_status_t uct_rocm_ipc_pack_key(void *address, size_t length,
                                   uct_rocm_ipc_key_t *key);
hsa_status_t uct_rocm_ipc_lock_ptr(void *ptr, size_t size, void **lock_ptr,
                                   hsa_agent_t *agent);

#endif
