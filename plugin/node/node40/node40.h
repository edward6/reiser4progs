/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40.h -- reiser4 node plugin structures. */

#ifndef NODE40_H
#define NODE40_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

#define NODE40_MAGIC 0x52344653

struct node40 {
	reiser4_plug_t *plug;
	aal_block_t *block;
	reiser4_plug_t *kplug;
};

typedef struct node40 node40_t;

/* Format of node header for node_common */
struct node40_header {

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
	
	/* id common for all nodes, generated at mkfs time. */
	d32_t mkfs_id;
	
	/* id of the flush the node was written at. */
	d64_t flush_id;
	
	/* Useful flags. */
	d16_t flags;

	/* Node level */
	d8_t level;
	
	d8_t pad;
}  __attribute__((packed));

typedef struct node40_header node40_header_t;  

extern int node40_isdirty(node_entity_t *entity);
extern void node40_mkdirty(node_entity_t *entity);
extern void node40_mkclean(node_entity_t *entity);

extern inline uint32_t node40_key_pol(node40_t *node);

extern uint32_t node40_size(node40_t *node, pos_t *pos,
			    uint32_t count);

extern errno_t node40_fetch(node_entity_t *entity,
			    pos_t *pos, place_t *place);

extern errno_t node40_expand(node_entity_t *entity, pos_t *pos,
			     uint32_t len, uint32_t count);

extern errno_t node40_shrink(node_entity_t *entity, pos_t *pos, 
			     uint32_t len, uint32_t count);

extern errno_t node40_copy(node_entity_t *dst_entity, pos_t *dst_pos,
			   node_entity_t *src_entity, pos_t *src_pos, 
			   uint32_t count);

#define	nh(block)                         \
        ((node40_header_t *)block->data)

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
        aal_set_le32(nh((node)->block), mkfs_id, val)

#define nh_get_mkfs_id(node)              \
        aal_get_le32(nh((node)->block), mkfs_id)

#define nh_set_flush_id(node, val)        \
        aal_set_le64(nh((node)->block), flush_id, val)

#define nh_get_flush_id(node)             \
        aal_get_le64(nh((node)->block), flush_id)

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

#ifdef ENABLE_SHORT_KEYS
union key3 {
	d64_t el[3];
	int pad;
};

typedef union key3 key3_t;

struct item_header3 {
	key3_t key3;
    
	d16_t offset;
	d16_t flags;
	d16_t pid;
} __attribute__((packed));

typedef struct item_header3 item_header3_t;
#endif

#ifdef ENABLE_LARGE_KEYS
union key4 {
	d64_t el[4];
	int pad;
};

typedef union key4 key4_t;

struct item_header4 {
	key4_t key4;
    
	d16_t offset;
	d16_t flags;
	d16_t pid;
} __attribute__((packed));

typedef struct item_header4 item_header4_t;
#endif

#if defined(ENABLE_SHORT_KEYS) && defined(ENABLE_LARGE_KEYS)
#define ih_size(pol)                                        \
        ((pol == 3) ?                                       \
	 sizeof(item_header3_t) :                           \
         sizeof(item_header4_t))

#define key_size(pol)                                       \
        ((pol == 3) ?                                       \
	 sizeof(key3_t) :                                   \
	 sizeof(key4_t))

#define ih_get_offset(ih, pol)	                            \
        ((pol == 3) ?                                       \
	 aal_get_le16((item_header3_t *)(ih), offset) :     \
         aal_get_le16((item_header4_t *)(ih), offset))

#define ih_set_offset(ih, val, pol)                         \
        ((pol == 3) ?                                       \
	 aal_set_le16((item_header3_t *)(ih), offset, val) :\
         aal_set_le16((item_header4_t *)(ih), offset, val))

#define ih_get_flags(ih, pol)                               \
        ((pol == 3) ?                                       \
         aal_get_le16((item_header3_t *)(ih), flags) :      \
	 aal_get_le16((item_header4_t *)(ih), flags))

#define ih_set_flags(ih, val, pol)                          \
        ((pol == 3) ?                                       \
         aal_set_le16((item_header3_t *)(ih), flags, val) : \
	 aal_set_le16((item_header4_t *)(ih), flags, val))

#define ih_get_pid(ih, pol)                                 \
        ((pol == 3) ?                                       \
         aal_get_le16((item_header3_t *)(ih), pid) :        \
	 aal_get_le16((item_header4_t *)(ih), pid))

#define ih_set_pid(ih, val, pol)                            \
        ((pol == 3) ?                                       \
         aal_set_le16((item_header3_t *)(ih), pid, val) :   \
	 aal_set_le16((item_header4_t *)(ih), pid, val))
#else
#if defined(ENABLE_SHORT_KEYS)
#define ih_size(pol)                                        \
        sizeof(item_header3_t)

#define key_size(pol)                                       \
        sizeof(key3_t)

#define ih_get_offset(ih, pol)	                            \
        aal_get_le16((item_header3_t *)(ih), offset)

#define ih_set_offset(ih, val, pol)                         \
	aal_set_le16((item_header3_t *)(ih), offset, val)

#define ih_get_flags(ih, pol)                               \
        aal_get_le16((item_header3_t *)(ih), flags)

#define ih_set_flags(ih, val, pol)                          \
        aal_set_le16((item_header3_t *)(ih), flags, val)

#define ih_get_pid(ih, pol)                                 \
        aal_get_le16((item_header3_t *)(ih), pid)

#define ih_set_pid(ih, val, pol)                            \
        aal_set_le16((item_header3_t *)(ih), pid, val)
#else
#define ih_size(pol)                                        \
        sizeof(item_header4_t)

#define key_size(pol)                                       \
        sizeof(key4_t)

#define ih_get_offset(ih, pol)	                            \
        aal_get_le16((item_header4_t *)(ih), offset)

#define ih_set_offset(ih, val, pol)                         \
	aal_set_le16((item_header4_t *)(ih), offset, val)

#define ih_get_flags(ih, pol)                               \
        aal_get_le16((item_header4_t *)(ih), flags)

#define ih_set_flags(ih, val, pol)                          \
        aal_set_le16((item_header4_t *)(ih), flags, val)

#define ih_get_pid(ih, pol)                                 \
        aal_get_le16((item_header4_t *)(ih), pid)

#define ih_set_pid(ih, val, pol)                            \
        aal_set_le16((item_header4_t *)(ih), pid, val)
#endif
#endif

#define ih_inc_offset(ih, val, pol)                        \
        ih_set_offset((ih), (ih_get_offset((ih), pol) +    \
        (val)), pol)

#define ih_dec_offset(ih, val, pol)                        \
        ih_set_offset((ih), (ih_get_offset((ih), pol) -    \
        (val)), pol)

#define ih_clear_flag(ih, flag, pol)                       \
        ih_set_flags(ih, (ih_get_flags(ih, pol) & ~flag),  \
        pol)

#define ih_set_flag(ih, flag, pol)                         \
        ih_set_flags(ih, (ih_get_flags(ih, pol) | flag),   \
        pol)

#define ih_test_flag(ih, flag, pol)                        \
        ({uint16_t flags = ih_get_flags(ih, pol);          \
        aal_test_bit(&flags, flag);})

extern uint16_t node40_free_space_end(node40_t *node);
extern void *node40_ih_at(node40_t *node, uint32_t pos);
extern void *node40_ib_at(node40_t *node, uint32_t pos);

#endif
