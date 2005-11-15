/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40.h -- reiser4 node plugin structures. */

#ifndef NODE40_H
#define NODE40_H

#include <aal/libaal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

#define NODE40_MAGIC 0x52344653

extern reiser4_node_plug_t node40_plug;

/* Format of node header for node_common */
typedef struct node40_header {

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
}  __attribute__((packed)) node40_header_t;  

extern reiser4_core_t *node40_core;

typedef int64_t (*modify_func_t) (reiser4_place_t *place,
				  trans_hint_t *hint);

extern void node40_mkdirty(reiser4_node_t *entity);
extern void node40_mkclean(reiser4_node_t *entity);
extern int node40_isdirty(reiser4_node_t *entity);

extern uint16_t node40_space(reiser4_node_t *entity);
extern uint32_t node40_items(reiser4_node_t *entity);

extern uint16_t node40_free_space_end(reiser4_node_t *entity);
extern void *node40_ih_at(reiser4_node_t *entity, uint32_t pos);
extern void *node40_ib_at(reiser4_node_t *entity, uint32_t pos);

extern uint8_t node40_get_level(reiser4_node_t *entity);
extern uint16_t node40_len(reiser4_node_t *entity, pos_t *pos);

extern uint32_t node40_size(reiser4_node_t *node, pos_t *pos,
			    uint32_t count);

extern errno_t node40_fetch(reiser4_node_t *entity,
			    pos_t *pos, reiser4_place_t *place);

extern int64_t node40_modify(reiser4_node_t *entity,
			     pos_t *pos, trans_hint_t *hint,
			     modify_func_t modify_func);

extern errno_t node40_expand(reiser4_node_t *entity, pos_t *pos,
			     uint32_t len, uint32_t count);

extern errno_t node40_shrink(reiser4_node_t *entity, pos_t *pos, 
			     uint32_t len, uint32_t count);

extern errno_t node40_copy(reiser4_node_t *dst_entity, pos_t *dst_pos,
			   reiser4_node_t *src_entity, pos_t *src_pos, 
			   uint32_t count);

extern void node40_set_flags(reiser4_node_t *entity, 
			     uint32_t pos, uint16_t flags);

extern uint16_t node40_get_flag(reiser4_node_t *entity, uint32_t pos);

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
typedef union key3 {
	d64_t el[3];
	int pad;
} key3_t;

typedef struct item_header3 {
	key3_t key3;
    
	d16_t offset;
	d16_t flags;
	d16_t pid;
} __attribute__((packed)) item_header3_t;
#endif

#ifdef ENABLE_LARGE_KEYS
typedef union key4 {
	d64_t el[4];
	int pad;
} key4_t;

typedef struct item_header4 {
	key4_t key4;
    
	d16_t offset;
	d16_t flags;
	d16_t pid;
} __attribute__((packed)) item_header4_t;
#endif

#if defined(ENABLE_SHORT_KEYS) && defined(ENABLE_LARGE_KEYS)
#define ih_size(pol)						\
        ((pol == 3) ?						\
	 sizeof(item_header3_t) :				\
         sizeof(item_header4_t))

#define key_size(pol)						\
        ((pol == 3) ?						\
	 sizeof(key3_t) :					\
	 sizeof(key4_t))

#define ih_get_offset(ih, pol)					\
        ((pol == 3) ?						\
	 aal_get_le16((item_header3_t *)(ih), offset) :		\
         aal_get_le16((item_header4_t *)(ih), offset))

#define ih_set_offset(ih, val, pol)				\
        ((pol == 3) ?						\
	 aal_set_le16((item_header3_t *)(ih), offset, val) :	\
         aal_set_le16((item_header4_t *)(ih), offset, val))

#define ih_get_flags(ih, pol)					\
        ((pol == 3) ?						\
         aal_get_le16((item_header3_t *)(ih), flags) :		\
	 aal_get_le16((item_header4_t *)(ih), flags))

#define ih_set_flags(ih, val, pol)				\
        ((pol == 3) ?						\
         aal_set_le16((item_header3_t *)(ih), flags, val) :	\
	 aal_set_le16((item_header4_t *)(ih), flags, val))

#define ih_get_pid(ih, pol)					\
        ((pol == 3) ?						\
         aal_get_le16((item_header3_t *)(ih), pid) :		\
	 aal_get_le16((item_header4_t *)(ih), pid))

#define ih_set_pid(ih, val, pol)				\
        ((pol == 3) ?						\
         aal_set_le16((item_header3_t *)(ih), pid, val) :	\
	 aal_set_le16((item_header4_t *)(ih), pid, val))
#else
#if defined(ENABLE_SHORT_KEYS)
#define ih_size(pol)						\
        sizeof(item_header3_t)

#define key_size(pol)						\
        sizeof(key3_t)

#define ih_get_offset(ih, pol)					\
        aal_get_le16((item_header3_t *)(ih), offset)

#define ih_set_offset(ih, val, pol)				\
	aal_set_le16((item_header3_t *)(ih), offset, val)

#define ih_get_flags(ih, pol)					\
        aal_get_le16((item_header3_t *)(ih), flags)

#define ih_set_flags(ih, val, pol)				\
        aal_set_le16((item_header3_t *)(ih), flags, val)

#define ih_get_pid(ih, pol)					\
        aal_get_le16((item_header3_t *)(ih), pid)

#define ih_set_pid(ih, val, pol)				\
        aal_set_le16((item_header3_t *)(ih), pid, val)
#else
#define ih_size(pol)						\
        sizeof(item_header4_t)

#define key_size(pol)						\
        sizeof(key4_t)

#define ih_get_offset(ih, pol)					\
        aal_get_le16((item_header4_t *)(ih), offset)

#define ih_set_offset(ih, val, pol)				\
	aal_set_le16((item_header4_t *)(ih), offset, val)

#define ih_get_flags(ih, pol)					\
        aal_get_le16((item_header4_t *)(ih), flags)

#define ih_set_flags(ih, val, pol)				\
        aal_set_le16((item_header4_t *)(ih), flags, val)

#define ih_get_pid(ih, pol)					\
        aal_get_le16((item_header4_t *)(ih), pid)

#define ih_set_pid(ih, val, pol)				\
        aal_set_le16((item_header4_t *)(ih), pid, val)
#endif
#endif

#define ih_inc_offset(ih, val, pol)				\
        ih_set_offset((ih), (ih_get_offset((ih), pol) +		\
        (val)), pol)

#define ih_dec_offset(ih, val, pol)				\
        ih_set_offset((ih), (ih_get_offset((ih), pol) -		\
        (val)), pol)

#endif
