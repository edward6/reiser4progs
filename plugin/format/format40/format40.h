/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40.h -- reiser4 disk-format plugin. */

#ifndef FORMAT40_H
#define FORMAT40_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

#define FORMAT40_MAGIC "ReIsEr40FoRmAt"

#define FORMAT40_BLOCKNR(blksize) \
        (REISER4_MASTER_BLOCKNR(blksize) + 1)

#define SUPER(entity) (&((format40_t *)entity)->super)

#define MAGIC_SIZE 16

#define FORMAT40_VERSION	1
#define FORMAT40_COMPATIBLE	1

typedef struct format40_super {
	d64_t sb_block_count;
	d64_t sb_free_blocks;
	d64_t sb_root_block;
	
	/* These 2 fields are for oid data. */
	d64_t sb_oid[2];
	
	d64_t sb_flushes;
    
	d32_t sb_mkfs_id;
	char sb_magic[MAGIC_SIZE];

	d16_t sb_tree_height;
	d16_t sb_policy;
	d64_t sb_flags;
	
	d32_t sb_version;
	d32_t sb_compatible;
	
	char sb_unused[424];
} __attribute__((packed)) format40_super_t;

typedef struct format40 {
	reiser4_format_plug_t *plug;

	uint32_t state;
	uint32_t blksize;
	
	aal_device_t *device;
	format40_super_t super;
} format40_t;

#ifndef ENABLE_MINIMAL
typedef struct format40_backup {
	char sb_magic[MAGIC_SIZE];
	d64_t sb_block_count;
	d32_t sb_mkfs_id;
	d16_t sb_policy;
	d64_t sb_flags;
	d32_t sb_version;
	d32_t sb_compatible;
	d64_t sb_reserved;
} __attribute__((packed)) format40_backup_t;
#endif


extern reiser4_format_plug_t format40_plug;
extern reiser4_core_t *format40_core;

#define get_sb_mkfs_id(sb)			aal_get_le32(sb, sb_mkfs_id)
#define set_sb_mkfs_id(sb, val)			aal_set_le32(sb, sb_mkfs_id, val)

#define get_sb_block_count(sb)			aal_get_le64(sb, sb_block_count)
#define set_sb_block_count(sb, val)		aal_set_le64(sb, sb_block_count, val)

#define get_sb_free_blocks(sb)			aal_get_le64(sb, sb_free_blocks)
#define set_sb_free_blocks(sb, val)		aal_set_le64(sb, sb_free_blocks, val)

#define get_sb_root_block(sb)			aal_get_le64(sb, sb_root_block)
#define set_sb_root_block(sb, val)		aal_set_le64(sb, sb_root_block, val)

#define get_sb_policy(sb)			aal_get_le16(sb, sb_policy)
#define set_sb_policy(sb, val)			aal_set_le16(sb, sb_policy, val)

/* FIXME: Should not be here, oid's stuff. */
#define get_sb_oid(sb)                         aal_get_le64(sb, sb_oid[0])
#define get_sb_file_count(sb)                  aal_get_le64(sb, sb_oid[1])

#define get_sb_flushes(sb)			aal_get_le64(sb, sb_flushes)
#define set_sb_flushes(sb, val)			aal_set_le64(sb, sb_flushes, val)

#define get_sb_tree_height(sb)			aal_get_le16(sb, sb_tree_height)
#define set_sb_tree_height(sb, val)		aal_set_le16(sb, sb_tree_height, val)

#define get_sb_flags(sb)			aal_get_le64(sb, sb_flags)
#define set_sb_flags(sb, val)		        aal_set_le64(sb, sb_flags, val)

#define get_sb_version(sb)			aal_get_le32(sb, sb_version)
#define set_sb_version(sb, val)			aal_set_le32(sb, sb_version, val)

#define get_sb_compatible(sb)			aal_get_le32(sb, sb_compatible)
#define set_sb_compatible(sb, val)		aal_set_le32(sb, sb_compatible, val)

#define FORMAT40_KEY_LARGE	0

#define format40_mkdirty(entity) \
	(((format40_t *)entity)->state |= (1 << ENTITY_DIRTY))

#define format40_mkclean(entity) \
	(((format40_t *)entity)->state &= ~(1 << ENTITY_DIRTY))

#ifndef ENABLE_MINIMAL
extern void format40_set_key(reiser4_format_ent_t *entity, rid_t key);
extern rid_t format40_get_key(reiser4_format_ent_t *entity);
#endif

#endif
