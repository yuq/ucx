/*
 * Copyright (C) Advanced Micro Devices, Inc. 2019. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */


#ifndef ROCM_IPC_UTIL_H
#define ROCM_IPC_UTIL_H

#include <hsa.h>
#include "rocm_ipc_md.h"

hsa_status_t uct_rocm_ipc_init(void);
hsa_agent_t uct_rocm_ipc_get_mem_agent(void *address);
hsa_agent_t uct_rocm_ipc_get_dev_agent(int dev_num);
hsa_status_t uct_rocm_ipc_pack_key(void *address, size_t length,
                                   uct_rocm_ipc_key_t *key);

#endif
