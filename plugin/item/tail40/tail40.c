/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   tail40.c -- reiser4 default tail plugin. */

#include <aux/aux.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

#define tail40_body(place) (place->body)

static reiser4_core_t *core = NULL;

/* Returns tail length */
uint32_t tail40_units(place_t *place) {
	return place->len;
}

/* Returns the key of the specified unit */
errno_t tail40_get_key(place_t *place, 
		       key_entity_t *key) 
{
	aal_assert("vpf-627", key != NULL);
	aal_assert("vpf-626", place != NULL);
	
	return body40_get_key(place, place->pos.unit, key, NULL);
}

static int32_t tail40_read(place_t *place, void *buff,
			   uint32_t pos, uint32_t count)
{
	aal_assert("umka-1673", place != NULL);
	aal_assert("umka-1674", buff != NULL);
	aal_assert("umka-1675", pos < place->len);

	aal_memcpy(buff, place->body + pos, count);
	return count;
}

#ifndef ENABLE_STAND_ALONE
/* Estimates how many byte need to write @hint->count into the tree. This
   function considers also, that tail item is not expandable one. That is, if
   insert pos point inside the item body, it will not be splitted, but rewritten
   instead. */
static errno_t tail40_estimate_insert(place_t *place,
				      insert_hint_t *hint)
{
	aal_assert("umka-1836", hint != NULL);

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

/* Rewrites tail from passed @pos by data specifed by hint */
static errno_t tail40_insert(place_t *place,
			     insert_hint_t *hint)
{
	uint32_t pos;
	uint32_t count;
	
	aal_assert("umka-1677", hint != NULL);
	aal_assert("umka-1678", place != NULL);

	count = hint->count;
	pos = place->pos.unit;
	
	if (count > place->len - pos)
		count = place->len - pos;

	/* Checking if we insert hole */
	if (hint->specific) {
		/* Copying new data into place */
		aal_memcpy(place->body + pos,
			   hint->specific, count);
	} else {
		/* Making hole of size @count */
		aal_memset(place->body + pos, 0, count);
	}

	/* Updating the key */
	if (pos == 0) {
		body40_get_key(place, 0, &place->key, NULL);
	}

	hint->bytes = count;
	place_mkdirty(place);
	
	return 0;
}

/* Removes the part of tail body */
static errno_t tail40_remove(place_t *place,
			     remove_hint_t *hint)
{
	uint32_t pos;
	uint32_t count;
	void *src, *dst;
	
	aal_assert("umka-2403", hint != NULL);
	aal_assert("umka-1661", place != NULL);
	aal_assert("umka-1663", pos < place->len);

	count = hint->count;
	pos = place->pos.unit;
	
	if (pos + count > place->len)
		count = place->len - pos;
			
	hint->ohd = 0;
	hint->len = place->len - (pos + count);
	
	src = place->body + pos;
	dst = src + count;

	aal_memmove(dst, src, hint->len);

	/* Updating the key */
	if (pos == 0) {
		body40_get_key(place, 0, &place->key, NULL);
	}

	place_mkdirty(place);
	return 0;
}

static errno_t tail40_print(place_t *place,
			    aal_stream_t *stream,
			    uint16_t options)
{
	aal_assert("umka-1489", place != NULL);
	aal_assert("umka-1490", stream != NULL);

	aal_stream_format(stream, "TAIL PLUGIN=%s LEN=%u, KEY=[%s]\n",
			  place->plug->label, place->len,
			  core->key_ops.print(&place->key, PO_DEF));
	return 0;
}

errno_t tail40_maxreal_key(place_t *place, 
			   key_entity_t *key) 
{
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

static lookup_res_t tail40_lookup(place_t *place, key_entity_t *key, 
				  lookup_mod_t mode)
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

	if (wanted >= offset &&
	    wanted < offset + units)
	{
		place->pos.unit = wanted - offset;
		return PRESENT;
	}

	place->pos.unit = units;
	return (mode == READ ? ABSENT : PRESENT);
}

#ifndef ENABLE_STAND_ALONE
static int tail40_mergeable(place_t *place1, place_t *place2) {
	aal_assert("umka-2201", place1 != NULL);
	aal_assert("umka-2202", place2 != NULL);

	return body40_mergeable(place1, place2);
}

/* Estimates how many bytes may be shifted into neighbour item */
static errno_t tail40_estimate_shift(place_t *src_place,
				     place_t *dst_place,
				     shift_hint_t *hint)
{
	aal_assert("umka-2279", hint != NULL);
	aal_assert("umka-1664", src_place != NULL);

	if (!(src_place->pos.item == hint->pos.item &&
	      hint->pos.unit != MAX_UINT32))
	{
		goto out_update_hint;
	}
	
	if (hint->control & SF_LEFT) {

		/* Can we update insert point? */
		if (hint->control & SF_UPTIP) {

			/* Correcting @hint->rest. It should contains number of
			   bytes we realy can move out. */
			if (hint->rest > hint->pos.unit)
				hint->rest = hint->pos.unit;

			hint->pos.unit -= hint->rest;

			/* Moving insert point into neighbour item */
			if (hint->pos.unit == 0 && hint->control & SF_MOVIP) {
				hint->result |= SF_MOVIP;

				hint->pos.unit = hint->rest +
					(dst_place ? dst_place->len : 0);
			}
		}
	} else {
		uint32_t right;

		if (hint->control & SF_UPTIP) {
			
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
				    hint->control & SF_MOVIP)
				{
					hint->result |= SF_MOVIP;
					hint->pos.unit = 0;
				}
			} else {
				/* Updating insert point to first position in
				   neighbour item. */
				if (hint->control & SF_MOVIP) {
					hint->result |= SF_MOVIP;
					hint->pos.unit = 0;
				}

				hint->rest = 0;
			}
		}
	}

 out_update_hint:
	hint->units = hint->rest;
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

