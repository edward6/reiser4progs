/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40.c -- reiser4 tail (formatting) item plugin. */

#include "tail40.h"
#include "tail40_repair.h"

#include <aux/aux.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

/* Returns tail length. */
uint32_t tail40_units(reiser4_place_t *place) {
	return place->len - place->off;
}

/* Returns key from tail at @place. */
errno_t tail40_fetch_key(reiser4_place_t *place, 
			 reiser4_key_t *key) 
{
	uint32_t pos;
	
	aal_assert("vpf-627", key != NULL);
	aal_assert("vpf-626", place != NULL);

	pos = place->pos.unit;
	return body40_get_key(place, pos, key, NULL);
}

/* Reads units from tail at @place into passed @hint. */
int64_t tail40_read_units(reiser4_place_t *place,
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
	if (tail40_pos(place) + hint->count > place->len)
		count = place->len - tail40_pos(place);

	/* Copying data from tail body to hint. */
	aal_memcpy(hint->specific, place->body + tail40_pos(place), count);
	
	return count;
}

#ifndef ENABLE_MINIMAL
/* Estimates how many bytes in tree is needed to write @hint->count bytes of
   data. This function considers, that tail item is not expandable one. That is,
   tail will not be splitted at insert point, but will be rewritten instead. */
errno_t tail40_prep_write(reiser4_place_t *place, trans_hint_t *hint) {
	uint16_t space;
	
	aal_assert("umka-1836", hint != NULL);
	aal_assert("umka-2437", place != NULL);
	aal_assert("umka-3113", place->node != NULL);
	
	/* Check if we want to create new tail item. If so, we say, that we need
	   @hint->count bytes in tree. Even if this is more than one node can
	   fit, it is okay, because write function will actually write only
	   amount of data which may fit into node at passed @place. */
	if (place->pos.unit == MAX_UINT32) {
		hint->len = hint->count;
		hint->overhead = place->off;

		aal_memcpy(&hint->maxkey, &hint->offset, sizeof(hint->maxkey));
	} else {
		uint32_t right;
		uint64_t max_offset;

		aal_assert("umka-2284", place != NULL);

		/* Item already exists. We will rewrite some part of it and some
		   part have to be append. */
		right = place->len - tail40_pos(place);
		
		hint->len = (right >= hint->count ? 0 :
			     hint->count - right);

		/* Getting maximal real key. It will be needed to determine if
		   we insert data inside tail or behind it. */
		tail40_maxreal_key(place, &hint->maxkey);

		max_offset = plug_call(hint->maxkey.plug->o.key_ops,
				       get_offset, &hint->maxkey) + 1;

		plug_call(hint->maxkey.plug->o.key_ops,
			  set_offset, &hint->maxkey, max_offset);
	}

	/* Max possible item size. */
	space = plug_call(place->node->plug->o.node_ops,
			  maxspace, place->node);
	
	if (hint->len > space)
		hint->len = space - hint->overhead;
		
	
	return 0;
}

/* Rewrites tail from passed @place by data from @hint. */
int64_t tail40_write_units(reiser4_place_t *place, trans_hint_t *hint) {
	uint64_t ins_offset;
	uint64_t max_offset;
	uint32_t count;
	
	aal_assert("umka-1677", hint != NULL);
	aal_assert("umka-1678", place != NULL);

	hint->bytes = 0;
	count = hint->count;

	/* Check if we create new tail item. If so -- normalize insert pos. */
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	/* Calculating actual amount of data to be written. */
	if (count + tail40_pos(place) > place->len)
		count = place->len - tail40_pos(place);

	/* Getting old max real offset. */
	max_offset = plug_call(hint->maxkey.plug->o.key_ops,
			       get_offset, &hint->maxkey);

	/* Getting insert offset. */
	ins_offset = plug_call(hint->offset.plug->o.key_ops,
			       get_offset, &hint->offset);
	
	/* Checking if we insert a hole. That is @hint->specific si null. If so,
	   then we write @count of zeros. Writing data from @hint->specific
	   otherwise. */
	if (hint->specific) {
		/* Copying data into @place. */
		aal_memcpy(place->body + tail40_pos(place), 
			   hint->specific, count);
	} else {
		/* Making hole @count of size. */
		aal_memset(place->body + tail40_pos(place), 
			   0, count);
	}

	/* Updating item key if pos is zero, that is start of item. 
	   FIXME: this seems to do nothing. */
	if (place->pos.unit == 0)
		body40_get_key(place, 0, &place->key, NULL);

	/* Bytes are added if we wrote something behind of item size. */
	if (ins_offset + count > max_offset)
		hint->bytes = ins_offset + count - max_offset;
	
	place_mkdirty(place);
	return count;
}

