/*
  tail40.c -- reiser4 default tail plugin.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <aux/aux.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

#define tail40_body(item) (item->body)

/* Returns tail length */
uint32_t tail40_units(item_entity_t *item) {
	return item->len;
}

/* Returns the key of the specified unit */
errno_t tail40_get_key(item_entity_t *item,
		       uint32_t pos, 
		       key_entity_t *key) 
{
	aal_assert("vpf-626", item != NULL);
	aal_assert("vpf-627", key != NULL);
	return body40_get_key(item, pos, key, NULL);
}

static int32_t tail40_read(item_entity_t *item, void *buff,
			   uint32_t pos, uint32_t count)
{
	aal_assert("umka-1673", item != NULL);
	aal_assert("umka-1674", buff != NULL);
	aal_assert("umka-1675", pos < item->len);

#ifndef ENABLE_STAND_ALONE
	if (count > item->len - pos)
		count = item->len - pos;
#endif

	aal_memcpy(buff, item->body + pos, count);
	return count;
}

static int tail40_data(void) {
	return 1;
}

#ifndef ENABLE_STAND_ALONE
/* Rewrites tail from passed @pos by data specifed by hint */
static errno_t tail40_insert(item_entity_t *item,
			     create_hint_t *hint,
			     uint32_t pos)
{
	uint32_t count;
	
	aal_assert("umka-1677", hint != NULL);
	aal_assert("umka-1678", item != NULL);
	aal_assert("umka-1679", pos < item->len);

	count = hint->count;
	
	if (count > item->len - pos)
		count = item->len - pos;
	
	/* Copying new data into place */
	aal_memcpy(item->body + pos,
		   hint->type_specific, count);

	/* Updating the key */
	if (pos == 0) {
		if (tail40_get_key(item, 0, &item->key))
			return -EINVAL;
	}

	return 0;
}

static errno_t tail40_estimate_insert(item_entity_t *item,
				      create_hint_t *hint,
				      uint32_t pos)
{
	aal_assert("umka-1836", hint != NULL);

	if (pos == ~0ul)
		hint->len = hint->count;
	else {
		uint32_t right;

		aal_assert("umka-2284", item != NULL);
		
		right = item->len - pos;

		hint->len = (right >= hint->count ? 0 :
			     hint->count - right);
	}
	
	return 0;
}

/* Removes the part of tail body */
static int32_t tail40_remove(item_entity_t *item, uint32_t pos,
			     uint32_t count)
{
	void *src, *dst;
	
	aal_assert("umka-1661", item != NULL);
	aal_assert("umka-1663", pos < item->len);

	if (pos + count < item->len) {
		src = item->body + pos;
		dst = src + count;

		aal_memmove(dst, src, item->len -
			    (pos + count));
	}

	/* Updating the key */
	if (pos == 0) {
		if (tail40_get_key(item, 0, &item->key))
			return -EINVAL;
	}
	
	return count;
}

static errno_t tail40_print(item_entity_t *item,
			    aal_stream_t *stream,
			    uint16_t options)
{
	aal_assert("umka-1489", item != NULL);
	aal_assert("umka-1490", stream != NULL);

	aal_stream_format(stream, "TAIL PLUGIN=%s LEN=%u, KEY=",
			  item->plugin->h.label, item->len);
		
	if (plugin_call(item->key.plugin->o.key_ops, print,
			&item->key, stream, options))
	{
		return -EINVAL;
	}
	
	aal_stream_format(stream, " UNITS=%u\n", item->len);
	
	return 0;
}

errno_t tail40_maxreal_key(item_entity_t *item, 
			   key_entity_t *key) 
{
	aal_assert("vpf-442", item != NULL);
	aal_assert("vpf-443", key != NULL);

	return body40_maxreal_key(item, key, NULL);
}
#endif

static errno_t tail40_maxposs_key(item_entity_t *item,
				  key_entity_t *key) 
{
	aal_assert("umka-1209", item != NULL);
	aal_assert("umka-1210", key != NULL);

	return body40_maxposs_key(item, key);
}

static lookup_t tail40_lookup(item_entity_t *item,
			      key_entity_t *key, 
			      uint32_t *pos)
{
	lookup_t res;
	uint64_t offset;
	
	aal_assert("umka-1228", item != NULL);
	aal_assert("umka-1229", key != NULL);
	aal_assert("umka-1230", pos != NULL);

	res = body40_lookup(item, key, &offset, NULL);

	*pos = offset;
	return res;
}

#ifndef ENABLE_STAND_ALONE
static int tail40_mergeable(item_entity_t *item1,
			    item_entity_t *item2)
{
	aal_assert("umka-2201", item1 != NULL);
	aal_assert("umka-2202", item2 != NULL);

	return body40_mergeable(item1, item2);
}

