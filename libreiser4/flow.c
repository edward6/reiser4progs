/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   flow.c -- functions for working with flow. */

#include <reiser4/libreiser4.h>

/* Reads reads some number of bytes from @tree to @hint. This function is used
   in tail conversion and for reading data from the files. */
int64_t reiser4_flow_read(reiser4_tree_t *tree, trans_hint_t *hint) {
	char *buff;
	errno_t res;
	int64_t total;
	uint64_t size;
	reiser4_key_t key;
	lookup_hint_t lhint;

	aal_assert("umka-2509", tree != NULL);
	aal_assert("umka-2510", hint != NULL);
	
	buff = hint->specific;
	reiser4_key_assign(&key, &hint->offset);
	
	for (total = 0, size = hint->count; size > 0; ) {
		int32_t read;
		reiser4_place_t place;

		lhint.level = LEAF_LEVEL;
		lhint.key = &hint->offset;
		lhint.collision = NULL;
	
		/* Looking for the place to read. */
		if ((res = reiser4_tree_lookup(tree, &lhint,
					       FIND_EXACT, &place)) < 0)
		{
			return res;
		}

		/* Data does not found. This may mean, that we have hole in tree
		   between keys. */
		if (res == ABSENT) {
			reiser4_key_t tkey;
			
			/* Here we suppose, that @place points to next item,
			   just behind the hole. */
			if (reiser4_place_right(&place))
				reiser4_place_inc(&place, 1);

			if ((res = reiser4_tree_place_key(tree, &place, &tkey)))
				return res;
			
			if (plug_call(tkey.plug->o.key_ops, compshort,
				      &tkey, &hint->offset))
			{
				/* No data found. */
				read = size;
			} else {
				uint64_t next, look, hole;
				
				next = reiser4_key_get_offset(&tkey);
				look = reiser4_key_get_offset(&hint->offset);

				hole = next - look;
				read = (hole > size ? size : hole);
			}
			
			/* If only hole is found, return 0. */
			if ((uint64_t)read == hint->count) {
				read = 0;
				break;
			}
			
			/* Making holes in buffer. */
			aal_memset(hint->specific, 0, read);
		} else {
			/* Prepare hint for read */
			hint->count = size;

#ifndef ENABLE_STAND_ALONE
			hint->blocks = tree->blocks;
#endif
			
			/* Read data from the tree */
			if ((read = reiser4_tree_read(tree, &place, hint)) < 0) {
				return read;
			} else {
				if (read == 0)
					break;
			}
		}

		size -= read;
		total += read;

		/* Updating key and data buffer pointer */
		hint->specific += read;
		reiser4_key_inc_offset(&hint->offset, read);
	}

	hint->specific = buff;
	reiser4_key_assign(&hint->offset, &key);
	
	return total;
}

#ifndef ENABLE_STAND_ALONE
/* Releases passed region in block allocator. This is used in tail during tree
   trunacte. */
static errno_t callback_release_region(void *entity, uint64_t start,
				       uint64_t width, void *data)
{
	reiser4_tree_t *tree = (reiser4_tree_t *)data;
	return reiser4_alloc_release(tree->fs->alloc, start, width);
}

/* Writes flow described by @hint to tree. Takes care about keys in index part
   of tree, root updatings, etc. Returns number of bytes actually written. */
int64_t reiser4_flow_write(reiser4_tree_t *tree, trans_hint_t *hint) {
	char *buff;
	errno_t res;
	uint64_t size;
	uint64_t bytes;
	uint64_t total;
	reiser4_key_t key;

	aal_assert("umka-2506", tree != NULL);
	aal_assert("umka-2507", hint != NULL);
	
	buff = hint->specific;
	reiser4_key_assign(&key, &hint->offset);

	/* Loop until desired number of bytes is written. */
	for (total = bytes = 0, size = hint->count; size > 0;) {
		int32_t write;
		uint32_t level;
		lookup_hint_t lhint;
		reiser4_place_t place;

		hint->count = size;
		hint->blocks = tree->blocks;

		lhint.level = LEAF_LEVEL;
		lhint.key = &hint->offset;
		lhint.collision = NULL;

		/* Looking for place to write. */
		if ((res = reiser4_tree_lookup(tree, &lhint,
					       FIND_CONV,
					       &place)) < 0)
		{
			return res;
		}

		/* level new item will be inserted a on. */
		level = reiser4_tree_target_level(tree, hint->plug);

		/* Writing data to tree. */
		if ((write = reiser4_tree_write(tree, &place,
						hint, level)) < 0)
		{
			return write;
		} else {
			if (write == 0)
				break;
		}

		/* Updating counters */
		size -= write;
		total += write;
		bytes += hint->bytes;
		
		/* Updating key and buffer pointer */
		if (hint->specific)
			hint->specific += write;
		
		reiser4_key_inc_offset(&hint->offset, write);
	}

	hint->bytes = bytes;
	hint->specific = buff;
	reiser4_key_assign(&hint->offset, &key);
	
	return total;
}

