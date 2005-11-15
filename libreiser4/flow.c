/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   flow.c -- functions for working with flow. */

#include <reiser4/libreiser4.h>

/* Reads reads some number of bytes from @tree to @hint. This function is used
   in tail conversion and for reading data from the files. */
int64_t reiser4_flow_read(reiser4_tree_t *tree, trans_hint_t *hint) {
	char *buff;
	errno_t res;
	int64_t total;
	int64_t size;
	reiser4_key_t key;
	lookup_hint_t lhint;
	reiser4_place_t place;

	aal_assert("umka-2509", tree != NULL);
	aal_assert("umka-2510", hint != NULL);
	
	buff = hint->specific;
	aal_memcpy(&key, &hint->offset, sizeof(key));

#ifndef ENABLE_MINIMAL
	hint->blocks = tree->blocks;
	lhint.collision = NULL;
#endif
	lhint.level = LEAF_LEVEL;
	lhint.key = &hint->offset;
	
	/* Looking for the place to read. */
	if ((res = reiser4_tree_lookup(tree, &lhint, FIND_EXACT, &place)) < 0)
		return res;

	if (res == ABSENT) {
		/* Here we suppose, that @place points to next item,
		   just behind the hole. */
		if (reiser4_place_right(&place))
			reiser4_place_inc(&place, 1);

		if (reiser4_place_rightmost(&place)) {
			if ((res = reiser4_tree_next_place(tree, &place, &place)))
				return res;
		}
		
		res = ABSENT;
	}
	
	for (total = 0, size = hint->count; size > 0; ) {
		uint64_t next, look;
		int64_t hole;
		int32_t read;

		if (!place.plug)
			break;
		
		/* Data does not found. This may mean, that we have hole in tree
		   between keys. */
		if (res == ABSENT) {
			if (reiser4_key_compshort(&place.key, &hint->offset))
				/* No data found. */
				break;
			
			next = reiser4_key_get_offset(&place.key);
			look = reiser4_key_get_offset(&hint->offset);
			
			hole = next - look;
			read = (hole > size ? size : hole);

			/* Making holes in buffer. */
			aal_memset(hint->specific, 0, read);

			/* If we need to read more, the hole is finished, set 
			   @res to PRESENT for the next loop. */
			if (size > read)
				res = PRESENT;
			
			reiser4_key_inc_offset(&hint->offset, read);
		} else {
			/* Prepare hint for read */
			hint->count = size;

			reiser4_node_lock(place.node);
			read = objcall(&place, object->read_units, hint);
			reiser4_node_unlock(place.node);
			
			/* Read data from the tree */
			if (read < 0) 
				return read;
			else if (read == 0)
				break;

			if (size > read) {
				if ((res = reiser4_tree_next_place(tree, &place,
								   &place)) < 0)
				{
					return res;
				}
				
				reiser4_key_inc_offset(&hint->offset, read);
				
				if (!place.plug)
					res = ABSENT;
				else if (reiser4_key_compfull(&place.key,
							      &hint->offset))
					res = ABSENT;
				else 
					res = PRESENT;
			}
		}
		
		size -= read;
		total += read;

		/* Updating key and data buffer pointer */
		hint->specific += read;
	}

	hint->specific = buff;
	aal_memcpy(&hint->offset, &key, sizeof(key));
	
	return total;
}

#ifndef ENABLE_MINIMAL
/* Releases passed region in block allocator. This is used in tail during tree
   trunacte. */
static errno_t cb_release_region(uint64_t start, uint64_t width, void *data) {
	reiser4_tree_t *tree = (reiser4_tree_t *)data;
	return reiser4_alloc_release(tree->fs->alloc, start, width);
}

/* Writes flow described by @hint to tree. Takes care about keys in index part
   of tree, root updatings, etc. Returns number of bytes actually written. */
