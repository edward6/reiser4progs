/*
  tail40.c -- reiser4 default tail plugin.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <aux/aux.h>
#include <reiser4/plugin.h>
#include <plugin/item/common40/common40.h>

static reiser4_core_t *core = NULL;

#define tail40_body(item) (item->body)

/* Returns tail length */
static uint32_t tail40_units(item_entity_t *item) {
	return item->len;
}

/* Returns the key of the specified unit */
static errno_t tail40_get_key(item_entity_t *item,
			      uint32_t pos, 
			      key_entity_t *key) 
{
	aal_assert("vpf-626", item != NULL);
	aal_assert("vpf-627", key != NULL);
	aal_assert("vpf-628", pos < tail40_units(item));

	return common40_get_key(item, pos, key, NULL);
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
static errno_t tail40_rep(item_entity_t *dst_item,
			  uint32_t dst_pos,
			  item_entity_t *src_item,
			  uint32_t src_pos,
			  uint32_t count)
{
	aal_assert("umka-2075", dst_item != NULL);
	aal_assert("umka-2076", src_item != NULL);

	aal_memcpy(dst_item->body + dst_pos,
		   src_item->body + src_pos, count);
	
	return 0;
}

static errno_t tail40_feel(item_entity_t *item,
			   key_entity_t *start,
			   key_entity_t *end,
			   feel_hint_t *hint)
{
	uint64_t start_offset, end_offset, offset;
	
	aal_assert("umka-2131", end != NULL);
	aal_assert("umka-1995", item != NULL);
	aal_assert("umka-1996", hint != NULL);
	aal_assert("umka-2132", start != NULL);

	end_offset = plugin_call(end->plugin->o.key_ops,
				 get_offset, end);
	
	start_offset = plugin_call(start->plugin->o.key_ops,
				   get_offset, start);
	
	offset = plugin_call(end->plugin->o.key_ops,
			     get_offset, &item->key);
	
	aal_assert("umka-2130", end_offset > start_offset);
	aal_assert("vpf-942", start_offset >= offset);

	hint->start = start_offset - offset;
	hint->len = end_offset - start_offset + 1;
	hint->count = end_offset - start_offset + 1;
	
	return 0;
}

static errno_t tail40_copy(item_entity_t *dst_item,
			   uint32_t dst_pos,
			   item_entity_t *src_item,
			   uint32_t src_pos,
			   key_entity_t *start,
			   key_entity_t *end,
			   feel_hint_t *hint)
{
	aal_assert("umka-2133", end != NULL);
	aal_assert("umka-2135", hint != NULL);
	aal_assert("umka-2136", start != NULL);
	aal_assert("umka-2134", dst_item != NULL);
	aal_assert("umka-2134", src_item != NULL);

	return tail40_rep(dst_item, dst_pos, src_item,
			  src_pos, hint->count);
}

/* Rewrites tail from passed @pos by data specifed by hint */
static int32_t tail40_write(item_entity_t *item, void *buff,
			    uint32_t pos, uint32_t count)
{
	create_hint_t *hint;
	
	aal_assert("umka-1677", buff != NULL);
	aal_assert("umka-1678", item != NULL);
	aal_assert("umka-1679", pos < item->len);

	hint = (create_hint_t *)buff;
	
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

	return count;
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

/* Removes the part of tail body between specified keys. */
static int32_t tail40_shrink(item_entity_t *item,
			     feel_hint_t *hint)
{
	aal_assert("vpf-930", item  != NULL);
	aal_assert("vpf-931", hint != NULL);
	
	return tail40_remove(item, hint->start, hint->count);
}

static errno_t tail40_init(item_entity_t *item) {
	aal_assert("umka-1668", item != NULL);
	
	aal_memset(item->body, 0, item->len);
	return 0;
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

static errno_t tail40_maxreal_key(item_entity_t *item,
				  key_entity_t *key) 
{
	aal_assert("vpf-442", item != NULL);
	aal_assert("vpf-443", key != NULL);

	return common40_maxreal_key(item, key, NULL);
}
#endif

static errno_t tail40_maxposs_key(item_entity_t *item,
				  key_entity_t *key) 
{
	aal_assert("umka-1209", item != NULL);
	aal_assert("umka-1210", key != NULL);

	return common40_maxposs_key(item, key);
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

	res = common40_lookup(item, key, &offset, NULL);

	*pos = offset;
	return res;
}

#ifndef ENABLE_STAND_ALONE
static int tail40_mergeable(item_entity_t *item1,
			    item_entity_t *item2)
{
	aal_assert("umka-2201", item1 != NULL);
	aal_assert("umka-2202", item2 != NULL);

	return common40_mergeable(item1, item2);
}

/* Estimates how many bytes may be shifted into neighbour item */
static errno_t tail40_predict(item_entity_t *src_item,
			      item_entity_t *dst_item,
			      shift_hint_t *hint)
{
	uint32_t space;
	
	aal_assert("umka-1664", src_item != NULL);
	aal_assert("umka-1690", dst_item != NULL);

	space = hint->rest;
		
	if (hint->control & SF_LEFT) {

		if (hint->rest > hint->pos.unit)
			hint->rest = hint->pos.unit;

		hint->pos.unit -= hint->rest;
		
		if (hint->pos.unit == 0 && hint->control & SF_MOVIP) {
			hint->result |= SF_MOVIP;
			hint->pos.unit = dst_item->len + hint->rest;
		}
	} else {
		uint32_t right;

		if (hint->pos.unit < src_item->len) {
			right = src_item->len - hint->pos.unit;
		
			if (hint->rest > right)
				hint->rest = right;

			hint->pos.unit += hint->rest;
			
			if (hint->pos.unit == src_item->len &&
			    hint->control & SF_MOVIP)
			{
				hint->result |= SF_MOVIP;
				hint->pos.unit = 0;
			}
		} else {
			if (hint->control & SF_MOVIP) {
				hint->result |= SF_MOVIP;
				hint->pos.unit = 0;
			}

			hint->rest = 0;
		}
	}

	return 0;
}

static errno_t tail40_shift(item_entity_t *src_item,
			    item_entity_t *dst_item,
			    shift_hint_t *hint)
{
	uint32_t len;
	void *src, *dst;
	
	aal_assert("umka-1665", src_item != NULL);
	aal_assert("umka-1666", dst_item != NULL);
	aal_assert("umka-1667", hint != NULL);

	len = dst_item->len > hint->rest ? dst_item->len - hint->rest :
		dst_item->len;

	if (hint->control & SF_LEFT) {
		
		/* Copying data from the src tail item to dst one */
		aal_memcpy(dst_item->body + len, src_item->body,
			   hint->rest);

		/* Moving src tail data at the start of tail item body */
		src = src_item->body + hint->rest;
		dst = src - hint->rest;
		
		aal_memmove(dst, src, src_item->len - hint->rest);

		/* Updating item's key by the first unit key */
		if (tail40_get_key(src_item, 0, &src_item->key))
			return -EINVAL;
	} else {
		/* Moving dst tail body into right place */
		src = dst_item->body;
		dst = src + hint->rest;
		
		aal_memmove(dst, src, len);

		/* Copying data from src item to dst one */
		aal_memcpy(dst_item->body, src_item->body +
			   src_item->len, hint->rest);

		/* Updating item's key by the first unit key */
		if (tail40_get_key(dst_item, 0, &dst_item->key))
			return -EINVAL;
	}
	
	return 0;
}
#endif

static reiser4_item_ops_t tail40_ops = {
#ifndef ENABLE_STAND_ALONE
	.init	        = tail40_init,
	.copy	        = tail40_copy,
	.write	        = tail40_write,
	.remove	        = tail40_remove,
	.shrink	        = tail40_shrink,
	.print	        = tail40_print,
	.predict        = tail40_predict,
	.shift	        = tail40_shift,		
	.feel           = tail40_feel,
		
	.maxreal_key    = tail40_maxreal_key,
	.gap_key        = tail40_maxreal_key,
		
	.check	        = NULL,
	.insert         = NULL,
	.estimate       = NULL,
	.set_key        = NULL,
	.layout	        = NULL,
	.layout_check   = NULL,
#endif
	.branch         = NULL,

	.units	        = tail40_units,
	.lookup	        = tail40_lookup,
	.read	        = tail40_read,
	.data		= tail40_data,

#ifndef ENABLE_STAND_ALONE
	.mergeable      = tail40_mergeable,
#else
	.mergeable      = NULL,
#endif
		
	.maxposs_key    = tail40_maxposs_key,
	.get_key        = tail40_get_key
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
	core = c;
	return &tail40_plugin;
}

plugin_register(tail40, tail40_start, NULL);