/* Return max real key inside tail at @place. */
errno_t tail40_maxreal_key(reiser4_place_t *place, reiser4_key_t *key) {
	aal_assert("vpf-442", place != NULL);
	aal_assert("vpf-443", key != NULL);
	return body40_maxreal_key(place, key, NULL);
}
#endif

/* Return max possible key iside tail at @place. */
errno_t tail40_maxposs_key(reiser4_place_t *place, reiser4_key_t *key) {
	aal_assert("umka-1209", place != NULL);
	aal_assert("umka-1210", key != NULL);
	return body40_maxposs_key(place, key);
}

/* Makes lookup of @key inside tail at @place. */
lookup_t tail40_lookup(reiser4_place_t *place,
		       lookup_hint_t *hint,
		       lookup_bias_t bias)
{
	uint32_t units;
	uint64_t offset;
	uint64_t wanted;

	aal_assert("umka-1229", hint != NULL);
	aal_assert("umka-1228", place != NULL);

	units = tail40_units(place);
	
	offset = plug_call(hint->key->plug->o.key_ops,
			   get_offset, &place->key);

	wanted = plug_call(hint->key->plug->o.key_ops,
			   get_offset, hint->key);

	/* Check if needed key is inside this tail. */
	if (wanted >= offset && wanted < offset + units) {
		place->pos.unit = wanted - offset;
		return PRESENT;
	}

	place->pos.unit = units;
	return (bias == FIND_CONV ? PRESENT : ABSENT);
}

