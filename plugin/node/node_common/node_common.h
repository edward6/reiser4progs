/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node_common.h -- reiser4 node common code. */

#ifndef NODE_COMMON_H
#define NODE_COMMON_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

#define NODE_MAGIC 0x52344653

struct node {
	reiser4_plug_t *plug;
	aal_block_t *block;
	reiser4_plug_t *kplug;
};

typedef struct node node_t;

struct node_flush {
	d32_t mkfs_id;
	d64_t flush_id;
};

typedef struct node_flush node_flush_t;

/* Format of node header for node_common */
struct node_header {

	/* Plugin id */
	d16_t pid;
    
	/* The number of items */
	d16_t num_items;
    
	/* Node free space */
	d16_t free_space;

	/* Free space start */
	d16_t free_space_start;

	/* node magic 0x52344653 */
	d32_t magic;
    
	/* Node level */
	d8_t level;

	/* Node flush stamp */
	node_flush_t flush;
};

typedef struct node_header node_header_t;  

#define	nh(block)                         \
        ((node_header_t *)block->data)

#define nh_get_pid(node)                  \
        aal_get_le16(nh((node)->block), pid)

#define nh_set_pid(node, val)             \
        aal_set_le16(nh((node)->block), pid, val)

#define nh_get_level(node)                \
        (nh((node)->block)->level)

#define nh_set_level(node, val)           \
        (nh((node)->block)->level = val)

#define nh_get_magic(node)                \
        aal_get_le32(nh((node)->block), magic)

#define nh_set_magic(node, val)           \
        aal_set_le32(nh((node)->block), magic, val)

#define nh_set_mkfs_id(node, val)         \
        aal_set_le32(nh((node)->block), flush.mkfs_id, val)

#define nh_get_mkfs_id(node)              \
        aal_get_le32(nh((node)->block), flush.mkfs_id)

#define nh_set_flush_id(node, val)        \
        aal_set_le64(nh((node)->block), flush.flush_id, val)

#define nh_get_flush_id(node)             \
        aal_get_le64(nh((node)->block), flush.flush_id)

#define nh_get_num_items(node)            \
        aal_get_le16(nh((node)->block), num_items)

#define nh_set_num_items(node, val)       \
        aal_set_le16(nh((node)->block), num_items, val)

#define nh_get_free_space(node)           \
        aal_get_le16(nh((node)->block), free_space)

#define nh_set_free_space(node, val)      \
        aal_set_le16(nh((node)->block), free_space, val)

#define nh_get_free_space_start(node)     \
        aal_get_le16(nh((node)->block), free_space_start)

#define nh_set_free_space_start(node, val) \
        aal_set_le16(nh((node)->block), free_space_start, val)

#define nh_inc_free_space(node, val)      \
        nh_set_free_space(node, (nh_get_free_space(node) + (val)));

#define nh_dec_free_space(node, val)      \
        nh_set_free_space(node, (nh_get_free_space(node) - (val)));

#define nh_inc_free_space_start(node, val) \
	nh_set_free_space_start(node, (nh_get_free_space_start(node) + (val)));

#define nh_dec_free_space_start(node, val) \
	nh_set_free_space_start(node, (nh_get_free_space_start(node) - (val)));

#define nh_inc_num_items(node, val)       \
	nh_set_num_items(node, (nh_get_num_items(node) + (val)));

#define nh_dec_num_items(node, val)       \
	nh_set_num_items(node, (nh_get_num_items(node) - (val)));

extern node_entity_t *node_common_init(aal_block_t *block,
				       reiser4_plug_t *plug,
				       reiser4_plug_t *kplug);

extern int node_common_confirm(node_entity_t *entity);
extern errno_t node_common_fini(node_entity_t *entity);
extern uint16_t node_common_items(node_entity_t *entity);
extern uint8_t node_common_get_level(node_entity_t *entity);

#ifndef ENABLE_STAND_ALONE
extern void  node_common_move(node_entity_t *entity,
			      blk_t nr);

extern int node_common_isdirty(node_entity_t *entity);
extern void node_common_mkdirty(node_entity_t *entity);
extern void node_common_mkclean(node_entity_t *entity);

extern errno_t node_common_sync(node_entity_t *entity);
extern uint16_t node_common_space(node_entity_t *entity);

extern uint32_t node_common_get_mstamp(node_entity_t *entity);
extern uint64_t node_common_get_fstamp(node_entity_t *entity);

extern errno_t node_common_clone(node_entity_t *src_entity,
				 node_entity_t *dst_entity);

extern errno_t node_common_fresh(node_entity_t *entity,
				 uint8_t level);

extern void node_common_set_mstamp(node_entity_t *entity,
				   uint32_t stamp);

extern void node_common_set_fstamp(node_entity_t *entity,
				   uint64_t stamp);

extern void node_common_set_level(node_entity_t *entity,
				  uint8_t level);

extern void node_common_set_stamp(node_entity_t *entity,
				  uint32_t stamp);
#endif

#endif
