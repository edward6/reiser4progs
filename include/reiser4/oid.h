/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid.h -- oid allocator functions. */

#ifndef REISER4_OID_H
#define REISER4_OID_H

#ifndef ENABLE_STAND_ALONE
#include <reiser4/types.h>

extern void reiser4_oid_close(reiser4_oid_t *oid);
extern reiser4_oid_t *reiser4_oid_open(reiser4_fs_t *fs);

extern errno_t reiser4_oid_layout(reiser4_oid_t *oid,
				  block_func_t block_func,
				  void *data);

extern errno_t reiser4_oid_sync(reiser4_oid_t *oid);
extern errno_t reiser4_oid_valid(reiser4_oid_t *oid);
extern reiser4_oid_t *reiser4_oid_create(reiser4_fs_t *fs);

extern oid_t reiser4_oid_next(reiser4_oid_t *oid);
extern oid_t reiser4_oid_allocate(reiser4_oid_t *oid);
extern void reiser4_oid_release(reiser4_oid_t *oid, oid_t id);

extern errno_t reiser4_oid_print(reiser4_oid_t *oid,
				 aal_stream_t *stream);

extern uint64_t reiser4_oid_free(reiser4_oid_t *oid);
extern uint64_t reiser4_oid_used(reiser4_oid_t *oid);

extern bool_t reiser4_oid_isdirty(reiser4_oid_t *oid);
extern void reiser4_oid_mkdirty(reiser4_oid_t *oid);
extern void reiser4_oid_mkclean(reiser4_oid_t *oid);

extern oid_t reiser4_oid_root_locality(reiser4_oid_t *oid);
extern oid_t reiser4_oid_root_objectid(reiser4_oid_t *oid);
extern oid_t reiser4_oid_hyper_locality(reiser4_oid_t *oid);

#endif

#endif