#ifndef ENABLE_MINIMAL
/* Estimates how many bytes may be shifted from @stc_place to @dst_place. */
errno_t tail40_prep_shift(reiser4_place_t *src_place,
			  reiser4_place_t *dst_place,
			  shift_hint_t *hint)
{
	int check_point;

	aal_assert("umka-2279", hint != NULL);
	aal_assert("umka-1664", src_place != NULL);

	check_point = (src_place->pos.item == hint->pos.item &&
		       hint->pos.unit != MAX_UINT32);

	/* Check if this is left shift. */
	if (hint->control & SF_ALLOW_LEFT) {
		/* Check if should take into account inert point from @hint. */
		if (hint->control & SF_UPDATE_POINT && check_point) {
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
					(dst_place ? dst_place->len - 
					 dst_place->off : 0);
			}
		} else {
			if (hint->units_bytes + src_place->off > src_place->len)
				hint->units_bytes = 
					src_place->len - src_place->off;
		}
	} else {
		uint32_t right;

		/* Check if should take into account insert point. */
		if (hint->control & SF_UPDATE_POINT && check_point) {
			/* Is insert point inside item? */
			if (hint->pos.unit + src_place->off < src_place->len) {
				right = src_place->len - hint->pos.unit - 
					src_place->off;

				/* Insert point inside item and we can move
				   something. */
				if (hint->units_bytes > right)
					hint->units_bytes = right;

				/* Updating insert point to first position in
				   neighbour item. */
				if (hint->pos.unit + src_place->off >= src_place->len && 
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
			if (hint->units_bytes + src_place->off > src_place->len)
			{
				hint->units_bytes = 
					src_place->len - src_place->off;
			}
		}
	}

	hint->units_number = hint->units_bytes;
	
	return 0;
}

/* Copy @count of units from @src_place at src_pos to @dst_place and
   @dst_pos. This function is used in balancing code. */
errno_t tail40_copy(reiser4_place_t *dst_place, uint32_t dst_pos,
		    reiser4_place_t *src_place, uint32_t src_pos,
		    uint32_t count)
{
	aal_assert("umka-2075", dst_place != NULL);
	aal_assert("umka-2076", src_place != NULL);

	if (count > 0) {
		aal_memcpy(dst_place->body + dst_pos + dst_place->off,
			   src_place->body + src_pos + src_place->off, count);

		place_mkdirty(dst_place);
	}
	
	return 0;
}

/* Expand tail at @place and @pos by @count bytes. Used in balancing code
   pathes. */
uint32_t tail40_expand(reiser4_place_t *place, uint32_t pos, uint32_t count) {
	uint32_t size;
	
	aal_assert("vpf-1559", place->len >= pos + count + place->off);
	
	size = place->len - pos - place->off - count;
	
	if (size && count) {
		aal_memmove(place->body + place->off + pos + count,
			    place->body + place->off + pos, size);

		place_mkdirty(place);
	}

	return 0;
}

/* Shrink tail at @place and @pos by @count bytes. Used in balancing code
   pathes. */
static uint32_t tail40_shrink(reiser4_place_t *place, uint32_t pos,
			      uint32_t count, uint32_t len)
{
	uint32_t size;
	
	aal_assert("vpf-1764", place->len >= pos + count + place->off);
	
	size = place->len - pos - place->off - count;
	
	if (size && count) { 
		aal_memmove(place->body + pos + place->off,
			    place->body + pos + place->off + count, size);
		
		place_mkdirty(place);
	}

	return 0;
}

/* Shift some number of units from @src_place to @dst_place. All actions are
   performed with keeping in mind passed @hint. */
errno_t tail40_shift_units(reiser4_place_t *src_place,
			   reiser4_place_t *dst_place,
			   shift_hint_t *hint)
{
	uint32_t pos;
	uint64_t offset;
	
	aal_assert("umka-1665", src_place != NULL);
	aal_assert("umka-1666", dst_place != NULL);
	aal_assert("umka-1667", hint != NULL);

	/* Check if this is left shift. */
	if (hint->control & SF_ALLOW_LEFT) {
		pos = dst_place->len - dst_place->off - hint->units_number;
		
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
		pos = src_place->len - src_place->off - hint->units_number;
		
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
int64_t tail40_trunc_units(reiser4_place_t *place, trans_hint_t *hint) {
	uint64_t count;

	aal_assert("umka-2480", hint != NULL);
	aal_assert("umka-2479", place != NULL);

	/* Correcting position. */
	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;

	/* Correcting count. */
	count = hint->count;
	
	if (tail40_pos(place) + count > place->len)
		count = place->len - tail40_pos(place);

	/* Taking care about rest of tail */
	if (tail40_pos(place) + count < place->len) {
		aal_memmove(place->body + tail40_pos(place),
			    place->body + tail40_pos(place) + count,
			    place->len - (tail40_pos(place) + count));
	}

	/* Updating key if it is needed. */
	if (place->pos.unit == 0 && tail40_pos(place) + count < place->len) {
		body40_get_key(place, place->pos.unit + count,
			       &place->key, NULL);
	}
	
	hint->overhead = count == (place->len - place->off) ? place->off : 0;
	hint->len = count;
	hint->bytes = count;
	
	return count;
}

/* Returns item size in bytes */
uint64_t tail40_size(reiser4_place_t *place) {
	aal_assert("vpf-1210", place != NULL);
	return place->len - place->off;
}
#endif
