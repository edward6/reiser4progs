/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40_repair.c -- reiser4 default stat data plugin. */

#ifndef ENABLE_MINIMAL
#include "stat40.h"
#include <repair/plugin.h>

typedef struct repair_stat_hint {
	repair_hint_t *repair;
	uint64_t extmask;
	uint64_t goodmask;
	uint64_t len;
} repair_stat_hint_t;

static errno_t cb_check_ext(stat_entity_t *stat, uint64_t extmask, void *data) {
	repair_stat_hint_t *hint = (repair_stat_hint_t *)data;
	uint8_t chunk;
	uint32_t len;
	
	/* Set read extmask. */
	if (!stat->ext_plug) {
		hint->extmask |= ((uint64_t)extmask);
		hint->len += sizeof(stat40_t);
		return 0;
	}

	/* Extention plugin was found, set the bit into @goodmask. */
	hint->goodmask |= ((uint64_t)1 << stat->ext_plug->id.id);

	if ((chunk = stat->ext_plug->id.id / 16) > 0) {
		hint->goodmask |= ((uint64_t)1 << (chunk * 16 - 1));
	}
	
	if (stat->ext_plug->o.sdext_ops->check_struct) {
		errno_t res;
		
		if ((res = plug_call(stat->ext_plug->o.sdext_ops,
				     check_struct, stat, hint->repair)))
			return res;
	}

	len = plug_call(stat->ext_plug->o.sdext_ops, length, stat, NULL);
	hint->len += len;

	/* Some part of the extention was removed. Shrink the item. */
	if (hint->repair->len) {
		uint32_t oldlen = len + hint->repair->len;
		
		aal_memmove(stat_body(stat) + len, stat_body(stat) + oldlen,
			    stat->place->len - stat->offset - oldlen);
		
		place_mkdirty(stat->place);
		hint->repair->len = 0;
	}
	
	return 0;
}

static errno_t cb_fix_mask(stat_entity_t *stat, uint64_t extmask, void *data) {
	uint64_t *mask = (uint64_t *)data;

	if (stat->ext_plug) 
		return 0;

	/* This time the callback is called for the extmask. Fix it. */
	st40_set_extmask(stat_body(stat), *(uint16_t *)mask);

	(*mask) >>= 16;
	return 0;
}

errno_t stat40_check_struct(reiser4_place_t *place, repair_hint_t *hint) {
	repair_stat_hint_t stat;
	errno_t res;
	
	aal_assert("vpf-775", place != NULL);
	
	aal_memset(&stat, 0, sizeof(stat));
	stat.repair = hint;
	
	if ((res = stat40_traverse(place, cb_check_ext, &stat)) < 0)
		return res;
	
	if (res) {
		fsck_mess("Node (%llu), item (%u), [%s]: does "
			  "not look like a valid stat data.", 
			  place_blknr(place), place->pos.item,
			  print_key(stat40_core, &place->key));
		
		return RE_FATAL;
	}
	
	if (stat.len + hint->len < place->len) {
		fsck_mess("Node (%llu), item (%u), [%s]: item has the "
			  "wrong length (%u). Should be (%llu).%s",
			  place_blknr(place), place->pos.item, 
			  print_key(stat40_core, &place->key),
			  place->len, stat.len + hint->len, 
			  hint->mode == RM_BUILD ? " Fixed." : "");
		
		if (hint->mode != RM_BUILD)
			return RE_FATAL;
		
		hint->len = place->len - stat.len;
		place_mkdirty(place);
	}
	
	/* Check the extention mask. */
	if (stat.extmask != stat.goodmask) {
		fsck_mess("Node (%llu), item (%u), [%s]: item has the "
			  "wrong extention mask (%llu). Should be (%llu)."
			  "%s", place_blknr(place), place->pos.item,
			  print_key(stat40_core, &place->key), stat.extmask,
			  stat.goodmask, hint->mode == RM_CHECK ? "" : 
			  " Fixed.");
		
		if (hint->mode == RM_CHECK)
			return RE_FIXABLE;

		if ((res = stat40_traverse(place, cb_fix_mask, 
					   &stat.goodmask)) < 0)
			return res;

		place_mkdirty(place);
	}
	
	return 0;
}


/* Callback for counting stat data extensions in use. */
static errno_t cb_count_ext(stat_entity_t *stat, uint64_t extmask, void *data) {
	if (!stat->ext_plug) 
		return 0;

        (*(uint32_t *)data)++;
        return 0;
}

/* This function returns stat data extension count. */
static uint32_t stat40_sdext_count(reiser4_place_t *place) {
        uint32_t count = 0;

        if (stat40_traverse(place, cb_count_ext, &count) < 0)
                return 0;

        return count;
}

/* Prints extension into @stream. */
static errno_t cb_print_ext(stat_entity_t *stat, uint64_t extmask, void *data) {
	uint16_t length;
	aal_stream_t *stream;

	stream = (aal_stream_t *)data;

	if (!stat->ext_plug) {
		aal_stream_format(stream, "mask:\t\t0x%x\n", extmask);
		return 0;
	}
				
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  stat->ext_plug->label);
	
	aal_stream_format(stream, "offset:\t\t%u\n",
			  stat->offset);
	
	length = plug_call(stat->ext_plug->o.sdext_ops,
			   length, stat, NULL);
	
	aal_stream_format(stream, "len:\t\t%u\n", length);
	
	plug_call(stat->ext_plug->o.sdext_ops, 
		  print, stat, stream, 0);

	return 0;
}

/* Prints statdata item into passed @stream */
void stat40_print(reiser4_place_t *place, aal_stream_t *stream,
		  uint16_t options)
{
	aal_assert("umka-1407", place != NULL);
	aal_assert("umka-1408", stream != NULL);
    
	aal_stream_format(stream, "exts:\t\t%u\n", 
			  stat40_sdext_count(place));
	
	stat40_traverse(place, cb_print_ext, (void *)stream);
}


errno_t stat40_prep_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	reiser4_place_t *src;
	
	aal_assert("vpf-1668", place != NULL);
	aal_assert("vpf-1669", hint != NULL);
	aal_assert("vpf-1670", hint->specific != NULL);
	
	src = (reiser4_place_t *)hint->specific;

	hint->overhead = 0;
	hint->bytes = 0;

	if (place->pos.unit == MAX_UINT32) {
		hint->count = 1;
		hint->len = src->len;
	} else {
		hint->count = hint->len = 0;
	}
	
	return 0;
}

errno_t stat40_insert_raw(reiser4_place_t *place, trans_hint_t *hint) {
	reiser4_place_t *src;
	
	aal_assert("vpf-1671", place != NULL);
	aal_assert("vpf-1672", hint != NULL);
	aal_assert("vpf-1673", hint->specific != NULL);

	if (!hint->len) return 0;
	
	src = (reiser4_place_t *)hint->specific;
	aal_memcpy(place->body, src->body, hint->len);
	place_mkdirty(place);
	
	return 0;

}
#endif