int64_t reiser4_flow_write(reiser4_tree_t *tree, trans_hint_t *hint) {
	char *buff;
	errno_t res;
	uint64_t off;
	uint64_t end;
	uint64_t size;
	uint64_t bytes;
	uint64_t total;
	reiser4_key_t key;

	lookup_hint_t lhint;
	reiser4_place_t place;

	aal_assert("umka-2506", tree != NULL);
	aal_assert("umka-2507", hint != NULL);
	
	buff = hint->specific;
	aal_memcpy(&key, &hint->offset, sizeof(key));
	end = off = reiser4_key_get_offset(&hint->offset);

	hint->blocks = tree->blocks;

	lhint.level = LEAF_LEVEL;
	lhint.key = &hint->offset;
	lhint.collision = NULL;

	/* Looking up the place to write into. */
	if ((res = reiser4_tree_lookup(tree, &lhint, FIND_CONV, &place)) < 0)
		return res;
	
	/* Loop until desired number of bytes is written. */
	for (total = bytes = 0, size = hint->count; size > 0;) {
		int32_t write;
		uint32_t level;
		
		if (end == off) {
			reiser4_key_t nkey;
			
			if ((res = reiser4_tree_next_key(tree, &place, &nkey)))
				return res;

			if (reiser4_key_compshort(&nkey, &hint->offset)) {
				/* No data found. */
				end = MAX_UINT64;
			} else {
				end = reiser4_key_get_offset(&nkey);
			}
		}
		
		hint->count = (size > end - off) ? end - off : size;
		hint->blocks = tree->blocks;
		
		/* level new item will be inserted a on. */
		level = reiser4_tree_target_level
			(tree, (reiser4_plug_t *)hint->plug);
		hint->bytes = 0;

		/* Writing data to tree. */
		if ((write = reiser4_tree_write(tree, &place,
						hint, level)) < 0)
		{
			return write;
		} else if (write == 0) {
			break;
		}

		/* Updating counters */
		size -= write;
		total += write;
		bytes += hint->bytes;
		
		/* Updating key and buffer pointer */
		if (hint->specific)
			hint->specific += write;
		
		off += write;
		reiser4_key_inc_offset(&hint->offset, write);
		
		if (end - off > 0) {
			/* Position in the place may be left not updated. 
			   Lookup the item again. */
			if ((res = objcall(&place, balance->lookup, 
					   &lhint, FIND_CONV)) < 0)
			{
				return res;
			}
		} else {
			res = reiser4_tree_next_place(tree, &place, &place);
			if (res) return res;
			
			aal_assert("vpf-1890", place.plug != NULL);
		}
	}

	hint->bytes = bytes;
	hint->specific = buff;
	aal_memcpy(&hint->offset, &key, sizeof(key));
	
	return total;
}

/* Truncates item pointed by @hint->offset key by value stored in
   @hint->count. This is used during tail conversion and in object plugins
   truncate() code path. */
int64_t reiser4_flow_truncate(reiser4_tree_t *tree, trans_hint_t *hint) {
	errno_t res;
	int64_t trunc;
	uint32_t size;
	uint64_t bytes;
	uint64_t total;
	reiser4_key_t key;

	aal_assert("umka-2475", tree != NULL);
	aal_assert("umka-2476", hint != NULL);

	aal_memcpy(&key, &hint->offset, sizeof(key));

	/* Setting up region func to release region callback. It is needed for
	   releasing extent blocks. */
	hint->region_func = cb_release_region;
	hint->blocks = tree->blocks;

	for (total = bytes = 0, size = hint->count; size > 0;
	     size -= trunc, total += trunc)
	{
		lookup_hint_t lhint;
		reiser4_place_t place;

		lhint.level = LEAF_LEVEL;
		lhint.key = &hint->offset;
		lhint.collision = NULL;
	
		/* FIXME: look into flow_write. */
		if ((res = reiser4_tree_lookup(tree, &lhint, FIND_EXACT,
					       &place)) < 0)
		{
			return res;
		}

		/* Nothing found by @hint->offset. This means, that tree has a
		   hole between keys. We will handle this, as it is needed for
		   fsck. */
		if (res == ABSENT) {
			reiser4_key_t tkey;
			
			/* Here we suppose, that @place points to next item,
			   just behind the hole. */
			if (reiser4_place_right(&place))
				reiser4_place_inc(&place, 1);

			if ((res = reiser4_tree_place_key(tree, &place, &tkey)))
				return res;
			
			if (objcall(&tkey, compshort, &hint->offset)) {
				/* No data found. */
				trunc = size;
			} else {			
				uint64_t hole, next, look;

				next = reiser4_key_get_offset(&tkey);
				look = reiser4_key_get_offset(&hint->offset);

				hole = next - look;
				trunc = (hole > size ? size : hole);
			}
			
			reiser4_key_inc_offset(&hint->offset, trunc);
			continue;
		}

		hint->count = size;
		hint->bytes = 0;

		/* Calling node truncate method. */
		if ((trunc = reiser4_node_trunc(place.node, &place.pos,
						hint)) < 0)
		{
			return trunc;
		}
		
		bytes += hint->bytes;
		
		/* Updating left delimiting keys in all parent nodes */
		if (reiser4_place_leftmost(&place) &&
		    place.node->p.node)
		{
			/* If node became empty it will be detached from the
			   tree, so updating is not needed and impossible,
			   because it has no items. */
			if (reiser4_node_items(place.node) > 0) {
				reiser4_key_t lkey;
				reiser4_place_t parent;

				/* Updating parent keys. */
				reiser4_node_leftmost_key(place.node, &lkey);
				aal_memcpy(&parent, &place.node->p, sizeof(parent));

				if ((res = reiser4_tree_update_keys(tree, &parent,
								    &lkey)))
				{
					return res;
				}
			}
		}
	
		/* Checking if the node got empty. If so, we release it. */
		if (reiser4_node_items(place.node) > 0) {
			if ((res = reiser4_tree_shrink(tree, &place)))
				return res;
		} else {
			/* Release @place.node, as it got empty.  */
			if (reiser4_node_locked(place.node)) {
				place.node->flags |= NF_HEARD_BANSHEE;
			} else {
				reiser4_tree_discard_node(tree, place.node);
				place.node = NULL;
			}
		}

		/* Drying tree up in the case root node has only one item */
		if (tree->root && reiser4_tree_singular(tree) &&
		    !reiser4_tree_minimal(tree))
		{
			if ((res = reiser4_tree_dryout(tree)))
				return res;
		}

		reiser4_key_inc_offset(&hint->offset, trunc);
	}
	
	hint->bytes = bytes;

	aal_memcpy(&hint->offset, &key, sizeof(key));
	return total;
}

