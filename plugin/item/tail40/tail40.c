/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40.c -- reiser4 tail items plugin. */

#include "tail40.h"
#include "tail40_repair.h"

#include <aux/aux.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

reiser4_core_t *tail40_core;

/* Returns tail length. */
uint32_t tail40_units(place_t *place) {
	return place->len;
}

/* Returns key from tail at @place. */
errno_t tail40_fetch_key(place_t *place, 
			 key_entity_t *key) 
{
	uint32_t pos;
	
	aal_assert("vpf-627", key != NULL);
	aal_assert("vpf-626", place != NULL);

	pos = place->pos.unit;
	return body40_get_key(place, pos, key, NULL);
}

/* Reads units from tail at @place into passed @hint. */
static int64_t tail40_read_units(place_t *place,
				 trans_hint_t *hint)
{
	uint32_t count;
	
	aal_assert("umka-1674", hint != NULL);
	aal_assert("umka-1673", place != NULL);

	count = hint->count;

	/* Check if we read from the start of item. If so normilize position to
	   read from. Value MAX_UINT32 is used in libreiser4 for denoting that
	   we want to do something to whole item, that is from its start. */
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	/* Calculating number of bytes, which can be actually read from this
	   tail item. It cannot be more than item length. */
	if (place->pos.unit + hint->count > place->len)
		count = place->len - place->pos.unit;

	/* Copying data from tail body to hint. */
	aal_memcpy(hint->specific, place->body +
		   place->pos.unit, count);
	
	return count;
}

#ifndef ENABLE_STAND_ALONE
/* Estimates how many bytes in tree is needed to write @hint->count bytes of
   data. This function considers, that tail item is not expandable one. That is,
   tail will not be splitted at insert point, but will be rewritten instead. */
static errno_t tail40_prep_write(place_t *place,
				 trans_hint_t *hint)
{
	aal_assert("umka-1836", hint != NULL);
	aal_assert("umka-2437", place != NULL);

	/* Check if we want to create new tail item. If so, we say, that we need
	   @hint->count bytes in tree. Even if this is more than one node can
	   fit, it is okay, because write function will actually write only
	   amount of data which may fit into node at passed @place. */
	if (place->pos.unit == MAX_UINT32) {
		hint->len = hint->count;
	} else {
		uint32_t right;

		aal_assert("umka-2284", place != NULL);

		/* Item already exists. We will rewrite some part of it and some
		   part have to be append. */
		right = place->len - place->pos.unit;
		
		hint->len = (right >= hint->count ? 0 :
			     hint->count - right);
	}
	
	return 0;
}

/* Rewrites tail from passed @place by data from @hint. */
static int64_t tail40_write_units(place_t *place,
				  trans_hint_t *hint)
{
	uint32_t count, pos;
	
	aal_assert("umka-1677", hint != NULL);
	aal_assert("umka-1678", place != NULL);

	count = hint->count;
        pos = place->pos.unit;

	/* Check if we create new tail item. If so -- normalize insert pos. */
	if (pos == MAX_UINT32)
		pos = 0;

	/* Calculating actual amount of data to be written. */
	if (count > place->len - pos)
		count = place->len - pos;

	/* Checking if we insert a hole. That is @hint->specific si null. If so,
	   then we write @count of zeros. Writing data from @hint->specific
	   otherwise. */
	if (hint->specific) {
		/* Copying data into @place. */
		aal_memcpy(place->body + pos, hint->specific, count);
	} else {
		/* Making hole @count of size. */
		aal_memset(place->body + pos, 0, count);
	}

	/* Updating item key if pos is zero, that is start of item. */
	if (pos == 0) {
		body40_get_key(place, 0, &place->key, NULL);
	}

	hint->bytes = count;
	place_mkdirty(place);
                                                                                       
	return count;
}

/* Return max real key inside tail at @place. */
errno_t tail40_maxreal_key(place_t *place, key_entity_t *key) {
	aal_assert("vpf-442", place != NULL);
	aal_assert("vpf-443", key != NULL);
	return body40_maxreal_key(place, key, NULL);
}
#endif

