/*
  block.h -- block functions.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licencing governed by
  reiser4progs/COPYING.
*/

#ifndef BLOCK_H
#define BLOCK_H

/*
  Disk block structure. It is a replica of struct buffer_head from the linux
  kernel. It consists of flags (dirty, clean, etc), data (pointer to data of
  block), block size, offset (offset in bytes where block is placed on device),
  and pointer to device, block opened on.
*/
struct aal_block {
	int flags;
	void *data;
	uint64_t offset;
	aal_device_t *device;
};

typedef struct aal_block aal_block_t;

extern aal_block_t *aal_block_create(aal_device_t *device, 
				     blk_t blk, char c);

extern aal_block_t *aal_block_open(aal_device_t *device, 
				   blk_t blk);

extern errno_t aal_block_reopen(aal_block_t *block, 
				aal_device_t *device, blk_t blk);

extern void aal_block_close(aal_block_t *block);
extern errno_t aal_block_sync(aal_block_t *block);
extern uint32_t aal_block_size(aal_block_t *block);

extern blk_t aal_block_number(aal_block_t *block);

extern void aal_block_relocate(aal_block_t *block, 
			       blk_t blk);

#define B_DIRTY 0

#define aal_block_isdirty(block) block->flags & (1 << B_DIRTY)
#define aal_block_isclean(block) (!aal_block_is_dirty(block))

#define aal_block_mkdirty(block) block->flags |=  (1 << B_DIRTY)
#define aal_block_mkclean(block) block->flags &= ~(1 << B_DIRTY)

#endif

