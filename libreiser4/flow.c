/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
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

	buff = hint->specific;
	reiser4_key_assign(&key, &hint->offset);
	
	for (total = 0, size = hint->count; size > 0; ) {
		int32_t read;
		place_t place;

		/* Looking for the place to read. */
		if ((res = reiser4_tree_lookup(tree, &hint->offset,
					       LEAF_LEVEL, FIND_EXACT,
					       &place)) < 0)
		{
			return res;
		}

		/* Data does not found. This may mean, that we have hole in tree
		   between keys. */
		if (res == ABSENT) {
			uint64_t hole_size;
			uint64_t next_offset;
			uint64_t look_offset;
			
			/* Here we suppose, that @place points to next item,
			   just behind the hole. */
			if ((res = reiser4_place_fetch(&place)))
				return res;

			next_offset = reiser4_key_get_offset(&place.key);
			look_offset = reiser4_key_get_offset(&hint->offset);

			hole_size = next_offset - look_offset;
			read = (hole_size > size ? size : hole_size);

			/* Making holes in buffer */
			aal_memset(hint->specific, 0, read);
		} else {
			/* Prepare hint for read */
			hint->tree = tree;
			hint->count = size;
		
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

	buff = hint->specific;
	reiser4_key_assign(&key, &hint->offset);

	/* Loop until desired number of bytes is written. */
	for (total = bytes = 0, size = hint->count; size > 0;) {
		int32_t write;
		uint32_t level;
		place_t place;

		hint->count = size;

		/* Looking for place to write. */
		if ((res = reiser4_tree_lookup(tree, &hint->offset,
					       LEAF_LEVEL, FIND_CONV,
					       &place)) < 0)
		{
			return res;
		}

		/* Making decission if we should write data to leaf level or to
		   twig. Probably this may be improved somehow. */
		if (hint->plug->id.group == TAIL_ITEM) {
			level = LEAF_LEVEL;
		} else {
			level = TWIG_LEVEL;
		}

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
	key_entity_t key;

	aal_assert("umka-2475", tree != NULL);
	aal_assert("umka-2476", hint != NULL);

	reiser4_key_assign(&key, &hint->offset);

	/* Setting up region func to release region callback. It is needed for
	   releasing extent blocks. */
	hint->region_func = callback_release_region;

	for (total = 0, size = hint->count; size > 0;
	     size -= trunc, total += trunc)
	{
		place_t place;
		
		if ((res = reiser4_tree_lookup(tree, &hint->offset,
					       LEAF_LEVEL, FIND_EXACT,
					       &place)) < 0)
		{
			return res;
		}

		/* Nothing found by @hint->offset. This means, that tree has a
		   hole between keys. We will handle this, as it is needed for
		   fsck. */
		if (res == ABSENT) {
			uint64_t hole_size;
			uint64_t next_offset;
			uint64_t look_offset;

			/* Emulating truncating unexistent item. */
			if ((res = reiser4_place_fetch(&place)))
				return res;

			next_offset = reiser4_key_get_offset(&place.key);
			look_offset = reiser4_key_get_offset(&hint->offset);

			hole_size = next_offset - look_offset;
			trunc = (hole_size > size ? size : hole_size);

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
				place_t p;
				reiser4_key_t lkey;

				/* Updating parent keys */
				reiser4_node_leftmost_key(place.node, &lkey);
				
				reiser4_place_init(&p, place.node->p.node,
						   &place.node->p.pos);

				reiser4_key_assign(&place.node->p.key, &lkey);

				if ((res = reiser4_tree_update_key(tree, &p,
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
	trans_hint_t trans;
	
	aal_assert("umka-2406", tree != NULL);
	aal_assert("umka-2407", hint != NULL);
	aal_assert("umka-2481", hint->plug != NULL);

	reiser4_key_assign(&trans.offset, &hint->offset);

	/* Check if convertion chunk is zero. If so -- use filesystem block
	   size. */
	if (hint->chunk == 0)
		hint->chunk = reiser4_tree_get_blksize(tree);
	
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
		trans.count = conv;

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