static errno_t tail40_shift(place_t *src_place,
			    place_t *dst_place,
			    shift_hint_t *hint)
{
	aal_assert("umka-1665", src_place != NULL);
	aal_assert("umka-1666", dst_place != NULL);
	aal_assert("umka-1667", hint != NULL);

	if (hint->control & SF_LEFT) {
		tail40_expand(dst_place, dst_place->len,
			     hint->units, hint->rest);
		
		tail40_copy(dst_place, dst_place->len,
			    src_place, 0, hint->rest);
		
		tail40_shrink(src_place, 0, hint->units,
			      hint->rest);

		/* Updating item's key by the first unit key */
		body40_get_key(src_place, hint->rest,
			       &src_place->key, NULL);
	} else {
		uint32_t pos;
		uint64_t offset;
		
		tail40_expand(dst_place, 0, hint->units,
			      hint->rest);

		pos = src_place->len - hint->units;
		
		tail40_copy(dst_place, 0, src_place, pos, hint->rest);
		tail40_shrink(src_place, pos, hint->units, hint->rest);

		/* Updating item's key by the first unit key */
		body40_get_key(dst_place, 0, &dst_place->key, NULL);

		offset = plug_call(dst_place->key.plug->o.key_ops,
				   get_offset, &dst_place->key);

		aal_assert("umka-2282", offset >= hint->rest);
		
		offset -= hint->rest;

		plug_call(dst_place->key.plug->o.key_ops,
			  set_offset, &dst_place->key, offset);
	}
	
	return 0;
}

static uint64_t tail40_size(place_t *place) {
	aal_assert("vpf-1210", place != NULL);
	return place->len;
}

extern errno_t tail40_merge(place_t *dst, place_t *src, 
			    merge_hint_t *hint);

extern errno_t tail40_estimate_merge(place_t *dst, place_t *src,
				     merge_hint_t *hint);
#endif

static reiser4_item_ops_t tail40_ops = {
	.read	          = tail40_read,
	.units	          = tail40_units,
	.lookup	          = tail40_lookup,
	.get_key          = tail40_get_key,
	.maxposs_key      = tail40_maxposs_key,
#ifndef ENABLE_STAND_ALONE
	.merge	          = tail40_merge,
	.insert	          = tail40_insert,
	.remove	          = tail40_remove,
	.mergeable        = tail40_mergeable,
	.print	          = tail40_print,
	.shift	          = tail40_shift,
	.size             = tail40_size,
	.bytes            = tail40_size,

	.maxreal_key      = tail40_maxreal_key,
	.estimate_merge   = tail40_estimate_merge,
	.estimate_shift   = tail40_estimate_shift,
	.estimate_insert  = tail40_estimate_insert,
	
	.overhead         = NULL,
	.check_struct	  = NULL,
	.init	          = NULL,
	.branch           = NULL,
	.layout	          = NULL,
	.set_key          = NULL,
	.check_layout     = NULL,
#else
	.mergeable        = NULL,
#endif
	.plugid		  = NULL
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
