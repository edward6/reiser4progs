/*
    direntry40_repair.c -- reiser4 default direntry plugin.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include "direntry40.h"

#define MIN_LEN			2 /* one simbol and '\0' */
#define de40_min_length(count)	count * (sizeof(entry40_t) + MIN_LEN) + \
				    sizeof(direntry40_t)

extern direntry40_t *direntry40_body(reiser4_item_t *item);
    
static errno_t direntry40_count_check(reiser4_item_t *item) {
    direntry40_t *de;
    uint16_t count_error, count, length;

    aal_assert("vpf-268", item != NULL, return -1);
    aal_assert("vpf-268", item->node != NULL, return -1);
    
    if (!(de = direntry40_body(item)))
	return -1;
    
    count_error = (de->entry[0].offset - sizeof(direntry40_t)) % 
	sizeof(entry40_t);
    
    count = count_error ? ~0u : (de->entry[0].offset - sizeof(direntry40_t)) / 
	sizeof(entry40_t);
    
    length = plugin_call(return -1, item->node->plugin->node_ops, item_len, 
	item->node, item->pos);
    
    if (de40_min_length(de40_get_count(de)) > length) {
	if (de40_min_length(count) > length) {
	    aal_exception_error("Node llu, item %d: unit array is not "
		"recognized.", item->pos->item);
	    return -1;
	} 
	
	aal_exception_error("Node llu, item %u, unit %u: unit count (%u) is "
	    "wrong. Fixed to (%u).", item->pos->item, item->pos->unit, 
	    de40_get_count(de), count);
	de40_set_count(de, count);
    } else {
	if ((de40_min_length(count) > length) || (count != de40_get_count(de))) 
	{
	    aal_exception_error("Node llu, item %d, unit 0: wrong offset "
		"(%llu). Fixed to (%llu).", item->pos->item, item->pos->unit, 
		de->entry[0].offset, de40_get_count(de) * sizeof(entry40_t) + 
		sizeof(direntry40_t));
	    de->entry[0].offset = de40_get_count(de) * sizeof(entry40_t) + 
		sizeof(direntry40_t);
	}
    }
	
    return 0;
}

static errno_t direntry40_region_delete(reiser4_body_t *body, uint16_t start_pos, 
    uint16_t end_pos) 
{
    int i;
    
    for (i = start_pos; i < end_pos; i++) {
	
    }
    return 0;
}

errno_t direntry40_check(reiser4_item_t *item, uint16_t options) {
    direntry40_t *de;
    uint16_t offset, start_pos = 0;
    int i;

    aal_assert("vpf-267", item != NULL, return -1);
    
    if (!(de = direntry40_body(item)))
	return -1;
    
    if (direntry40_count_check(item))
	return -1;
    
    for (i = 1; i <= de40_get_count(de); i++) {
	uint16_t interval = start_pos ? i - start_pos + 1 : 1;
	uint16_t length = plugin_call(return -1, item->node->plugin->node_ops, 
	    item_len, item->node, item->pos);

	offset = (i == de40_get_count(de) ? length : en40_get_offset(&de->entry[i]));

	/* Check the offset. */
	if ((offset - MIN_LEN * interval < en40_get_offset(&de->entry[i - 1])) || 
	    (offset + MIN_LEN * (de40_get_count(de) - i) > length))
	{
	    /* Wrong offset occured. */
	    aal_exception_error("Node llu, item %d, unit %d: wrong offset (%llu).",
		item->pos->item, item->pos->unit, offset);
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


