/*
  block.h -- block working functions declaration.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_BLOCK_H
#define AAL_BLOCK_H

#include <aal/types.h>

extern aal_block_t *aal_block_create(aal_device_t *device, 
				     blk_t blk, char c);

extern aal_block_t *aal_block_open(aal_device_t *device, 
				   blk_t blk);

extern void aal_block_close(aal_block_t *block);
extern uint32_t aal_block_size(aal_block_t *block);

extern blk_t aal_block_number(aal_block_t *block);

#ifndef ENABLE_STAND_ALONE

extern errno_t aal_block_reopen(aal_block_t *block, 
				aal_device_t *device,
				blk_t blk);

extern errno_t aal_block_sync(aal_block_t *block);

extern void aal_block_relocate(aal_block_t *block, 
			       blk_t blk);

#endif

#define B_DIRTY 0

#define aal_block_isdirty(block) block->flags & (1 << B_DIRTY)
#define aal_block_isclean(block) (!aal_block_is_dirty(block))

#define aal_block_mkdirty(block) block->flags |=  (1 << B_DIRTY)
#define aal_block_mkclean(block) block->flags &= ~(1 << B_DIRTY)

#endif