/* Estimates how many bytes may be shifted into neighbour item */
static errno_t tail40_estimate_shift(item_entity_t *src_item,
				     item_entity_t *dst_item,
				     shift_hint_t *hint)
{
	aal_assert("umka-2279", hint != NULL);
	aal_assert("umka-1664", src_item != NULL);

	if (!(src_item->pos.item == hint->pos.item &&
	      hint->pos.unit != ~0ul))
	{
		goto out_update_hint;
	}
	
	if (hint->control & SF_LEFT) {

		/* Can we update insert point? */
		if (hint->control & SF_UPTIP) {

			/*
			  Correcting @hint->rest. It should contains number of
			  bytes we realy can move out.
			*/
			if (hint->rest > hint->pos.unit)
				hint->rest = hint->pos.unit;

			hint->pos.unit -= hint->rest;

			/* Moving insert point into neighbour item */
			if (hint->pos.unit == 0 && hint->control & SF_MOVIP) {
				hint->result |= SF_MOVIP;

				hint->pos.unit = hint->rest +
					(dst_item ? dst_item->len : 0);
			}
		}
	} else {
		uint32_t right;

		if (hint->control & SF_UPTIP) {
			
			/* Is insert point inside item? */
			if (hint->pos.unit < src_item->len) {
				right = src_item->len - hint->pos.unit;

				/*
				  Insert point inside item and we can move
				  something.
				*/
				if (hint->rest > right)
					hint->rest = right;

				hint->pos.unit += hint->rest;

				/*
				  Updating insert point to first position in
				  neighbour item.
				*/
				if (hint->pos.unit == src_item->len &&
				    hint->control & SF_MOVIP)
				{
					hint->result |= SF_MOVIP;
					hint->pos.unit = 0;
				}
			} else {
				/*
				  Updating insert point to first position in
				  neighbour item.
				*/
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

errno_t tail40_rep(item_entity_t *dst_item, uint32_t dst_pos,
		   item_entity_t *src_item, uint32_t src_pos,
		   uint32_t count)
{
	aal_assert("umka-2075", dst_item != NULL);
	aal_assert("umka-2076", src_item != NULL);

	if (count > 0) {
		aal_memcpy(dst_item->body + dst_pos,
			   src_item->body + src_pos, count);
	}
	
	return 0;
}

static uint32_t tail40_expand(item_entity_t *item, uint32_t pos,
			      uint32_t count, uint32_t len)
{
	if (pos < item->len) {
		aal_memmove(item->body + pos + count,
			    item->body + pos, item->len - pos);
	}

	return 0;
}

static uint32_t tail40_shrink(item_entity_t *item, uint32_t pos,
			      uint32_t count, uint32_t len)
{
	if (pos < item->len) {
		aal_memmove(item->body + pos,
			    item->body + pos + count,
			    item->len - (pos + count));
	}

	return 0;
}

static errno_t tail40_shift(item_entity_t *src_item,
			    item_entity_t *dst_item,
			    shift_hint_t *hint)
{
	void *src, *dst;
	
	aal_assert("umka-1665", src_item != NULL);
	aal_assert("umka-1666", dst_item != NULL);
	aal_assert("umka-1667", hint != NULL);

	if (hint->control & SF_LEFT) {
		tail40_expand(dst_item, dst_item->len,
			     hint->units, hint->rest);
		
		tail40_rep(dst_item, dst_item->len,
			   src_item, 0, hint->rest);
		
		tail40_shrink(src_item, 0, hint->units,
			      hint->rest);

		/* Updating item's key by the first unit key */
		tail40_get_key(src_item, hint->rest, &src_item->key);
	} else {
		uint32_t pos;
		uint64_t offset;
		
		tail40_expand(dst_item, 0, hint->units,
			      hint->rest);

		pos = src_item->len - hint->units;
		
		tail40_rep(dst_item, 0, src_item, pos, hint->rest);
		tail40_shrink(src_item, pos, hint->units, hint->rest);

		/* Updating item's key by the first unit key */
		tail40_get_key(dst_item, 0, &dst_item->key);

		offset = plugin_call(dst_item->key.plugin->o.key_ops,
				     get_offset, &dst_item->key);

		aal_assert("umka-2282", offset >= hint->rest);
		
		offset -= hint->rest;

		plugin_call(dst_item->key.plugin->o.key_ops,
			    set_offset, &dst_item->key, offset);
	}
	
	return 0;
}

extern errno_t tail40_copy(item_entity_t *dst,
			   uint32_t dst_pos, 
			   item_entity_t *src,
			   uint32_t src_pos, 
			   copy_hint_t *hint);

extern errno_t tail40_estimate_copy(item_entity_t *dst,
				    uint32_t dst_pos,
				    item_entity_t *src,
				    uint32_t src_pos,
				    copy_hint_t *hint);
#endif

static reiser4_item_ops_t tail40_ops = {
#ifndef ENABLE_STAND_ALONE
	.copy	          = tail40_copy,
	.rep	          = tail40_rep,
	.expand	          = tail40_expand,
	.shrink           = tail40_shrink,
	.insert	          = tail40_insert,
	.remove	          = tail40_remove,
	.print	          = tail40_print,
	.shift	          = tail40_shift,
	.maxreal_key      = tail40_maxreal_key,
	
	.estimate_copy    = tail40_estimate_copy,
	.estimate_shift   = tail40_estimate_shift,
	.estimate_insert  = tail40_estimate_insert,
	
	.overhead         = NULL,
	.check	          = NULL,
	.init	          = NULL,
	.branch           = NULL,
	.layout	          = NULL,
	.set_key          = NULL,
	.layout_check     = NULL,
#endif
	.units	          = tail40_units,
	.lookup	          = tail40_lookup,
	.read	          = tail40_read,
	.data		  = tail40_data,

#ifndef ENABLE_STAND_ALONE
	.mergeable        = tail40_mergeable,
#else
	.mergeable        = NULL,
#endif
		
	.get_key          = tail40_get_key,
	.maxposs_key      = tail40_maxposs_key
};

static reiser4_plugin_t tail40_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = ITEM_TAIL40_ID,
		.group = TAIL_ITEM,
		.type = ITEM_PLUGIN_TYPE,
#ifndef ENABLE_STAND_ALONE
		.label = "tail40",
		.desc = "Tail item for reiser4, ver. " VERSION
#endif
	},
	.o = {
		.item_ops = &tail40_ops
	}
};

static reiser4_plugin_t *tail40_start(reiser4_core_t *c) {
	return &tail40_plugin;
}

plugin_register(tail40, tail40_start, NULL);
