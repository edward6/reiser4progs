/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   alloc.h -- repair block allocator functions. */

#ifndef REPAIR_ALLOC_H
#define REPAIR_ALLOC_H

extern errno_t repair_alloc_layout_bad(reiser4_alloc_t *alloc, 
				       region_func_t func, void *data);

extern errno_t repair_alloc_print(reiser4_alloc_t *alloc,
				  aal_stream_t *stream);

extern errno_t repair_alloc_pack(reiser4_alloc_t *alloc,
				 aal_stream_t *stream);

extern reiser4_alloc_t *repair_alloc_unpack(reiser4_fs_t *fs,
					    aal_stream_t *stream);

#endif