/* Return max possible key iside tail at @place. */
static errno_t tail40_maxposs_key(place_t *place,
				  key_entity_t *key) 
{
	aal_assert("umka-1209", place != NULL);
	aal_assert("umka-1210", key != NULL);
	return body40_maxposs_key(place, key);
}

/* Makes lookup of @key inside tail at @place. */
static lookup_t tail40_lookup(place_t *place,
			      key_entity_t *key, 
			      bias_t bias)
{
	uint32_t units;
	uint64_t offset;
	uint64_t wanted;

	aal_assert("umka-1229", key != NULL);
	aal_assert("umka-1228", place != NULL);

	units = tail40_units(place);
	
	offset = plug_call(key->plug->o.key_ops,
			   get_offset, &place->key);

	wanted = plug_call(key->plug->o.key_ops,
			   get_offset, key);

	/* Check if needed key is inside this tail. */
	if (wanted >= offset &&
	    wanted < offset + units)
	{
		place->pos.unit = wanted - offset;
		return PRESENT;
	}

	place->pos.unit = units;
	return (bias == FIND_CONV ? PRESENT : ABSENT);
}

#ifndef ENABLE_STAND_ALONE
/* Return 1 if two tail items are mergeable. Otherwise 0 will be returned. This
   method is used in balancing to determine if two border items may be
   merged. */
static int tail40_mergeable(place_t *place1, place_t *place2) {
	aal_assert("umka-2201", place1 != NULL);
	aal_assert("umka-2202", place2 != NULL);
	return body40_mergeable(place1, place2);
}

/* Estimates how many bytes may be shifted from @stc_place to @dst_place. */
static errno_t tail40_prep_shift(place_t *src_place,
				 place_t *dst_place,
				 shift_hint_t *hint)
{
	aal_assert("umka-2279", hint != NULL);
	aal_assert("umka-1664", src_place != NULL);

	hint->units_number = 0;
	
	/* Check if we have to check if insert point should be staying inside
	   @src_place. Otherwise, we want to shift everything. */
	if (src_place->pos.item != hint->pos.item ||
	    hint->pos.unit == MAX_UINT32)
	{
		if (hint->units_bytes > src_place->len)
			hint->units_bytes = src_place->len;
	
		hint->units_number = hint->units_bytes;
		return 0;
	}

	/* Check if this is left shift. */
	if (hint->control & SF_ALLOW_LEFT) {
		/* Are we able to update insert point? */
		if (hint->control & SF_UPDATE_POINT) {
			/* Correcting @hint->rest. It should contain number of
			   bytes we realy can shift. */
			if (hint->units_bytes > hint->pos.unit)
				hint->units_bytes = hint->pos.unit;

			hint->pos.unit -= hint->units_bytes;

			/* Moving insert point to @dst_place. */
			if (hint->pos.unit == 0 &&
			    hint->control & SF_MOVE_POINT)
			{
				hint->result |= SF_MOVE_POINT;

				hint->pos.unit = hint->units_bytes +
					(dst_place ? dst_place->len : 0);
			}
		} else {
			if (hint->units_bytes > src_place->len)
				hint->units_bytes = src_place->len;
		}
	} else {
		uint32_t right;

		if (hint->control & SF_UPDATE_POINT) {
			/* Is insert point inside item? */
			if (hint->pos.unit < src_place->len) {
				right = src_place->len - hint->pos.unit;

				/* Insert point inside item and we can move
				   something. */
				if (hint->units_bytes > right)
					hint->units_bytes = right;

				hint->pos.unit += hint->units_bytes;

				/* Updating insert point to first position in
				   neighbour item. */
				if (hint->pos.unit >= src_place->len &&
				    hint->control & SF_MOVE_POINT)
				{
					hint->result |= SF_MOVE_POINT;
					hint->pos.unit = 0;
				}
			} else {
				/* Updating insert point to first position in
				   neighbour item. */
				if (hint->control & SF_MOVE_POINT) {
					hint->result |= SF_MOVE_POINT;
					hint->pos.unit = 0;
				}

				hint->units_bytes = 0;
			}
		} else {
			if (hint->units_bytes > src_place->len)
				hint->units_bytes = src_place->len;
		}
	}

	hint->units_number = hint->units_bytes;
	
	return 0;
}

