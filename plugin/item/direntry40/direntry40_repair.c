/*
    direntry40_repair.c -- reiser4 default direntry plugin.
  
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include "direntry40.h"

#ifndef ENABLE_ALONE

#define MIN_LEN			2 /* one symbol and '\0' */
#define de40_min_length(count)	count * (sizeof(entry40_t) + MIN_LEN) + \
				    sizeof(direntry40_t)

static errno_t direntry40_count_check(item_entity_t *item) {
    direntry40_t *de;
    uint16_t count_error, count;

    aal_assert("vpf-268", item != NULL);
    aal_assert("vpf-495", item->body != NULL);
    
    if (!(de = direntry40_body(item)))
	return -1;
    
    count_error = (de->entry[0].offset - sizeof(direntry40_t)) % 
	sizeof(entry40_t);
    
    count = count_error ? ~0u : (de->entry[0].offset - sizeof(direntry40_t)) / 
	sizeof(entry40_t);
    
    if (de40_min_length(de40_get_count(de)) > item->len) {
	if (de40_min_length(count) > item->len) {
	    aal_exception_error("Node %llu, item %u: unit array is not "
		"recognized.", item->con.blk, item->pos);
	    return -1;
	} 
	
	aal_exception_error("Node %llu, item %u: unit count (%u) is "
	    "wrong. Fixed to (%u).", item->con.blk, item->pos,
	    de40_get_count(de), count);
	
	de40_set_count(de, count);
    } else {
	if ((de40_min_length(count) > item->len) || (count != de40_get_count(de))) 
	{
	    aal_exception_error("Node %llu, item %u: wrong offset "
		"(%llu). Fixed to (%llu).", item->con.blk, item->pos,
		de->entry[0].offset, de40_get_count(de) * sizeof(entry40_t) + 
		sizeof(direntry40_t));
	    
	    de->entry[0].offset = de40_get_count(de) * sizeof(entry40_t) + 
		sizeof(direntry40_t);
	}
    }
	
    return 0;
}

static errno_t direntry40_region_delete(rbody_t *body, uint16_t start_pos, 
    uint16_t end_pos) 
{
    int i;
    
    for (i = start_pos; i < end_pos; i++) {
	
    }
    return 0;
}

errno_t direntry40_check(item_entity_t *item) {
    direntry40_t *de;
    uint16_t offset, start_pos = 0;
    int i;

    aal_assert("vpf-267", item != NULL);
    
    if (!(de = direntry40_body(item)))
	return -1;
    
    if (direntry40_count_check(item))
	return -1;
    
    for (i = 1; i <= de40_get_count(de); i++) {
	uint16_t interval = start_pos ? i - start_pos + 1 : 1;

	offset = (i == de40_get_count(de) ? item->len : en40_get_offset(&de->entry[i]));

	/* Check the offset. */
	if ((offset - MIN_LEN * interval < en40_get_offset(&de->entry[i - 1])) || 
	    (offset + MIN_LEN * (de40_get_count(de) - i) > (int)item->len))
	{
	    /* Wrong offset occured. */
	    aal_exception_error("Node %llu, item %u: wrong offset (%llu).",
		item->con.blk, item->pos, offset);
	    if (!start_pos)
		start_pos = i;
	} else if (start_pos) {
	    /* First correct offset after the problem interval. */
	    direntry40_region_delete(de, start_pos, i);
	    de40_set_count(de, de40_get_count(de) - interval);
	    start_pos = 0;
	    i -= interval;
	}
    }

    return 0;
}

#endif
