/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node_common.c -- reiser4 node common code. */

#include "node_common.h"

#ifndef ENABLE_STAND_ALONE
int node_common_isdirty(node_entity_t *entity) {
	aal_assert("umka-2091", entity != NULL);
	return ((node_t *)entity)->block->dirty;
}

void node_common_mkdirty(node_entity_t *entity) {
	aal_assert("umka-2092", entity != NULL);
	((node_t *)entity)->block->dirty = 1;
}

void node_common_mkclean(node_entity_t *entity) {
	aal_assert("umka-2093", entity != NULL);
	((node_t *)entity)->block->dirty = 0;
}
#endif

/* Returns node level */
uint8_t node_common_get_level(node_entity_t *entity) {
	aal_assert("umka-1116", entity != NULL);
	return nh_get_level((node_t *)entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns node make stamp */
uint32_t node_common_get_mstamp(node_entity_t *entity) {
	aal_assert("umka-1127", entity != NULL);
	return nh_get_mkfs_id((node_t *)entity);
}

/* Returns node flush stamp */
uint64_t node_common_get_fstamp(node_entity_t *entity) {
	aal_assert("vpf-645", entity != NULL);
	return nh_get_flush_id((node_t *)entity);
}

errno_t node_common_clone(node_entity_t *src_entity,
			  node_entity_t *dst_entity)
{
	aal_assert("umka-2308", src_entity != NULL);
	aal_assert("umka-2309", dst_entity != NULL);

	aal_memcpy(((node_t *)dst_entity)->block->data,
		   ((node_t *)src_entity)->block->data,
		   ((node_t *)src_entity)->block->size);

	return 0;
}

errno_t node_common_fresh(node_entity_t *entity,
			  uint8_t level)
{
	node_t *node;
	uint32_t header;

	aal_assert("umka-2374", entity != NULL);
	
	node = (node_t *)entity;
	
	nh_set_pid(node, 0);
	nh_set_num_items(node, 0);
	nh_set_level(node, level);

	header = sizeof(node_header_t);
	nh_set_magic(node, NODE_MAGIC);

	nh_set_free_space_start(node, header);
	nh_set_free_space(node, node->block->size - header);

	return 0;
}

/* Saves node to device */
errno_t node_common_sync(node_entity_t *entity) {
	errno_t res;
	
	aal_assert("umka-1552", entity != NULL);
	
	if ((res = aal_block_write(((node_t *)entity)->block)))
		return res;

	node_common_mkclean(entity);
	return 0;
}

void  node_common_move(node_entity_t *entity, blk_t nr) {
	aal_block_t *block;
	
	aal_assert("umka-2377", entity != NULL);

	block = ((node_t *)entity)->block;
	aal_block_move(block, block->device, nr);
}
#endif

node_entity_t *node_common_init(aal_block_t *block,
				reiser4_plug_t *plug,
				reiser4_plug_t *kplug)
{
	node_t *node;
	
	aal_assert("umka-2376", kplug != NULL);
	aal_assert("umka-2375", block != NULL);
	
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	node->plug = plug;
	node->kplug = kplug;
	node->block = block;
	
	return (node_entity_t *)node;
}

/* Closes node by means of closing its block */
errno_t node_common_fini(node_entity_t *entity) {
	aal_assert("umka-825", entity != NULL);

	aal_block_free(((node_t *)entity)->block);
	aal_free(entity);
	return 0;
}

/* Confirms that passed node corresponds current plugin */
int node_common_confirm(node_entity_t *entity) {
	aal_assert("vpf-014", entity != NULL);
	return (nh_get_magic((node_t *)entity) == NODE_MAGIC);
}

/* Returns item number in passed node entity. Used for any loops through the all
   node items. */
uint16_t node_common_items(node_entity_t *entity) {
	aal_assert("vpf-018", entity != NULL);
	return nh_get_num_items((node_t *)entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns node free space */
uint16_t node_common_space(node_entity_t *entity) {
	aal_assert("vpf-020", entity != NULL);
	return nh_get_free_space((node_t *)entity);
}

/* Returns node make stamp */
void node_common_set_mstamp(node_entity_t *entity,
			    uint32_t stamp)
{
	aal_assert("vpf-644", entity != NULL);
	
	node_common_mkdirty(entity);
	nh_set_mkfs_id((node_t *)entity, stamp);
}

/* Returns node flush stamp */
void node_common_set_fstamp(node_entity_t *entity,
			    uint64_t stamp)
{
	aal_assert("vpf-643", entity != NULL);
	
	node_common_mkdirty(entity);
	nh_set_flush_id((node_t *)entity, stamp);
}

void node_common_set_level(node_entity_t *entity,
			   uint8_t level)
{
	aal_assert("umka-1864", entity != NULL);
	
	node_common_mkdirty(entity);
	nh_set_level((node_t *)entity, level);
}

/* Updating node stamp */
void node_common_set_stamp(node_entity_t *entity,
			   uint32_t stamp)
{
	aal_assert("umka-1126", entity != NULL);

	node_common_mkdirty(entity);
	nh_set_mkfs_id(((node_t *)entity), stamp);
}
#endif
