/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40.h -- reiser4 disk-format plugin. */

#ifndef FORMAT40_H
#define FORMAT40_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

#define FORMAT40_MAGIC "ReIsEr40FoRmAt"

#define FORMAT40_BLOCKNR(blksize) \
        ((REISER4_MASTER_OFFSET / blksize) + 1)

#define MASTER_BLOCKNR(blksize) \
        (REISER4_MASTER_OFFSET / blksize)

#define SUPER(entity) (&((format40_t *)entity)->super)

#define MAGIC_SIZE 16
	
struct format40_super {
	d64_t sb_block_count;
	d64_t sb_free_blocks;
	d64_t sb_root_block;
	d64_t sb_oid;
	d64_t sb_file_count;
	d64_t sb_flushes;
    
	d32_t sb_mkfs_id;
	char sb_magic[MAGIC_SIZE];

	d16_t sb_tree_height;
	d16_t sb_tail_policy;
	d64_t sb_flags;
	
	char sb_unused[432];
} __attribute__((packed));

typedef struct format40_super format40_super_t;

struct format40 {
	reiser4_plug_t *plug;

	uint32_t state;
	uint32_t blksize;
	
	aal_device_t *device;
	format40_super_t super;
};

typedef struct format40 format40_t;
extern reiser4_plug_t format40_plug;
extern reiser4_core_t *format40_core;

#define get_sb_mkfs_id(sb)			aal_get_le32(sb, sb_mkfs_id)
#define set_sb_mkfs_id(sb, val)			aal_set_le32(sb, sb_mkfs_id, val)

#define get_sb_block_count(sb)			aal_get_le64(sb, sb_block_count)
#define set_sb_block_count(sb, val)		aal_set_le64(sb, sb_block_count, val)

#define get_sb_free_blocks(sb)			aal_get_le64(sb, sb_free_blocks)
#define set_sb_free_blocks(sb, val)		aal_set_le64(sb, sb_free_blocks, val)

#define get_sb_root_block(sb)			aal_get_le64(sb, sb_root_block)
#define set_sb_root_block(sb, val)		aal_set_le64(sb, sb_root_block, val)

#define get_sb_tail_policy(sb)			aal_get_le16(sb, sb_tail_policy)
#define set_sb_tail_policy(sb, val)		aal_set_le16(sb, sb_tail_policy, val)

#define get_sb_oid(sb)				aal_get_le64(sb, sb_oid)
#define set_sb_oid(sb, val)			aal_set_le64(sb, sb_oid, val)

#define get_sb_file_count(sb)			aal_get_le64(sb, sb_file_count)
#define set_sb_file_count(sb, val)		aal_set_le64(sb, sb_file_count, val)

#define get_sb_flushes(sb)			aal_get_le64(sb, sb_flushes)
#define set_sb_flushes(sb, val)			aal_set_le64(sb, sb_flushes, val)

#define get_sb_tree_height(sb)			aal_get_le16(sb, sb_tree_height)
#define set_sb_tree_height(sb, val)		aal_set_le16(sb, sb_tree_height, val)

#define get_sb_flags(sb)			aal_get_le64(sb, sb_flags)
#define set_sb_flags(sb, val)		        aal_set_le64(sb, sb_flags, val)

#endif
