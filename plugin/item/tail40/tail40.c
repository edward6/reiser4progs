/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40.c -- reiser4 default tail plugin. */

#include "tail40.h"
#include "tail40_repair.h"

#include <aux/aux.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

static reiser4_core_t *core = NULL;

/* Returns tail length */
uint32_t tail40_number_units(place_t *place) {
	return place->len;
}

/* Returns the key of the specified unit */
errno_t tail40_fetch_key(place_t *place, 
			 key_entity_t *key) 
{
	uint32_t pos;
	
	aal_assert("vpf-627", key != NULL);
	aal_assert("vpf-626", place != NULL);

	pos = place->pos.unit;
	return body40_get_key(place, pos, key, NULL);
}

static int64_t tail40_read_units(place_t *place,
				 trans_hint_t *hint)
{
	uint32_t count;
	
	aal_assert("umka-1674", hint != NULL);
	aal_assert("umka-1673", place != NULL);

	count = hint->count;

	if (place->pos.unit == MAX_UINT32)
		place->pos.unit = 0;
	
	if (place->pos.unit + hint->count > place->len)
		count = place->len - place->pos.unit;
			
	aal_memcpy(hint->specific, place->body +
		   place->pos.unit, count);
	
	return count;
}

#ifndef ENABLE_STAND_ALONE
/* Estimates how many byte need to write @hint->count into the tree. This
   function considers also, that tail item is not expandable one. That is, if
   insert pos point inside the item body, it will not be splitted, but rewritten
   instead. */
static errno_t tail40_prep_write(place_t *place,
				 trans_hint_t *hint)
{
	aal_assert("umka-1836", hint != NULL);
	aal_assert("umka-2437", place != NULL);

	if (place->pos.unit == MAX_UINT32) {
		hint->len = hint->count;
	} else {
		uint32_t right;

		aal_assert("umka-2284", place != NULL);
		
		right = place->len - place->pos.unit;
		
		hint->len = (right >= hint->count ? 0 :
			     hint->count - right);
	}
	
	return 0;
}

/* Rewrites tail from passed @pos by data from hint */
static int64_t tail40_write_units(place_t *place,
				  trans_hint_t *hint)
{
	uint32_t count, pos;
	
	aal_assert("umka-1677", hint != NULL);
	aal_assert("umka-1678", place != NULL);

	count = hint->count;
        pos = place->pos.unit;

	if (pos == MAX_UINT32)
		pos = 0;
	
	if (count > place->len - pos)
		count = place->len - pos;

	/* Checking if we insert hole */
	if (hint->specific) {

		/* Copying new data into place */
		aal_memcpy(place->body + pos,
			   hint->specific, count);
	} else {
		/* Making hole of size @count */
		aal_memset(place->body + pos,
			   0, count);
	}

	/* Updating the key */
	if (pos == 0) {
		body40_get_key(place, 0,
			       &place->key, NULL);
	}

	hint->bytes = count;
	place_mkdirty(place);
                                                                                       
	return count;
}

static errno_t tail40_print(place_t *place, aal_stream_t *stream,
			    uint16_t options)
{
	aal_assert("umka-1489", place != NULL);
	aal_assert("umka-1490", stream != NULL);

	aal_stream_format(stream, "TAIL PLUGIN=%s LEN=%u, KEY=[%s]\n",
			  place->plug->label, place->len,
			  core->key_ops.print(&place->key, PO_DEF));
	return 0;
}

errno_t tail40_maxreal_key(place_t *place, key_entity_t *key) {
	aal_assert("vpf-442", place != NULL);
	aal_assert("vpf-443", key != NULL);

	return body40_maxreal_key(place, key, NULL);
}
#endif

static errno_t tail40_maxposs_key(place_t *place,
				  key_entity_t *key) 
{
	aal_assert("umka-1209", place != NULL);
	aal_assert("umka-1210", key != NULL);

	return body40_maxposs_key(place, key);
}

