/*
    node40_repair.c -- reiser4 default node plugin.
  
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "node40.h"
#include <repair/repair_plugin.h>

#define MIN_ITEM_LEN	1

extern errno_t node40_remove(object_entity_t *entity, pos_t *pos, 
    uint32_t count);
extern errno_t node40_item(object_entity_t *entity, pos_t *pos, 
    item_entity_t *item);
extern errno_t node40_expand(object_entity_t *entity, pos_t *pos, uint32_t len,
    uint32_t count);
extern errno_t node40_rep(node40_t *dst_node, pos_t *dst_pos, 
    node40_t *src_node, pos_t *src_pos, uint32_t count);
extern errno_t node40_shrink(object_entity_t *entity, pos_t *pos, uint32_t len,
    uint32_t count);

static void node40_set_offset_at(node40_t *node, int pos, uint16_t offset) {
    if (pos > nh40_get_num_items(node))
	return;
    
    if (nh40_get_num_items(node) == pos) 
	nh40_set_free_space_start(node, offset);
    else 
	ih40_set_offset(node40_ih_at(node, pos), offset);
}

static errno_t node40_region_delete(node40_t *node, uint16_t start_pos, 
    uint16_t end_pos) 
{
    uint8_t i;
    pos_t pos;
    item40_header_t *ih;
     
    aal_assert("vpf-201", node != NULL);
    aal_assert("vpf-202", node->block != NULL);
    aal_assert("vpf-213", start_pos <= end_pos);
    aal_assert("vpf-214", end_pos <= nh40_get_num_items(node));
    
    ih = node40_ih_at(node, start_pos);
    for (i = start_pos; i < end_pos; i++, ih--)
	ih40_set_offset(ih, ih40_get_offset(ih + 1) + 1);
    
    pos.unit = ~0ul;
    pos.item = start_pos - 1;
    
    if(node40_remove((object_entity_t *)node, &pos, end_pos - pos.item)) {
	aal_exception_bug("Node (%llu): Failed to delete the item (%d) of a "
	    "region [%u..%u].", aal_block_number(node->block), i - pos.item,
	    start_pos, end_pos);
	return -EINVAL;
    }
    
    return 0;    
}

static bool_t node40_item_count_valid(uint32_t blk_size, uint32_t count) {
    if ((blk_size - sizeof(node40_header_t)) / (sizeof(item40_header_t) +
	MIN_ITEM_LEN) < count)
	return 0;

    return 1;
}

static uint32_t node40_count_estimate(node40_t *node) {
    uint32_t num, blk_size;
    
    aal_assert("vpf-804", node != NULL);
    aal_assert("vpf-806", node->block != NULL);
    
    blk_size = aal_block_size(node->block);
    
    /* Free space start is less then node_header + MIN_ITEM_LEN. */
    if (sizeof(node40_header_t) + MIN_ITEM_LEN > 
	nh40_get_free_space_start(node))
	return 0;

    /* Free space start is greater then the first item  */
    if (blk_size - sizeof(item40_header_t) < nh40_get_free_space_start(node))
	return 0;

    /* Free space + node_h + 1 item_h + 1 MIN_ITEM_LEN should be less them 
     * blksize. */
    if (nh40_get_free_space(node) > blk_size - sizeof(node40_header_t) - 
	sizeof(item40_header_t) - MIN_ITEM_LEN)
	return 0;

    num = nh40_get_free_space_start(node) + nh40_get_free_space(node);
	
    /* Free space end > first item header offset. */
    if (num > blk_size - sizeof(item40_header_t))
	return 0;
    
    num = blk_size - num;
	
    /* The space between free space end and the end of block should be 
     * divisible by item_header. */
    if (num % sizeof(item40_header_t))
	return 0;
	
    num /= sizeof(item40_header_t);
    
    if (!node40_item_count_valid(blk_size, num))
	return 0;

    return num;
}

/* Count of items is correct. Free space fields and item locations should be 
 * checked/recovered if broken. */