/* Converts file body at @hint->offset from tail to extent or from extent to
   tail. Main tail convertion function. It uses tree_read_flow(),
   tree_truc_flow() and tree_write_flow(). */
errno_t reiser4_flow_convert(reiser4_tree_t *tree, conv_hint_t *hint) {
	char *buff;
	errno_t res;
	int64_t conv;
	uint64_t size;
	int64_t insert;
	uint32_t blksize;
	trans_hint_t trans;
	
	aal_assert("umka-2406", tree != NULL);
	aal_assert("umka-2407", hint != NULL);
	aal_assert("umka-2481", hint->plug != NULL);

	blksize = reiser4_tree_get_blksize(tree);
	aal_memcpy(&trans.offset, &hint->offset, sizeof(trans.offset));

	insert = hint->count;
	
	/* Check if the start byte is not multiple by the block size. 
	   Adjust if needed. */
	conv = reiser4_key_get_offset(&trans.offset);
	size = conv & (blksize - 1);
	
	if (size) {
		reiser4_key_set_offset(&trans.offset, conv - size);
		if (hint->count != MAX_UINT64)
			insert += size;
	}

	/* Check if number of bytes to be converted is not multiple of block
	   size. If so, have to round @hint->count up to blksize. */
	size = insert & (blksize - 1);
	
	if (hint->count != MAX_UINT64 && size)
		insert += (blksize - size);
	
	/* Loop until @size bytes is converted. */
	for (hint->bytes = 0; insert > 0; insert -= conv) {
		/* Each convertion tick may be divided onto tree stages:

		   (1) Read blksize bytes @trans hint.

		   (2) Truncate data in tree we have just read described by
		   @trans hint.

		   (3) Write data back to tree with target item plugin used for
		   writing (tail plugin if we convert extents to tails and
		   extent plugin is used otherwise).
		*/
		
		/* Preparing buffer to read data to it. */
		trans.count = blksize > insert ? insert : blksize;

		if (!(buff = aal_calloc(trans.count, 0)))
			return -ENOMEM;

		trans.specific = buff;

		/* First stage -- reading data from tree. */
		if ((conv = reiser4_flow_read(tree, &trans)) < 0) {
			res = conv;
			goto error_free_buff;
		}
		
		/* Second statge -- removing data from the tree. */
		trans.data = tree;
		
		if (conv == 0) {
			/* If nothing was read, depending on hint->ins_hole flag
			   the hole will be inserted or convertion is finished. */
			if (!hint->ins_hole) {
				res = 0;
				goto error_free_buff;
			}

			trans.specific = NULL;
		} else {
			/* Trunc & insert only read @count bytes. */
			trans.count = conv;
		}

		if ((conv = reiser4_flow_truncate(tree, &trans)) < 0) {
			res = conv;
			goto error_free_buff;
		}

		if (insert > 0) {
			/* Insert only allowed amount of bytes. */
			trans.count = conv > insert ? insert : conv;
			trans.plug = hint->plug;
			trans.shift_flags = SF_DEFAULT;
			trans.place_func = hint->place_func;

			/* Third stage -- writing data back to tree with 
			   new item plugin used.*/
			if ((res = reiser4_flow_write(tree, &trans)) < 0)
				goto error_free_buff;
		}

		hint->bytes += trans.bytes;
		reiser4_key_inc_offset(&trans.offset, conv);

		aal_free(buff);
	}

	return 0;
	
 error_free_buff:
	aal_free(buff);
	return res;
}
#endif
