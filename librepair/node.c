/* Copyright 2001-2005 by Hans Reiser, licensing governed by 
   reiser4progs/COPYING.
   
   librepair/node.c - methods are needed for node recovery. */

#include <repair/librepair.h>

/* Opens the node if it has correct mkid stamp. */
reiser4_node_t *repair_node_open(reiser4_tree_t *tree, blk_t blk, uint32_t mkid)
{
	reiser4_node_t *node;
	
	aal_assert("vpf-708", tree != NULL);
	
	if (!(node = reiser4_node_open(tree, blk)))
		return NULL;
	
	/* Extra checks are needed. */
	if (mkid && mkid != reiser4_node_get_mstamp(node))
		goto error_node_free;
	
	return node;
	
 error_node_free:
	reiser4_node_close(node);
	return NULL;
}

/* Checks all the items of the node. */
static errno_t repair_node_items_check(reiser4_node_t *node, place_func_t func,
				       uint8_t mode, void *data) 
{
	reiser4_key_t key, prev;
	trans_hint_t hint;
	errno_t res, ret;
	uint32_t count;
	reiser4_place_t place;
	pos_t *pos;
	
	aal_assert("vpf-229", node != NULL);
	aal_assert("vpf-231", node->plug != NULL);
	
	res = 0;
	pos = &place.pos;
	place.node = node;
	count = reiser4_node_items(node);
	aal_memset(&prev, 0, sizeof(prev));

	for (pos->item = 0; pos->item < count; pos->item++) {
		pos->unit = MAX_UINT32;
		
		/* Open the item, checking its plugin id. */
		if (reiser4_place_fetch(&place))
			return -EINVAL;
		
		aal_memcpy(&key, &place.key, sizeof(key));

		if ((ret =  objcall(&key, check_struct) < 0))
			return ret;
		
		if (ret) {
			/* Key has some corruptions and cannot be recovered. */
			fsck_mess("Node (%llu): The key [%s] of the item "
				  "(%u) is broken.%s",
				  (unsigned long long)node->block->nr,
				  reiser4_print_key(&place.key),
				  pos->item, mode == RM_BUILD ? " Removed." : 
				  "");
			
			goto error_remove_item;
		} else if (reiser4_key_compfull(&key, &place.key)) {
			/* @Key has been fixed. */
			fsck_mess("Node (%llu): The key [%s] of the item (%u) "
				  "is broken. %s [%s].",
				  (unsigned long long)node->block->nr,
				  reiser4_print_key(&place.key),
				  pos->item, (ret && mode == RM_CHECK) ? 
				  "Should be" : "Fixed to", 
				  reiser4_print_key(&key));
			
			if (mode == RM_CHECK)
				res |= RE_FIXABLE;
			else {
				reiser4_node_update_key(node, pos, &key);
				reiser4_node_mkdirty(node);
			}
		}
		
		/* Check the item structure. */
		if ((ret = repair_item_check_struct(&place, mode)) < 0)
			return ret;

		/* Remove the item if fatal error. */
		if (ret & RE_FATAL) {
			fsck_mess("Node (%llu), item (%u), [%s]: broken item "
				  "found.%s",
				  (unsigned long long)node->block->nr,
				  pos->item, reiser4_print_key(&key),
				  mode == RM_BUILD ? " Remove it." : "");

			goto error_remove_item;
		}
		
		res |= ret;
		
		if (prev.plug) {
			if (reiser4_key_compfull(&prev, &key) > 0) {
				fsck_mess("Node (%llu), items (%u) and "
					  "(%u): Wrong order of keys.",
					  (unsigned long long)node->block->nr,
					  pos->item - 1, pos->item);

				return RE_FATAL;
			}
		}
		
		prev = key;

		if ((ret = reiser4_item_maxreal_key(&place, &key)))
			return ret;
		
		/* Check that the maxreal key is not less than the place key */
		if (reiser4_key_compfull(&prev, &key) > 0) {
			fsck_mess("Node (%llu), item (%u): has the wrong key "
				  "or too large body: from [%s] to [%s].",
				  (unsigned long long)node->block->nr,
				  pos->item,
				  reiser4_print_key(&prev),
				  reiser4_print_key(&key));
			
			goto error_remove_item;
		}
		
		if (func && (ret = func(&place, data)))
			return ret;
		
		prev = key;
		
		continue;
		
	error_remove_item:
		if (mode != RM_BUILD) {
			res |= RE_FATAL;
			continue;
		}
		
		aal_memset(&hint, 0, sizeof(hint));

		hint.count = 1;
		hint.place_func = NULL;
		hint.region_func = NULL;
		hint.shift_flags = SF_DEFAULT;

		if ((ret = reiser4_node_remove(node, pos, &hint)))
			return ret;

		pos->item--;
		count = reiser4_node_items(node);
	}
	
	return res;
}