/* Copy @count of units from @src_place at src_pos to @dst_place and
   @dst_pos. This function is used in balancing code. */
errno_t tail40_copy(place_t *dst_place, uint32_t dst_pos,
		    place_t *src_place, uint32_t src_pos,
		    uint32_t count)
{
	aal_assert("umka-2075", dst_place != NULL);
	aal_assert("umka-2076", src_place != NULL);

	if (count > 0) {
		aal_memcpy(dst_place->body + dst_pos,
			   src_place->body + src_pos, count);

		place_mkdirty(dst_place);
	}
	
	return 0;
}

/* Expand tail at @place and @pos by @count bytes. Used in balancing code
   pathes. */
uint32_t tail40_expand(place_t *place, uint32_t pos, uint32_t count) {
	if (pos < place->len) {
		uint32_t size = place->len - count - pos;
		
		aal_memmove(place->body + pos + count,
			    place->body + pos, size);

		place_mkdirty(place);
	}

	return 0;
}

/* Shrink tail at @place and @pos by @count bytes. Used in balancing code
   pathes. */
static uint32_t tail40_shrink(place_t *place, uint32_t pos,
			      uint32_t count, uint32_t len)
{
	if (pos < place->len) {
		uint32_t size;
		void *src, *dst;

		dst = place->body + pos;
		src = place->body + pos + count;
		size = place->len - (pos + count);
		
		aal_memmove(dst, src, size);
		place_mkdirty(place);
	}

	return 0;
}

/* Shift some number of units from @src_place to @dst_place. All actions are
   performed with keeping in mind passed @hint. */
static errno_t tail40_shift_units(place_t *src_place,
				  place_t *dst_place,
				  shift_hint_t *hint)
{
	uint32_t pos;
	uint64_t offset;
	
	aal_assert("umka-1665", src_place != NULL);
	aal_assert("umka-1666", dst_place != NULL);
	aal_assert("umka-1667", hint != NULL);

	/* Check if this is left shift. */
	if (hint->control & SF_ALLOW_LEFT) {
		pos = dst_place->len - hint->units_number;
		
		/* Expanding tail item at @dst_place by @hint->units_number
		   value. It is that, prep_shift() has prepared for us. */
		tail40_expand(dst_place, pos, hint->units_number);

		/* Copy @hint->units_bytes units from @src_place to @dst_place
		   at @dst_place->len position. */
		tail40_copy(dst_place, pos, src_place, 0,
			    hint->units_bytes);

		/* Shrink @src_place at 0 position by @hint->rest bytes, that is
		   by number of bytes we have just moved to @dst_place. */
		tail40_shrink(src_place, 0, hint->units_number,
			      hint->units_bytes);

		/* Updating @place->key in order to maintain consistency of left
		   delimiting keys. */
		offset = plug_call(src_place->key.plug->o.key_ops,
				   get_offset, &src_place->key);

		plug_call(src_place->key.plug->o.key_ops,
			  set_offset, &src_place->key,
			  offset + hint->units_bytes);
	} else {
		/* Right shift. Expanding @dst_place at 0 pos. */
		tail40_expand(dst_place, 0, hint->units_number);

		/* Copying data and removing it from source. */
		pos = src_place->len - hint->units_number;
		
		tail40_copy(dst_place, 0, src_place, pos,
			    hint->units_bytes);
		
		tail40_shrink(src_place, pos, hint->units_number,
			      hint->units_bytes);

		/* Updating place key. */
		offset = plug_call(dst_place->key.plug->o.key_ops,
				   get_offset, &dst_place->key);
		
		plug_call(dst_place->key.plug->o.key_ops,
			  set_offset, &dst_place->key,
			  offset - hint->units_bytes);
	}
	
	return 0;
}

