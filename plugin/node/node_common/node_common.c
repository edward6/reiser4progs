/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node_common.c -- reiser4 node common code. */

#include "node_common.h"

#ifndef ENABLE_STAND_ALONE
int node_common_isdirty(object_entity_t *entity) {
	aal_assert("umka-2091", entity != NULL);
	return ((node_t *)entity)->dirty;
}

void node_common_mkdirty(object_entity_t *entity) {
	aal_assert("umka-2092", entity != NULL);
	((node_t *)entity)->dirty = 1;
}

void node_common_mkclean(object_entity_t *entity) {
	aal_assert("umka-2093", entity != NULL);
	((node_t *)entity)->dirty = 0;
}
#endif

/* Returns node level */
uint8_t node_common_get_level(object_entity_t *entity) {
	aal_assert("umka-1116", entity != NULL);
	aal_assert("umka-2017", loaded(entity));
	
	return nh_get_level((node_t *)entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns node make stamp */
uint32_t node_common_get_mstamp(object_entity_t *entity) {
	aal_assert("umka-1127", entity != NULL);
	aal_assert("umka-2018", loaded(entity));
	
	return nh_get_mkfs_id((node_t *)entity);
}

/* Returns node flush stamp */
uint64_t node_common_get_fstamp(object_entity_t *entity) {
	aal_assert("vpf-645", entity != NULL);
	aal_assert("umka-2019", loaded(entity));
	
	return nh_get_flush_id((node_t *)entity);
}
#endif

/* Creates node_common entity on specified device and block number. This can be
   used later for working with all node methods. */
object_entity_t *node_common_init(aal_device_t *device,
				  uint32_t size, blk_t blk)
{
	node_t *node;
    
	aal_assert("umka-806", device != NULL);

	/* Allocating memory for the entity */
	if (!(node = aal_calloc(sizeof(*node), 0)))
		return NULL;

	node->size = size;
	node->number = blk;
	node->device = device;

	return (object_entity_t *)node;
}

#ifndef ENABLE_STAND_ALONE
errno_t node_common_clone(object_entity_t *src_entity,
			  object_entity_t *dst_entity)
{
	node_t *src_node, *dst_node;
	
	aal_assert("umka-2308", src_entity != NULL);
	aal_assert("umka-2309", dst_entity != NULL);
	aal_assert("umka-2310", loaded(src_entity));
	aal_assert("umka-2311", loaded(dst_entity));

	src_node = (node_t *)src_entity;
	dst_node = (node_t *)dst_entity;
	
	aal_memcpy(dst_node->block->data,
		   src_node->block->data,
		   src_node->block->size);

	return 0;
}

void node_common_move(object_entity_t *entity,
		      blk_t number)
{
	node_t *node;
	
	aal_assert("umka-2249", entity != NULL);
	aal_assert("umka-2012", loaded(entity));

	node = (node_t *)entity;
	node->number = number;
	
	aal_block_move(node->block, number);
}

/* Opens node on passed device and block number */
errno_t node_common_form(object_entity_t *entity,
			 uint8_t level)
{
	node_t *node;
	uint32_t header;
    
	aal_assert("umka-2013", entity != NULL);

	node = (node_t *)entity;
	
	if (node->block == NULL) {
		if (!(node->block = aal_block_create(node->device,
						     node->size,
						     node->number, 0)))
		{
			return -ENOMEM;
		}
	}

	nh_set_pid(node, 0);
	nh_set_num_items(node, 0);
	nh_set_level(node, level);
	nh_set_magic(node, NODE_MAGIC);

	header = sizeof(node_header_t);
	nh_set_free_space_start(node, header);
	nh_set_free_space(node, node->size - header);

	node->dirty = 1;
	return 0;
}
#endif

errno_t node_common_load(object_entity_t *entity) {
	node_t *node;
	
	aal_assert("umka-2010", entity != NULL);
	
	node = (node_t *)entity;
	
	if ((node = (node_t *)entity)->block)
		return 0;

	if (!(node->block = aal_block_read(node->device,
					   node->size,
					   node->number)))
	{
		return -EIO;
	}

#ifndef ENABLE_STAND_ALONE
	node->dirty = 0;
#endif
	return 0;
}

errno_t node_common_unload(object_entity_t *entity) {
	node_t *node;
	
	aal_assert("umka-2011", entity != NULL);
	aal_assert("umka-2012", loaded(entity));

	node = (node_t *)entity;
	
	aal_block_free(node->block);
	node->block = NULL;

#ifndef ENABLE_STAND_ALONE
	node->dirty = 0;
#endif
	return 0;
}

/* Closes node by means of closing its block */
errno_t node_common_close(object_entity_t *entity) {
	node_t *node;

	aal_assert("umka-825", entity != NULL);

	node = (node_t *)entity;

	if (node->block)
		node_common_unload(entity);
	
	aal_free(entity);
	return 0;
}

/* Confirms that passed node corresponds current plugin */
int node_common_confirm(object_entity_t *entity) {
	aal_assert("vpf-014", entity != NULL);
	aal_assert("umka-2020", loaded(entity));

	return (nh_get_magic((node_t *)entity) == NODE_MAGIC);
}

/* Returns item number in passed node entity. Used for any loops through the all
   node items. */
uint16_t node_common_items(object_entity_t *entity) {
	aal_assert("vpf-018", entity != NULL);
	aal_assert("umka-2021", loaded(entity));
	
	return nh_get_num_items((node_t *)entity);
}

#ifndef ENABLE_STAND_ALONE
/* Returns node free space */
uint16_t node_common_space(object_entity_t *entity) {
	aal_assert("vpf-020", entity != NULL);
	aal_assert("umka-2025", loaded(entity));
	
	return nh_get_free_space((node_t *)entity);
}

/* Returns node make stamp */
void node_common_set_mstamp(object_entity_t *entity,
			    uint32_t stamp)
{
	aal_assert("vpf-644", entity != NULL);
	aal_assert("umka-2038", loaded(entity));
	
	((node_t *)entity)->dirty = 1;
	nh_set_mkfs_id((node_t *)entity, stamp);
}

/* Returns node flush stamp */
void node_common_set_fstamp(object_entity_t *entity,
			    uint64_t stamp)
{
	aal_assert("vpf-643", entity != NULL);
	aal_assert("umka-2039", loaded(entity));
	
	((node_t *)entity)->dirty = 1;
	nh_set_flush_id((node_t *)entity, stamp);
}

void node_common_set_level(object_entity_t *entity,
			   uint8_t level)
{
	aal_assert("umka-1864", entity != NULL);
	aal_assert("umka-2040", loaded(entity));
	
	((node_t *)entity)->dirty = 1;
	nh_set_level((node_t *)entity, level);
}

/* Updating node stamp */
errno_t node_common_set_stamp(object_entity_t *entity,
			      uint32_t stamp)
{
	aal_assert("umka-1126", entity != NULL);
	aal_assert("umka-2042", loaded(entity));

	nh_set_mkfs_id(((node_t *)entity), stamp);
	return 0;
}

/* Saves node to device */
errno_t node_common_sync(object_entity_t *entity) {
	errno_t res;
	
	aal_assert("umka-1552", entity != NULL);
	aal_assert("umka-2043", loaded(entity));
	
	if ((res = aal_block_write(((node_t *)entity)->block)))
		return res;

	((node_t *)entity)->dirty = 0;
	return 0;
}
#endif
