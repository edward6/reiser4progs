/*
    node40_repair.c -- reiser4 default node plugin.
  
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE

#include "node40.h"
#include "repair/repair_plugins.h"

#define INVALID_U16	0xffff

extern errno_t node40_remove(object_entity_t *entity, rpos_t *pos, uint32_t count);
extern uint16_t node40_items(object_entity_t *entity);

static uint16_t node40_get_offset_at(object_entity_t *entity, int pos) {
    node40_t *node = (node40_t *)entity;
    
    if (pos > nh40_get_num_items(node))
	return 0;
    
    return nh40_get_num_items(node) == pos ? nh40_get_free_space_start(node) :
	ih40_get_offset(node40_ih_at(node, pos));
}

static void node40_set_offset_at(object_entity_t *entity, int pos, uint16_t offset) {
    node40_t *node = (node40_t *)entity;
    
    if (pos > nh40_get_num_items(node))
	return;
    
    if (nh40_get_num_items(node) == pos) 
	nh40_set_free_space_start(node, offset);
    else 
	ih40_set_offset(node40_ih_at(node, pos), offset);
}

static errno_t node40_region_delete(object_entity_t *entity,
    uint16_t start_pos, uint16_t end_pos) 
{
    int i;
    rpos_t pos;
    item40_header_t *ih;
    node40_t *node = (node40_t *)entity;
     
    aal_assert("vpf-201", node != NULL);
    aal_assert("vpf-202", node->block != NULL);
    aal_assert("vpf-213", start_pos <= end_pos);
    aal_assert("vpf-214", end_pos <= node40_items(entity));
    
    ih = node40_ih_at(node, start_pos);
    for (i = start_pos; i <= end_pos; i++, ih--) {
	if (i != end_pos || i == node40_items(entity)) 
	    node40_set_offset_at(entity, i, ih40_get_offset(ih + 1) + 1);	
    }

    /*
      FIXME-UMKA->VITALY: Here can be used node40_remove with right count
      parameter, or node40_cut.
    */
    pos.unit = ~0ul;
    pos.item = start_pos - 1;
    for (i = start_pos - 1; i < end_pos; i++) {
	if (node40_remove(entity, &pos, 1)) {
	    aal_exception_bug("Node (%llu): Failed to delete the item (%d) "
		"of a region (%d-%d).", aal_block_number(node->block), 
		i - start_pos + 1, start_pos, end_pos);
	    return -1;
	}
    }
    
    return 0;    
}

