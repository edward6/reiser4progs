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

/* This flag indicates that backup should be updated
   (the update is performed by fsck) */
#define FORMAT40_UPDATE_BACKUP  (1 << 31)

#define format40_key_pid format40_get_key

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
	d32_t sb_node_pid;

	/* Reiser5 fields */
	d64_t sb_subvol_id;
	d64_t sb_num_subvols;

	d64_t sb_data_room;
	d64_t sb_volinfo_loc;
	d8_t  sb_num_sgs_bits;
	char  not_used[389];
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
	d64_t sb_reserved;
} __attribute__((packed)) format40_backup_t;
#endif


extern reiser4_format_plug_t format40_plug;
extern reiser4_core_t *format40_core;

#define get_sb_mkfs_id(sb)		aal_get_le32(sb, sb_mkfs_id)
#define set_sb_mkfs_id(sb, val)		aal_set_le32(sb, sb_mkfs_id, val)

#define get_sb_block_count(sb)		aal_get_le64(sb, sb_block_count)
#define set_sb_block_count(sb, val)	aal_set_le64(sb, sb_block_count, val)

#define get_sb_free_blocks(sb)		aal_get_le64(sb, sb_free_blocks)
#define set_sb_free_blocks(sb, val)	aal_set_le64(sb, sb_free_blocks, val)

#define get_sb_root_block(sb)		aal_get_le64(sb, sb_root_block)
#define set_sb_root_block(sb, val)	aal_set_le64(sb, sb_root_block, val)

#define get_sb_policy(sb)		aal_get_le16(sb, sb_policy)
#define set_sb_policy(sb, val)		aal_set_le16(sb, sb_policy, val)

/* FIXME: Should not be here, oid's stuff. */
#define get_sb_oid(sb)			aal_get_le64(sb, sb_oid[0])
#define get_sb_file_count(sb)		aal_get_le64(sb, sb_oid[1])

#define get_sb_flushes(sb)		aal_get_le64(sb, sb_flushes)
#define set_sb_flushes(sb, val)		aal_set_le64(sb, sb_flushes, val)

#define get_sb_tree_height(sb)		aal_get_le16(sb, sb_tree_height)
#define set_sb_tree_height(sb, val)	aal_set_le16(sb, sb_tree_height, val)

#define get_sb_flags(sb)		aal_get_le64(sb, sb_flags)
#define set_sb_flags(sb, val)		aal_set_le64(sb, sb_flags, val)

#define get_sb_node_pid(sb)		aal_get_le32(sb, sb_node_pid)
#define set_sb_node_pid(sb, val)	aal_set_le32(sb, sb_node_pid, val)

#define get_sb_version(sb)	\
	(aal_get_le32(sb, sb_version) & ~FORMAT40_UPDATE_BACKUP)

#define set_sb_version(sb, val)		aal_set_le32(sb, sb_version, val)

#define sb_update_backup(sb)	\
	(aal_get_le32(sb, sb_version) & FORMAT40_UPDATE_BACKUP)

#define FORMAT40_KEY_LARGE		0

#define format40_mkdirty(entity) \
	(((format40_t *)entity)->state |= (1 << ENTITY_DIRTY))

#define format40_mkclean(entity) \
	(((format40_t *)entity)->state &= ~(1 << ENTITY_DIRTY))

#ifndef ENABLE_MINIMAL
extern uint64_t format40_start(reiser4_format_ent_t *entity);
extern uint64_t format40_get_len(reiser4_format_ent_t *entity);
extern uint64_t format40_get_free(reiser4_format_ent_t *entity);
extern void format40_set_free(reiser4_format_ent_t *entity, uint64_t blocks);
extern uint32_t format40_get_stamp(reiser4_format_ent_t *entity);
extern void format40_set_stamp(reiser4_format_ent_t *entity, uint32_t mkfsid);
extern rid_t format40_get_policy(reiser4_format_ent_t *entity);
extern void format40_set_policy(reiser4_format_ent_t *entity, rid_t tail);
extern uint32_t format40_node_pid(reiser4_format_ent_t *entity);
extern uint32_t format40_get_state(reiser4_format_ent_t *entity);
extern void format40_set_state(reiser4_format_ent_t *entity, uint32_t state);
extern void format40_set_root(reiser4_format_ent_t *entity, uint64_t root);
extern void format40_set_len(reiser4_format_ent_t *entity, uint64_t blocks);
extern void format40_set_height(reiser4_format_ent_t *entity, uint16_t height);
extern rid_t format40_oid_pid(reiser4_format_ent_t *entity);
extern void format40_oid_area(reiser4_format_ent_t *entity, void **start,
			      uint32_t *len);
extern rid_t format40_journal_pid(reiser4_format_ent_t *entity);
extern rid_t format40_alloc_pid(reiser4_format_ent_t *entity);
extern errno_t format40_backup(reiser4_format_ent_t *entity,
			       backup_hint_t *hint);
extern uint32_t format40_version(reiser4_format_ent_t *entity);
extern errno_t format40_valid(reiser4_format_ent_t *entity);
extern errno_t format40_sync(reiser4_format_ent_t *entity);

extern void format40_set_key(reiser4_format_ent_t *entity, rid_t key);
extern errno_t format40_clobber_block(void *entity, blk_t start,
				      count_t width, void *data);
errno_t format40_layout(reiser4_format_ent_t *entity,
			region_func_t region_func,
			void *data);
extern void set_sb_format40(format40_super_t *super, format_hint_t *desc);
extern reiser4_format_ent_t *format40_create_common(aal_device_t *device,
					    format_hint_t *desc,
					    void (*set_sb)(format40_super_t *s,
							   format_hint_t *dsc));
#endif

extern errno_t check_super_format40(format40_super_t *super);
extern errno_t format40_super_open_common(format40_t *format,
					  errno_t (*check)(format40_super_t *s));
extern reiser4_format_ent_t *format40_open_common(aal_device_t *device,
					  uint32_t blksize,
					  reiser4_format_plug_t *plug,
					  errno_t (*super_open)(format40_t *f));
extern void format40_close(reiser4_format_ent_t *entity);
extern uint64_t format40_get_root(reiser4_format_ent_t *entity);
extern uint16_t format40_get_height(reiser4_format_ent_t *entity);
extern rid_t format40_get_key(reiser4_format_ent_t *entity);
#endif

/*
   Local variables:
   c-indentation-style: "K&R"
   mode-name: "LC"
   c-basic-offset: 8
   tab-width: 8
   fill-column: 80
   scroll-step: 1
   End:
*/