/* Truncates item pointed by @hint->offset key by value stored in
   @hint->count. This is used during tail conversion and in object plugins
   truncate() code path. */
int64_t reiser4_flow_truncate(reiser4_tree_t *tree, trans_hint_t *hint) {
	errno_t res;
	int64_t trunc;
	uint32_t size;
	uint64_t total;
	reiser4_key_t key;

	aal_assert("umka-2475", tree != NULL);
	aal_assert("umka-2476", hint != NULL);

	reiser4_key_assign(&key, &hint->offset);

	/* Setting up region func to release region callback. It is needed for
	   releasing extent blocks. */
	hint->region_func = callback_release_region;

	for (total = 0, size = hint->count; size > 0;
	     size -= trunc, total += trunc)
	{
		lookup_hint_t lhint;
		reiser4_place_t place;

		lhint.level = LEAF_LEVEL;
		lhint.key = &hint->offset;
		lhint.collision = NULL;
	
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
			
			if (plug_call(tkey.plug->o.key_ops, compshort,
				      &tkey, &hint->offset))
			{
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

		/* Calling node truncate method. */
		if ((trunc = reiser4_node_trunc(place.node, &place.pos,
						hint)) < 0)
		{
			return trunc;
		}
		
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
				reiser4_place_dup(&parent, &place.node->p);

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

	reiser4_key_assign(&hint->offset, &key);
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
	uint32_t blksize;
	trans_hint_t trans;
	
	aal_assert("umka-2406", tree != NULL);
	aal_assert("umka-2407", hint != NULL);
	aal_assert("umka-2481", hint->plug != NULL);

	blksize = reiser4_tree_get_blksize(tree);
	reiser4_key_assign(&trans.offset, &hint->offset);

	/* Check if convertion chunk is zero. If so -- use filesystem block
	   size. */
	if (hint->chunk == 0)
		hint->chunk = blksize;

	/* Check if number of bytes to be converted is not multiple of block
	   size and this is tail2extent conversion. If so, have to align
	   @hint->count byblock size into highest side. */
	if (hint->plug->id.group == EXTENT_ITEM &&
	    (hint->count & (blksize - 1)) != 0)
	{
		hint->count += blksize -
			(hint->count & (blksize - 1));
	}
	
	/* Loop until @size bytes is converted. */
	for (size = hint->count, hint->bytes = 0;
	     size > 0; size -= conv)
	{
		/* Each convertion tick may be divided onto tree stages:

		   (1) Read convert chunk (@hint->chunk bytes long now) to
		   @trans hint.

		   (2) Truncate data in tree we have just read described by
		   @trans hint.

		   (3) Write data back to tree with target item plugin used for
		   writing (tail plugin if we convert extents to tails and
		   extent plugin is used otherwise).
		*/
		
		/* Preparing buffer to read data to it. */
		trans.count = hint->chunk;

		if (trans.count > size)
			trans.count = size;

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
			/* If nothing was read, the hole will be inserted. */
			trans.specific = NULL;
		} else {
			/* Trunc & insert only read @count bytes. */
			trans.count = conv;
		}

		if ((conv = reiser4_flow_truncate(tree, &trans)) < 0) {
			res = conv;
			goto error_free_buff;
		}

		trans.count = conv;
		trans.plug = hint->plug;
		trans.shift_flags = SF_DEFAULT;
		trans.place_func = hint->place_func;
		
		/* Third stage -- writing data back to tree with new item plugin
		   used.*/
		if ((conv = reiser4_flow_write(tree, &trans)) < 0) {
			res = conv;
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