static lookup_t tail40_lookup(place_t *place,
			      key_entity_t *key, 
			      bias_t bias)
{
	uint32_t units;
	uint64_t offset;
	uint64_t wanted;

	aal_assert("umka-1229", key != NULL);
	aal_assert("umka-1228", place != NULL);

	units = tail40_number_units(place);
	
	offset = plug_call(key->plug->o.key_ops,
			   get_offset, &place->key);

	wanted = plug_call(key->plug->o.key_ops,
			   get_offset, key);

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
static int tail40_mergeable(place_t *place1, place_t *place2) {
	aal_assert("umka-2201", place1 != NULL);
	aal_assert("umka-2202", place2 != NULL);

	return body40_mergeable(place1, place2);
}

/* Estimates how many bytes may be shifted into neighbour item */
static errno_t tail40_prep_shift(place_t *src_place,
				 place_t *dst_place,
				 shift_hint_t *hint)
{
	aal_assert("umka-2279", hint != NULL);
	aal_assert("umka-1664", src_place != NULL);

	/* Check if we have to check for insert point to be staying inside src
	   place. This is usually needed. Otherwise, we want to shift
	   everything. */
	if (!(src_place->pos.item == hint->pos.item &&
	      hint->pos.unit != MAX_UINT32))
	{
		if (hint->rest > src_place->len)
			hint->rest = src_place->len;
	
		hint->units = hint->rest;
		
		return 0;
	}
	
	if (hint->control & MSF_LEFT) {

		/* Can we update insert point? */
		if (hint->control & MSF_IPUPDT) {

			/* Correcting @hint->rest. It should contains number of
			   bytes we realy can move out. */
			if (hint->rest > hint->pos.unit)
				hint->rest = hint->pos.unit;

			hint->pos.unit -= hint->rest;

			/* Moving insert point into neighbour item */
			if (hint->pos.unit == 0 && hint->control & MSF_IPMOVE) {
				hint->result |= MSF_IPMOVE;

				hint->pos.unit = hint->rest +
					(dst_place ? dst_place->len : 0);
			}
		}
	} else {
		uint32_t right;

		if (hint->control & MSF_IPUPDT) {
			
			/* Is insert point inside item? */
			if (hint->pos.unit < src_place->len) {
				right = src_place->len - hint->pos.unit;

				/* Insert point inside item and we can move
				   something. */
				if (hint->rest > right)
					hint->rest = right;

				hint->pos.unit += hint->rest;

				/* Updating insert point to first position in
				   neighbour item. */
				if (hint->pos.unit == src_place->len &&
				    hint->control & MSF_IPMOVE)
				{
					hint->result |= MSF_IPMOVE;
					hint->pos.unit = 0;
				}
			} else {
				/* Updating insert point to first position in
				   neighbour item. */
				if (hint->control & MSF_IPMOVE) {
					hint->result |= MSF_IPMOVE;
					hint->pos.unit = 0;
				}

				hint->rest = 0;
			}
		}
	}

	return 0;
}

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

static uint32_t tail40_expand(place_t *place, uint32_t pos,
			      uint32_t count, uint32_t len)
{
	if (pos < place->len) {
		aal_memmove(place->body + pos + count,
			    place->body + pos, place->len - pos);

		place_mkdirty(place);
	}

	return 0;
}

static uint32_t tail40_shrink(place_t *place, uint32_t pos,
			      uint32_t count, uint32_t len)
{
	if (pos < place->len) {
		aal_memmove(place->body + pos,
			    place->body + pos + count,
			    place->len - (pos + count));

		place_mkdirty(place);
	}

	return 0;
}

static errno_t tail40_shift_units(place_t *src_place,
				  place_t *dst_place,
				  shift_hint_t *hint)
{
	uint64_t offset;
	
	aal_assert("umka-1665", src_place != NULL);
	aal_assert("umka-1666", dst_place != NULL);
	aal_assert("umka-1667", hint != NULL);

	if (hint->control & MSF_LEFT) {
		tail40_expand(dst_place, dst_place->len,
			     hint->units, hint->rest);
		
		tail40_copy(dst_place, dst_place->len,
			    src_place, 0, hint->rest);
		
		tail40_shrink(src_place, 0, hint->units,
			      hint->rest);

		/* Updating item's key by the first unit key */
		offset = plug_call(src_place->key.plug->o.key_ops,
				   get_offset, &src_place->key);

		plug_call(src_place->key.plug->o.key_ops,
			  set_offset, &src_place->key,
			  offset + hint->rest);
	} else {
		uint32_t pos;
		
		tail40_expand(dst_place, 0, hint->units,
			      hint->rest);

		pos = src_place->len - hint->units;
		
		tail40_copy(dst_place, 0, src_place, pos, hint->rest);
		tail40_shrink(src_place, pos, hint->units, hint->rest);

		offset = plug_call(dst_place->key.plug->o.key_ops,
				   get_offset, &dst_place->key);
		
		plug_call(dst_place->key.plug->o.key_ops,
			  set_offset, &dst_place->key,
			  offset - hint->rest);
	}
	
	return 0;
}

static int64_t tail40_trunc_units(place_t *place,
				  trans_hint_t *hint)
{
	uint32_t pos;
	uint64_t count;

	aal_assert("umka-2480", hint != NULL);
	aal_assert("umka-2479", place != NULL);

	/* Correcting position */
	pos = place->pos.unit;

	if (pos == MAX_UINT32)
		pos = 0;

	/* Correcting count */
	count = hint->count;
	
	if (pos + count > place->len)
		count = place->len - pos;

	/* Taking care about rest of tail */
	if (pos + count < place->len) {
		aal_memmove(place->body + pos,
			    place->body + pos + count,
			    place->len - (pos + count));
	}

	/* Updating key */
	if (pos == 0 && pos + count < place->len) {
		body40_get_key(place, pos + count,
			       &place->key, NULL);
	}
	
	hint->ohd = 0;
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
	.update_key       = NULL,
	
	.mergeable        = tail40_mergeable,
	.maxreal_key      = tail40_maxreal_key,
	.prep_shift       = tail40_prep_shift,
	.shift_units      = tail40_shift_units,
#endif
	.lookup           = tail40_lookup,
	.fetch_key        = tail40_fetch_key,
	.number_units     = tail40_number_units,
	.maxposs_key      = tail40_maxposs_key
};

static item_object_ops_t object_ops = {
#ifndef ENABLE_STAND_ALONE
	.size             = tail40_size,
	.bytes            = tail40_size,
	.prep_write       = tail40_prep_write,
	.write_units      = tail40_write_units,
	.trunc_units      = tail40_trunc_units,
	
	.prep_insert      = NULL,
	.insert_units     = NULL,
	.remove_units     = NULL,
	.update_units     = NULL,
	.layout	          = NULL,
#endif
	.fetch_units      = NULL,
	.object_plug      = NULL,
	
	.read_units       = tail40_read_units
};

static item_repair_ops_t repair_ops = {
#ifndef ENABLE_STAND_ALONE
	.prep_merge       = tail40_prep_merge,
	.merge_units      = tail40_merge_units,
	
	.check_struct	  = NULL,
	.check_layout     = NULL
#endif
};

static item_debug_ops_t debug_ops = {
#ifndef ENABLE_STAND_ALONE
	.print	          = tail40_print
#endif
};

static item_tree_ops_t tree_ops = {
	.down_link        = NULL,
#ifndef ENABLE_STAND_ALONE
	.update_link      = NULL
#endif
};

static reiser4_item_ops_t tail40_ops = {
	.tree             = &tree_ops,
	.debug            = &debug_ops,
	.object           = &object_ops,
	.repair           = &repair_ops,
	.balance          = &balance_ops
};

static reiser4_plug_t tail40_plug = {
	.cl    = CLASS_INIT,
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
	core = c;
	return &tail40_plug;
}

plug_register(tail40, tail40_start, NULL);
