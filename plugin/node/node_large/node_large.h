/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node_large.h -- reiser4 node with large keys. */

#ifndef NODE_LARGE_H
#define NODE_LARGE_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

/* (*(__u32 *)"R4FS") */
#define NODE_LARGE_MAGIC	0x52344653

struct node_large {
	reiser4_plugin_t *plugin;

#ifndef ENABLE_STAND_ALONE
	int dirty;
#endif

	blk_t number;
	uint32_t size;
	aal_block_t *block;
	aal_device_t *device;
};

typedef struct node_large node_large_t;

struct node_large_flush {
	d32_t mkfs_id;
	d64_t flush_id;
};

typedef struct node_large_flush node_large_flush_t;

/* Format of node header for node_large */
struct node_large_header {

	/* The node common header */
	node_header_t h;
    
	/* The number of items */
	d16_t num_items;
    
	/* Node free space */
	d16_t free_space;

	/* Free space start */
	d16_t free_space_start;

	/* Node_Large magic 0x52344653 */
	d32_t magic;
    
	/* Node level (is not used in libreiser4) */
	d8_t level;

	/* Node flush stamp */
	node_large_flush_t flush;
};

typedef struct node_large_header node_large_header_t;  

#define	nh(block)                       \
        ((node_large_header_t *)block->data)

#define nh_get_pid(node)                \
        aal_get_le16(&nh((node)->block)->h, pid)

#define nh_set_pid(node, val)           \
        aal_set_le16(&nh((node)->block)->h, pid, val)

#define nh_get_level(node)              \
        (nh((node)->block)->level)

#define nh_set_level(node, val)         \
        (nh((node)->block)->level = val)

#define nh_get_magic(node)              \
        aal_get_le32(nh((node)->block), magic)

#define nh_set_magic(node, val)         \
        aal_set_le32(nh((node)->block), magic, val)

#define nh_set_mkfs_id(node, val)       \
        aal_set_le32(nh((node)->block), flush.mkfs_id, val)

#define nh_get_mkfs_id(node)            \
        aal_get_le32(nh((node)->block), flush.mkfs_id)

#define nh_set_flush_id(node, val)      \
        aal_set_le64(nh((node)->block), flush.flush_id, val)

#define nh_get_flush_id(node)           \
        aal_get_le64(nh((node)->block), flush.flush_id)

#define nh_get_num_items(node)          \
        aal_get_le16(nh((node)->block), num_items)

#define nh_set_num_items(node, val)     \
        aal_set_le16(nh((node)->block), num_items, val)

#define nh_get_free_space(node)         \
        aal_get_le16(nh((node)->block), free_space)

#define nh_set_free_space(node, val)    \
        aal_set_le16(nh((node)->block), free_space, val)

#define nh_get_free_space_start(node)   \
        aal_get_le16(nh((node)->block), free_space_start)

#define nh_set_free_space_start(node, val) \
        aal_set_le16(nh((node)->block), free_space_start, val)

#define nh_inc_free_space(node, val)    \
        nh_set_free_space(node, (nh_get_free_space(node) + (val)));

#define nh_dec_free_space(node, val)    \
        nh_set_free_space(node, (nh_get_free_space(node) - (val)));

#define nh_inc_free_space_start(node, val) \
	nh_set_free_space_start(node, (nh_get_free_space_start(node) + (val)));

#define nh_dec_free_space_start(node, val) \
	nh_set_free_space_start(node, (nh_get_free_space_start(node) - (val)));

#define nh_inc_num_items(node, val)     \
	nh_set_num_items(node, (nh_get_num_items(node) + (val)));

#define nh_dec_num_items(node, val)     \
	nh_set_num_items(node, (nh_get_num_items(node) - (val)));

union key {
	d64_t el[4];
	int pad;
};

typedef union key key_t;

struct item_header {
	key_t key;
    
	d16_t offset;
	d16_t flags;
	d16_t pid;
};

typedef struct item_header item_header_t;

#define ih_get_offset(ih)	        \
        aal_get_le16(ih, offset)

#define ih_set_offset(ih, val)          \
        aal_set_le16(ih, offset, val);

#define ih_inc_offset(ih, val)          \
        ih_set_offset(ih, (ih_get_offset(ih) + (val)))

#define ih_dec_offset(ih, val)          \
        ih_set_offset(ih, (ih_get_offset(ih) - (val)))

#define ih_clear_flag(ih, flag)         \
        aal_clear_bit(ih->flags, flag)

#define ih_test_flag(ih, flag)          \
        aal_test_bit(ih->flags, flag)

#define ih_set_flag(ih, flag)	        \
        aal_set_bit(ih->flags, flag)

#define ih_get_pid(ih)	                \
        aal_get_le16(ih, pid)

#define ih_set_pid(ih, val)	        \
        aal_set_le16(ih, pid, (val))

#define loaded(entity) \
        (((node_large_t *)entity)->block != NULL)

extern item_header_t *node_large_ih_at(node_large_t *node,
				       uint32_t pos);

extern uint16_t node_large_free_space_end(node_large_t *node);
extern void *node_large_ib_at(node_large_t *node, uint32_t pos);
#endif