/* Removes some number of units from tail at @place based on @hint. This is used
   in trunc_flow() code path. That is in tail convertion code and in object
   truncate() functions. */
static int64_t tail40_trunc_units(place_t *place,
				  trans_hint_t *hint)
{
	uint32_t pos;
	uint64_t count;

	aal_assert("umka-2480", hint != NULL);
	aal_assert("umka-2479", place != NULL);

	/* Correcting position. */
	pos = place->pos.unit;

	if (pos == MAX_UINT32)
		pos = 0;

	/* Correcting count. */
	count = hint->count;
	
	if (pos + count > place->len)
		count = place->len - pos;

	/* Taking care about rest of tail */
	if (pos + count < place->len) {
		aal_memmove(place->body + pos,
			    place->body + pos + count,
			    place->len - (pos + count));
	}

	/* Updating key if it is needed. */
	if (pos == 0 && pos + count < place->len) {
		body40_get_key(place, pos + count,
			       &place->key, NULL);
	}
	
	hint->overhead = 0;
	hint->len = count;
	hint->bytes = count;
	
	return count;
}

/* Returns item size in bytes */
static uint64_t tail40_size(place_t *place) {
	aal_assert("vpf-1210", place != NULL);
	return place->len;
}
#endif

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_STAND_ALONE
	.fuse		  = NULL,
	.update_key	  = NULL,
	.mergeable	  = tail40_mergeable,
	.maxreal_key	  = tail40_maxreal_key,
	.prep_shift	  = tail40_prep_shift,
	.shift_units	  = tail40_shift_units,
#endif
	.units            = tail40_units,
	.lookup		  = tail40_lookup,
	.fetch_key	  = tail40_fetch_key,
	.maxposs_key	  = tail40_maxposs_key
};

static item_object_ops_t object_ops = {
#ifndef ENABLE_STAND_ALONE
	.size		  = tail40_size,
	.bytes		  = tail40_size,
	.prep_write	  = tail40_prep_write,
	.write_units	  = tail40_write_units,
	.trunc_units	  = tail40_trunc_units,
	
	.prep_insert	  = NULL,
	.insert_units	  = NULL,
	.remove_units	  = NULL,
	.update_units	  = NULL,
	.layout		  = NULL,
#endif
	.fetch_units	  = NULL,
	.object_plug	  = NULL,
	.read_units	  = tail40_read_units
};

static item_repair_ops_t repair_ops = {
#ifndef ENABLE_STAND_ALONE
	.check_struct	  = NULL,
	.check_layout	  = NULL,

	.prep_merge	  = tail40_prep_merge,
	.merge		  = tail40_merge,

	.pack		  = tail40_pack,
	.unpack		  = tail40_unpack
#endif
};

static item_debug_ops_t debug_ops = {
#ifndef ENABLE_STAND_ALONE
	.print		  = NULL
#endif
};

static item_tree_ops_t tree_ops = {
	.down_link	  = NULL,
#ifndef ENABLE_STAND_ALONE
	.update_link	  = NULL
#endif
};

static reiser4_item_ops_t tail40_ops = {
	.tree		  = &tree_ops,
	.debug		  = &debug_ops,
	.object		  = &object_ops,
	.repair		  = &repair_ops,
	.balance	  = &balance_ops
};

static reiser4_plug_t tail40_plug = {
	.cl    = class_init,
	.id    = {ITEM_TAIL40_ID, TAIL_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "tail40",
	.desc  = "Tail item for reiser4, ver. " VERSION,
#endif
	.o = {
		.item_ops = &tail40_ops
	}
};

static reiser4_plug_t *tail40_start(reiser4_core_t *c) {
	tail40_core = c;
	return &tail40_plug;
}

plug_register(tail40, tail40_start, NULL);