static errno_t node40_item_array_check(node40_t *node, uint8_t mode) {
    uint32_t limit, offset, last_relable, count, i, last_pos;
    errno_t res = REPAIR_OK;
    bool_t free_valid;
    blk_t blk;
    
    aal_assert("vpf-208", node != NULL);
    aal_assert("vpf-209", node->block != NULL);

    offset = 0;
    blk = aal_block_number(node->block);
    
    /* Free space fields cossider as valid if count calculated on the base of 
     * it matches the count ofrm the node_header. */
    count = nh40_get_num_items(node);
    
    free_valid = (node40_count_estimate(node) == count);
    
    limit = free_valid ? nh40_get_free_space_start(node) : 
	aal_block_size(node->block) - count * sizeof(item40_header_t);
    
    last_pos = 0;
    last_relable = sizeof(node40_header_t);
    for(i = 0; i <= count; i++) {
	offset = (i == count) ? nh40_get_free_space_start(node) : 
	    ih40_get_offset(node40_ih_at(node, i));
	
	if (i == 0) {
	    if (offset != last_relable) {
		aal_exception_error("Node (%llu), item (0): Offset (%u) is "
		    "wrong. Should be (%u). %s", blk, offset, last_relable, 
		    mode == REPAIR_REBUILD ? "Fixed." : "");

		if (mode == REPAIR_REBUILD) {
		    ih40_set_offset(node40_ih_at(node, 0), last_relable);
		    res |= REPAIR_FIXED;
		} else {
		    res |= REPAIR_FATAL;
		}
	    }
	    continue;
	}
		
	if (offset < last_relable + (i - last_pos) * MIN_ITEM_LEN || 
	    offset + (count - i) * MIN_ITEM_LEN > limit) 
	{
	    aal_exception_error("Node (%llu), item (%u): Offset (%u) is wrong.",
		blk, i, offset);
	} else {
	    if ((mode == REPAIR_REBUILD) && (last_pos != i - 1)) {
		/* Some items are to be deleted. */
		aal_exception_error("Node (%llu): Region of items [%d-%d] with "
		    "wrong offsets is deleted.", blk, last_pos, i - 1);
		limit -= (offset - last_relable);
		count -= (i - last_pos);
		if (node40_region_delete(node, last_pos + 1, i))
		    return -EINVAL;
		
		i = last_pos;
	    } else {	    
		last_pos = i;
		last_relable = (i == count) ?
		    nh40_get_free_space_start(node) : 
		    ih40_get_offset(node40_ih_at(node, i));
	    }
	}
    }
    
    if (free_valid) {
	aal_assert("vpf-807", node40_count_estimate(node) == count);
    }
    
    /* Last relable position is not free space spart. Correct it. */
    if (last_pos != count) {	
	/* There is left region with broken offsets, remove it. */
	aal_exception_error("Node (%llu): Free space start (%u) is wrong. "
	    "Should be (%u). %s", blk, offset, last_relable, 
	    mode == REPAIR_REBUILD ? "Fixed." : "");
	
	if (mode == REPAIR_REBUILD) {
	    nh40_set_free_space(node, nh40_get_free_space(node) + offset - 
		last_relable);	
	    nh40_set_free_space_start(node, last_relable);
	    res |= REPAIR_FIXED;
	} else {
	    res |= REPAIR_FATAL;
	}
    }
    
    last_relable = aal_block_size(node->block) - last_relable - 
	sizeof(item40_header_t) * count;
    
    if (last_relable != nh40_get_free_space(node)) {
	/* Free space is wrong. */
	aal_exception_error("Node (%llu): the free space (%u) is wrong. Should "
	    "be (%u). %s", blk, nh40_get_free_space(node), last_relable, 
	    mode == REPAIR_CHECK ? "" : "Fixed.");
	
	if (mode == REPAIR_CHECK) {
	    res |= REPAIR_FIXABLE;
	} else {
	    nh40_set_free_space(node, last_relable);
	    res |= REPAIR_FIXED;
	}
    }
 
    return res;
}

static errno_t node40_item_array_find(node40_t *node, uint8_t mode) {
    uint32_t offset, i, nr = 0;
    errno_t res = REPAIR_OK;
    blk_t blk;
    
    aal_assert("vpf-800", node != NULL);

    blk = aal_block_number(node->block);
	
    for (i = 0; ; i++) {
	offset = ih40_get_offset(node40_ih_at(node, i));
	if (i) {
	    if (offset < sizeof(node40_header_t) + i * MIN_ITEM_LEN)
		break;	
	} else {
	    if (offset != sizeof(node40_header_t))
		return REPAIR_FATAL;
	}
	if (aal_block_size(node->block) - sizeof(item40_header_t) * (i + 1) -
	    MIN_ITEM_LEN < offset)
	    break;
	
	nr++;
    }

    /* Only nr - 1 item can be recovered as free space start is unknown. */
    if (nr <= 1)
	return REPAIR_FATAL;
    
    if (--nr != nh40_get_num_items(node)) {
	aal_exception_error("Node (%llu): Count of items (%u) is wrong. Found "
	    "only (%u) items. %s", blk, nh40_get_num_items(node), nr, 
	    mode == REPAIR_REBUILD ? "Fixed." : "");

	if (mode == REPAIR_REBUILD) {
	    nh40_set_num_items(node, nr);
	    res = REPAIR_FIXED;
	} else
	    return REPAIR_FATAL;
    }
    
    offset = ih40_get_offset(node40_ih_at(node, nr + 1));
    if (offset != nh40_get_free_space_start(node)) {
	aal_exception_error("Node (%llu): Free space start (%u) is wrong. (%u) "
	    "looks correct. %s", blk, nh40_get_free_space_start(node), offset, 
	    mode == REPAIR_CHECK ? "" : "Fixed.");
	
	if (mode != REPAIR_CHECK) {
	    nh40_set_free_space_start(node, offset);
	    res |= REPAIR_FIXED;
	} else
	    return REPAIR_FIXABLE;
    }
    
    offset = aal_block_size(node->block) - offset - 
	nr * sizeof(item40_header_t);

    if (offset != nh40_get_free_space_start(node)) {
	aal_exception_error("Node (%llu): Free space (%u) is wrong. Should be "
	    "(%u). %s", blk, nh40_get_free_space(node), offset, 
	    mode == REPAIR_CHECK ? "" : "Fixed.");
	
	if (mode != REPAIR_CHECK) {
	    nh40_set_free_space(node, offset);
	    res |= REPAIR_FIXED;
	} else
	    return REPAIR_FIXABLE;
    }
    
    return res;
}

