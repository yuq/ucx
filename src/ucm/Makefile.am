#
# Copyright (C) Mellanox Technologies Ltd. 2001-2015.  ALL RIGHTS RESERVED.
# Copyright (c) UT-Battelle, LLC. 2017. ALL RIGHTS RESERVED.
#
# See file LICENSE for terms.
#

lib_LTLIBRARIES = libucm.la

libucm_ladir       = $(includedir)/ucm
libucm_la_LIBADD   = # ../ucs/libucs.la
libucm_la_LDFLAGS  = \
    -ldl -version-info $(SOVERSION) \
    -Xlinker -z -Xlinker nodelete \
    -Xlinker -z -Xlinker interpose

libucm_la_CPPFLAGS = \
    $(BASE_CPPFLAGS) \
    -DUCM_MALLOC_PREFIX=ucm_dl

libucm_la_CFLAGS = \
	$(BASE_CFLAGS) \
	$(CFLAGS_NO_DEPRECATED)

nobase_dist_libucm_la_HEADERS = \
	api/ucm.h

noinst_HEADERS = \
	event/event.h \
	malloc/malloc_hook.h \
	malloc/allocator.h \
	mmap/mmap.h \
	util/replace.h \
	util/log.h \
	util/reloc.h \
	util/sys.h \
	bistro/bistro_int.h \
	bistro/bistro.h \
	bistro/bistro_x86_64.h \
	bistro/bistro_aarch64.h \
	bistro/bistro_ppc64.h

libucm_la_SOURCES = \
	event/event.c \
	malloc/malloc_hook.c \
	mmap/install.c \
	util/replace.c \
	util/log.c \
	util/reloc.c \
	util/sys.c \
	bistro/bistro.c \
	bistro/bistro_x86_64.c \
	bistro/bistro_aarch64.c \
	bistro/bistro_ppc64.c

if HAVE_CUDA
libucm_la_CPPFLAGS += $(CUDA_CPPFLAGS)
libucm_la_LDFLAGS  += $(CUDA_LDFLAGS)
noinst_HEADERS += \
	cuda/cudamem.h
libucm_la_SOURCES += \
    cuda/cudamem.c
endif

if HAVE_ROCM
libucm_la_CPPFLAGS += $(ROCM_CPPFLAGS)
libucm_la_LDFLAGS  += $(ROCM_LDFLAGS) $(ROCM_LIBS)
noinst_HEADERS += \
	rocm/rocmmem.h
libucm_la_SOURCES += \
    rocm/rocmmem.c
endif

if HAVE_UCM_PTMALLOC286
libucm_la_CPPFLAGS += \
    -fno-strict-aliasing \
    -DUSE_LOCKS=1 \
    -DMALLINFO_FIELD_TYPE=int

libucm_la_SOURCES += \
    ptmalloc286/malloc.c

noinst_HEADERS += \
    ptmalloc286/malloc-2.8.6.h
endif