errno_t repair_node_check_level(reiser4_node_t *node, uint8_t mode) {
	reiser4_place_t place;
	uint32_t count;
	uint8_t level;
	errno_t res;
	pos_t *pos;
	
	aal_assert("vpf-494", node != NULL);
	aal_assert("vpf-220", node->plug != NULL);
	
	level = reiser4_node_get_level(node);
	count = reiser4_node_items(node);
	place.node = node;
	pos = &place.pos;
	res = 0;
	
	level = reiser4_node_get_level(node);
	
	/* Level of the node must be > 0 */
	if (!level) {
		fsck_mess("Node (%llu): illegal level found (%u).", 
			  (unsigned long long)node->block->nr, level);
		return RE_FATAL;
	}

	pos->unit = MAX_UINT32;
	for (pos->item = 0; pos->item < count; pos->item++) {
		/* Open the item, checking its plugin id. */
		if ((res = reiser4_place_fetch(&place)))
			return res;
		
		/* Check that the item is legal for this node. If not, it 
		   is not recoverable corruption for now. FIXME-VITALY: 
		   how to recover valuable data from here? */
		if (!repair_tree_legal_level(place.plug, 
					     reiser4_node_get_level(node)))
		{
			fsck_mess("Node (%llu): Node level (%u) does not match "
				  "to the item type (%s).",
				  (unsigned long long)node->block->nr,
				  reiser4_node_get_level(node),
				  place.plug->p.label);
			
			return RE_FATAL;
		}
	}

	return 0;
}

/*  Checks the node content. */
errno_t repair_node_check_struct(reiser4_node_t *node, place_func_t func,
				 uint8_t mode, void *data) 
{
	errno_t res;
	
	aal_assert("vpf-494", node != NULL);
	aal_assert("vpf-220", node->plug != NULL);
	
	res = objcall(node, check_struct, mode);
	
	if (repair_error_fatal(res))
		return res;
	
	res |= repair_node_items_check(node, func, mode, data);
	
	return res;
}

errno_t repair_node_clear_flags(reiser4_node_t *node) {
	pos_t *pos;
	uint32_t count;
	reiser4_place_t place;
	
	aal_assert("vpf-1401", node != NULL);
	aal_assert("vpf-1403", node->plug != NULL);

	place.node = node;
	count = reiser4_node_items(node);
	
	pos = &place.pos;
	pos->unit = MAX_UINT32;
	
	for (pos->item = 0; pos->item < count; pos->item++) {
		if (reiser4_place_fetch(&place))
			return -EINVAL;
		
		reiser4_item_clear_flags(&place);
	}

	return 0;
}

/* Packes @node to @stream. */
errno_t repair_node_pack(reiser4_node_t *node,
			 aal_stream_t *stream)
{
	aal_assert("umka-2622", node != NULL);
	aal_assert("umka-2623", stream != NULL);

	return objcall(node, pack, stream);
}

/* Create node from passed @stream. */
reiser4_node_t *repair_node_unpack(reiser4_tree_t *tree, 
				   aal_stream_t *stream) 
{
	blk_t blk;
	rid_t pid;
	
	reiser4_node_t *node;
	uint32_t size;
	aal_block_t *block;
	reiser4_plug_t *plug;
	aal_device_t *device;
	
	aal_assert("umka-2625", stream != NULL);
	aal_assert("umka-2624", tree != NULL);
	aal_assert("vpf-1656", tree->fs != NULL);
	aal_assert("vpf-1657", tree->fs->device != NULL);

	if (aal_stream_read(stream, &pid, sizeof(pid)) != sizeof(pid))
		goto error_eostream;

	if (aal_stream_read(stream, &blk, sizeof(blk)) != sizeof(blk))
		goto error_eostream;
	
	/* Finding the node plugin by its id */
	if (!(plug = reiser4_factory_ifind(NODE_PLUG_TYPE, pid))) {
		aal_error("Can't find node plugin by its id 0x%x.", pid);
		return NULL;
	}

	size = reiser4_tree_get_blksize(tree);
	device = tree->fs->device;

	/* Allocate new node of @size at @nr. */
	if (!(block = aal_block_alloc(device, size, blk)))
		return NULL;

	aal_block_fill(block, 0);
	
	/* Requesting the plugin for initialization node entity. */
	if (!(node = plugcall((reiser4_node_plug_t *)plug, unpack, 
			      block, tree->key.plug, stream)))
	{
		goto error_free_block;
	}

	return node;
	
 error_free_block:
	aal_block_free(block);
	return NULL;
 error_eostream:
	aal_error("Can't unpack the node. Stream is over?");
	return NULL;
}

/* Print passed @node to the specified @stream */
void repair_node_print(reiser4_node_t *node, aal_stream_t *stream) {
	aal_assert("umka-1537", node != NULL);
	aal_assert("umka-1538", stream != NULL);
	
	plugcall(node->plug, print, node, stream, -1, -1, 0);
}