/* Checks the count of items written in node_header. If it is wrong, it tries
 * to estimate it on the base of free_space fields and recover if REBUILD mode.
 * Returns FATAL otherwise. */
static errno_t node40_count_check(node40_t *node, uint8_t mode) {
    uint32_t num;
    blk_t blk;
    
    aal_assert("vpf-802", node != NULL);
    aal_assert("vpf-803", node->block != NULL);

    blk = aal_block_number(node->block);

    if (node40_item_count_valid(aal_block_size(node->block), 
	nh40_get_num_items(node)))
	return REPAIR_OK;

    /* Count is wrong. Try to recover it if possible. */
    num = node40_count_estimate(node);
    
    /* Recover is impossible. */
    if (num == 0) {
	aal_exception_error("Node (%llu): Count of items (%u) is wrong.", 
	    blk, nh40_get_num_items(node));
	return REPAIR_FATAL;
    }
    
    /* Recover is possible. */
    aal_exception_error("Node (%llu): Count of items (%u) is wrong. (%u) looks "
	"correct. %s", blk, nh40_get_num_items(node), num, 
	mode == REPAIR_REBUILD ? "Fixed." : "");

    if (mode == REPAIR_REBUILD) {
	nh40_set_num_items(node,  num);
	return REPAIR_FIXED;
    }
    
    return REPAIR_FATAL;
}

errno_t node40_check(object_entity_t *entity, uint8_t mode) {
    node40_t *node = (node40_t *)entity;
    errno_t res;
    
    aal_assert("vpf-194", node != NULL);
 
    /* Check the content of the node40 header. */
    res = node40_count_check(node, mode);
    
    /* Count is wrong and not recoverable on the base of free space end. */
    if (repair_error_exists(res)) {
	if (mode != REPAIR_REBUILD)
	    return res;

	/* Recover count on the base of correct item array if one exists. */
	return node40_item_array_find(node, mode);
    }
    
    /* Count looks ok. Recover item array. */
    return node40_item_array_check(node, mode);    
}

static errno_t node40_corrupt(object_entity_t *entity, uint16_t options) {
    node40_t *node = (node40_t *)entity;
    int i;
    item40_header_t *ih;    
    
    for(i = 0; i < nh40_get_num_items(node) + 1; i++) {
	if (aal_test_bit(&options, i)) {
	    node40_set_offset_at(node, i, 0xffff);
	}
    }

    return 0;
}

errno_t node40_copy(object_entity_t *dst, pos_t *dst_pos, 
    object_entity_t *src, pos_t *src_pos, copy_hint_t *hint) 
{
    node40_t *dst_node;
    node40_t *src_node;
    errno_t res;
    	
    item40_header_t *ih;
    item_entity_t src_item;
    item_entity_t dst_item;
    reiser4_plugin_t *plugin;

    aal_assert("vpf-965", dst != NULL);
    aal_assert("vpf-966", src != NULL);
    aal_assert("vpf-912", hint != NULL);
    
    aal_assert("umka-2029", node40_loaded(dst));
    aal_assert("umka-2030", node40_loaded(src));
	
    dst_node = (node40_t *)dst;
    src_node = (node40_t *)src;
    
    if (hint->src_count == 0)
	return 0;
    
    if (node40_item(src, src_pos, &src_item))
	return -EINVAL;
    
    if (hint->len_delta > 0) {
	/* Expand the node first. */
	if (node40_expand(dst, dst_pos, hint->len_delta, 1))
	    return -EINVAL;
    }
    
    if (node40_item(dst, dst_pos, &dst_item))
	return -EINVAL;
	
    /* If the whole @src item is to be inserted */
    if (dst_pos->unit == ~0ul)
	return node40_rep(dst_node, dst_pos, src_node, src_pos, 1);
    
    if ((res = plugin_call(src_item.plugin->o.item_ops, copy, &dst_item, 
	dst_pos->unit, &src_item, src_pos->unit, hint)))
    {
	aal_exception_error("Can't copy units from node %llu to node %llu.",
	    aal_block_number(src_node->block), 
	    aal_block_number(dst_node->block));
	return res;
    }
   
    if (hint->len_delta < 0) {
	/* Shrink the node if needed. */
	if (node40_shrink(dst, dst_pos, -hint->len_delta, 1))
	    return -EINVAL;
    }
    	
    /* Updating item's key if we insert new item or if we insert unit into 
     * leftmost postion. */
    if (dst_pos->unit == 0) {
	ih = node40_ih_at(dst_node, dst_pos->item);
	
	aal_memcpy(&ih->key, dst_item.key.body, sizeof(ih->key));
    }

    dst_node->dirty = 1;
    return 0;
}

#endif
