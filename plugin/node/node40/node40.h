/*
  node40.h -- reiser4 default node structures.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef NODE40_H
#define NODE40_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

/* (*(__u32 *)"R4FS") */
#define NODE40_MAGIC	0x52344653

struct node40 {
	reiser4_plugin_t *plugin;

	blk_t blk;
	
#ifndef ENABLE_STAND_ALONE
	int dirty;
#endif

	uint32_t size;
	aal_block_t *block;
	aal_device_t *device;
};

typedef struct node40 node40_t;

struct node40_flush {
	d32_t mkfs_id;
	d64_t flush_id;
};

typedef struct node40_flush node40_flush_t;

/* Format of node header for node40 */
struct node40_header {

	/* The node common header */
	node_header_t h;
    
	/* The number of items */
	d16_t num_items;
    
	/* Node free space */
	d16_t free_space;

	/* Free space start */
	d16_t free_space_start;

	/* Node40 magic 0x52344653 */
	d32_t magic;
    
	/* Node level (is not used in libreiser4) */
	d8_t level;

	/* Node flush stamp */
	node40_flush_t flush;
};

typedef struct node40_header node40_header_t;  

#define	nh40(block) \
        ((node40_header_t *)block->data)

#define nh40_get_pid(node) \
        aal_get_le16(&nh40((node)->block)->h, pid)

#define nh40_set_pid(node, val) \
        aal_set_le16(&nh40((node)->block)->h, pid, val)

#define nh40_get_level(node) \
        (nh40((node)->block)->level)

#define nh40_set_level(node, val) \
        (nh40((node)->block)->level = val)

#define nh40_get_magic(node) \
        aal_get_le32(nh40((node)->block), magic)

#define nh40_set_magic(node, val) \
        aal_set_le32(nh40((node)->block), magic, val)

#define nh40_set_mkfs_id(node, val) \
        aal_set_le32(nh40((node)->block), flush.mkfs_id, val)

#define nh40_get_mkfs_id(node) \
        aal_get_le32(nh40((node)->block), flush.mkfs_id)

#define nh40_set_flush_id(node, val) \
        aal_set_le64(nh40((node)->block), flush.flush_id, val)

#define nh40_get_flush_id(node) \
        aal_get_le64(nh40((node)->block), flush.flush_id)

#define nh40_get_num_items(node) \
        aal_get_le16(nh40((node)->block), num_items)

#define nh40_set_num_items(node, val) \
        aal_set_le16(nh40((node)->block), num_items, val)

#define nh40_get_free_space(node) \
        aal_get_le16(nh40((node)->block), free_space)

#define nh40_set_free_space(node, val) \
        aal_set_le16(nh40((node)->block), free_space, val)

#define nh40_get_free_space_start(node) \
        aal_get_le16(nh40((node)->block), free_space_start)

#define nh40_set_free_space_start(node, val) \
        aal_set_le16(nh40((node)->block), free_space_start, val)

#define nh40_inc_free_space(node, val) \
        nh40_set_free_space(node, (nh40_get_free_space(node) + (val)));

#define nh40_dec_free_space(node, val) \
        nh40_set_free_space(node, (nh40_get_free_space(node) - (val)));

#define nh40_inc_free_space_start(node, val) \
	nh40_set_free_space_start(node, (nh40_get_free_space_start(node) + (val)));

#define nh40_dec_free_space_start(node, val) \
	nh40_set_free_space_start(node, (nh40_get_free_space_start(node) - (val)));

#define nh40_inc_num_items(node, val) \
	nh40_set_num_items(node, (nh40_get_num_items(node) + (val)));

#define nh40_dec_num_items(node, val) \
	nh40_set_num_items(node, (nh40_get_num_items(node) - (val)));

union key40 {
	d64_t el[3];
	int pad;
};

typedef union key40 key40_t;

struct item40_header {
	key40_t key;
    
	d16_t offset;
	d16_t len;
	d16_t pid;
};

typedef struct item40_header item40_header_t;

#define ih40_get_offset(ih)	  aal_get_le16(ih, offset)
#define ih40_set_offset(ih, val)  aal_set_le16(ih, offset, val);

#define ih40_inc_offset(ih, val)  ih40_set_offset(ih, (ih40_get_offset(ih) + (val)))
#define ih40_dec_offset(ih, val)  ih40_set_offset(ih, (ih40_get_offset(ih) - (val)))

#define ih40_get_len(ih)	  aal_get_le16(ih, len)
#define ih40_set_len(ih, val)	  aal_set_le16(ih, len, val)

#define ih40_inc_len(ih, val)     ih40_set_len(ih, (ih40_get_len(ih) + (val)))
#define ih40_dec_len(ih, val)     ih40_set_len(ih, (ih40_get_len(ih) - (val)))

#define ih40_get_pid(ih)	  aal_get_le16(ih, pid)
#define ih40_set_pid(ih, val)	  aal_set_le16(ih, pid, (val))

extern uint16_t node40_free_space_end(node40_t *node);
extern void *node40_ib_at(node40_t *node, uint32_t pos);
extern item40_header_t *node40_ih_at(node40_t *node, uint32_t pos);

#define node40_loaded(entity)	  (((node40_t *)entity)->block != NULL)

#endif