/* 
    Checks the set of items within the node. Delete item with wrong offsets, fix free space,
    free space start.
*/
static errno_t node40_item_array_check(object_entity_t *entity, uint8_t mode) {
    node40_t *node = (node40_t *)entity;
    int64_t start_pos, end_pos;
    uint64_t offset, r_limit;
    int i, l_pos;
    blk_t blk;
    errno_t res = REPAIR_OK;
    
    aal_assert("vpf-208", node != NULL);
    aal_assert("vpf-209", node->block != NULL);

    blk = aal_block_number(node->block);
    
    /* First item offset must be at node40_header bytes. */
    if (node40_get_offset_at(entity, 0) != sizeof(node40_header_t)) {
	aal_exception_error("Node (%llu): item (0) has a wrong offset (%u), "
	    "Should be (%u). %s", blk, node40_get_offset_at(entity, 0), 
	    sizeof(node40_header_t), mode == REPAIR_CHECK ? "" : "Fixed.");

	/* Node will not be synced on disk, fix it for further checks only. */
	node40_set_offset_at(entity, 0, sizeof(node40_header_t));	
	
	res |= mode != REPAIR_CHECK ? REPAIR_FIXED : REPAIR_FIXABLE;
    }

    /* Rigth limit for offsets is at item40_headers count from the end of block. */
    r_limit = node40_free_space_end(node);

    /* Start of free space must be between node40_header and r_limit */
    if (nh40_get_free_space_start(node) < sizeof(node40_header_t) || 
	nh40_get_free_space_start(node) > r_limit)
    {
	aal_exception_error("Node (%llu): the start of the free space (%u) "
	    "is invalid.", blk, nh40_get_free_space_start(node));
	nh40_set_free_space_start(node, INVALID_U16);

	res |= mode != REPAIR_CHECK ? REPAIR_FIXED : REPAIR_FIXABLE;
    }

    /* Free space cannot be more then r_limit - node40_header */
    if (nh40_get_free_space(node) + sizeof(node40_header_t) > r_limit) {
	aal_exception_error("Node (%llu): the free space (%u) is invalid.", 
	    blk, nh40_get_free_space(node));
	nh40_set_free_space(node, INVALID_U16);

	res |= mode != REPAIR_CHECK ? REPAIR_FIXED : REPAIR_FIXABLE;
    }
    
    /* If free_space_start + free_space == r_limit => free_space_start is 
     * relable. */
    if ((uint64_t)nh40_get_free_space(node) + nh40_get_free_space_start(node) == r_limit) {
	r_limit = nh40_get_free_space_start(node);
    } else if (nh40_get_free_space(node) != INVALID_U16 && 
	     nh40_get_free_space_start(node) != INVALID_U16)
    {
	/* Cannot rely on neither free_space nor free_space_start. */
	aal_exception_error("Node (%llu): the start of the free space (%u) "
	    " + free space (%u) is not equal to rigth offset limit (%u).", 
	    blk, nh40_get_free_space_start(node), nh40_get_free_space(node), 
	    r_limit);
	nh40_set_free_space(node, INVALID_U16);
	nh40_set_free_space_start(node, INVALID_U16);
    }
 
    l_pos = 0;
    for(i = 1; i <= node40_items(entity); i++) {
	offset = node40_get_offset_at(entity, i);

	if (offset == INVALID_U16)
	    continue;
    	    
	/* Check if the item offset is invalid. */
	if (offset < sizeof(node40_header_t) || offset > r_limit) {
	    aal_exception_error("Node (%llu): the offset (%u) of the item"
		" (%d) is invalid.", blk, offset, i);

	    node40_set_offset_at(entity, i, INVALID_U16);
	} else {
	    /* Offset is not INVALID_U16 */
	    if (node40_get_offset_at(entity, l_pos) > offset)
		/* Both offsets are in the valid interval and not in increasing 
		 * order. Not recoverable. */
		return -1;
	    if (l_pos != i - 1) {
		/* Some items are to be deleted. */
		aal_exception_error("Node (%llu): items [%d-%d] are deleted due "
		    "to unrecoverable offsets.", blk, l_pos, i - 1);
		if (node40_region_delete(entity, l_pos + 1, i))
		    return -1;
		
		i = l_pos;
	    }
		
	    l_pos = i;
	}
    }
    
    if (l_pos != node40_items(entity)) {	
	/* There is a region at the end with broken offsets, 
	 * free_space_start is also broken. */
	offset = node40_get_offset_at(entity, l_pos);
	if (node40_region_delete(entity, l_pos + 1, node40_items(entity)))
	    return -1;
	nh40_set_free_space_start(node, offset);
	nh40_set_free_space(node, node40_free_space_end(node) - offset);
    }

    return 0;
}

static errno_t node40_item_count_check(object_entity_t *entity, uint8_t mode) {
    node40_t *node = (node40_t *)entity;

    aal_assert("vpf-199", node != NULL);
    aal_assert("vpf-200", node->block != NULL);
    aal_assert("vpf-247", node->block->device != NULL);

    if (node40_items(entity) > 
	(aal_device_get_bs(node->block->device) - sizeof(node40_header_t)) / 
	(sizeof(item40_header_t) + 1)) 
    {
	aal_exception_error("Node (%llu): number of items (%u) exceeds the "
	    "limit.", aal_block_number(node->block), node40_items(entity));
	return REPAIR_FATAL;
    }
    
    return REPAIR_OK;
}

static errno_t node40_corrupt(object_entity_t *entity, uint16_t options) {
    int i;
    item40_header_t *ih;
    node40_t *node = (node40_t *)entity;
    
    for(i = 0; i < node40_items(entity) + 1; i++) {
	if (aal_test_bit(&options, i)) {
	    node40_set_offset_at(entity, i, INVALID_U16);
	}
    }

    return 0;
}

errno_t node40_check(object_entity_t *entity, uint8_t mode) {
    node40_t *node = (node40_t *)entity;
    errno_t res;
    
    aal_assert("vpf-194", node != NULL);
 
    /* Check the count of items */
    res = node40_item_count_check(entity, mode);

    if (repair_error_fatal(res))
	return res;

    /* Check the item array and free space. */
    res |= node40_item_array_check(entity, mode);

    return res;
}

#endif
