/*
 * Copyright (C) Advanced Micro Devices, Inc. 2019. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <ucm/rocm/rocmmem.h>

#include <ucm/event/event.h>
#include <ucm/util/log.h>
#include <ucm/util/reloc.h>
#include <ucs/sys/preprocessor.h>

#include <unistd.h>
#include <pthread.h>

static ucm_reloc_patch_t patches[] = {
    {UCS_PP_MAKE_STRING(hsa_amd_memory_pool_allocate),
     ucm_override_hsa_amd_memory_pool_allocate},
    {UCS_PP_MAKE_STRING(hsa_amd_memory_pool_free),
     ucm_override_hsa_amd_memory_pool_free},
    {NULL, NULL}
};

ucs_status_t ucm_rocmmem_install(void)
{
    static int ucm_rocmmem_installed = 0;
    static pthread_mutex_t install_mutex = PTHREAD_MUTEX_INITIALIZER;
    ucm_reloc_patch_t *patch;
    ucs_status_t status = UCS_OK;

    pthread_mutex_lock(&install_mutex);

    if (ucm_rocmmem_installed) {
        goto out_unlock;
    }

    for (patch = patches; patch->symbol != NULL; ++patch) {
        status = ucm_reloc_modify(patch);
        if (status != UCS_OK) {
            ucm_warn("failed to install relocation table entry for '%s'", patch->symbol);
            goto out_unlock;
        }
    }

    ucm_debug("rocm hooks are ready");
    ucm_rocmmem_installed = 1;

out_unlock:
    pthread_mutex_unlock(&install_mutex);
    return status;
}
