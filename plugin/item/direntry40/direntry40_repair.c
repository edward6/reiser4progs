/*
    direntry40_repair.c -- reiser4 default direntry plugin.
  
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

/* 
    Description:
    1) If offset is obviously wrong (no space for direntry40_t,entry40_t's, 
    objid40_t's) mark it as not relable - NR.
    2) If 2 offsets are not NR and entry contents between them are valid,
    (probably even some offsets between them can be recovered also) mark 
    all offsets beteen them as relable - R.
    3) If pair of offsets does not look ok and left is R, mark right as NR.
    4) If there is no R offset on the left, compare the current with 0-th 
    through the current if not NR.
    5) If there are some R offsets on the left, compare with the most right
    of them through the current if not NR. 
    6) If after 1-5 there is no one R - this is likely not to be direntry40 
    item.
    7) If there is a not NR offset and it has a neighbour which is R and
    its pair with the nearest R offset from the other side if any is ok,
    mark it as R. ?? Disabled. ??
    8) Remove all units with not R offsets.
    9) Remove the space between the last entry40_t and the first R offset.
    10)Remove the space between the end of the last entry and the end of 
    the item. 
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE

#include "direntry40.h"
#include <aux/bitmap.h>
#include <repair/repair_plugin.h>

#define S_NAME		0
#define L_NAME		1

/* short name is all in the key, long is 16 + '\0' */
#define NAME_LEN_MIN(kind)	(kind ? 16 + 1: 0)
#define ENTRY_LEN_MIN(kind) 	(sizeof(objid_t) + NAME_LEN_MIN(kind))
#define UNIT_LEN_MIN(kind)	(sizeof(hash_t) + ENTRY_LEN_MIN(kind))
#define DENTRY_LEN_MIN		(UNIT_LEN_MIN(S_NAME) + sizeof(direntry40_t))

#define de40_len_min(count)	((uint64_t)count * UNIT_LEN_MIN(S_NAME) + \
				sizeof(direntry40_t))

#define OFFSET(de, i)		(en40_get_offset(&de->entry[i]))

#define R(n)			(2 * n + 1)
#define NR(n)			(2 * n)

extern int32_t direntry40_remove(item_entity_t *item, uint32_t pos, uint32_t count);
 
/* Check the i-th offset of the unit body within the item. */
static errno_t direntry40_offset_check(item_entity_t *item, uint32_t pos) {
    direntry40_t *de = direntry40_body(item);
    uint32_t offset, limit;
    
    aal_assert("vpf-752", item->len >= de40_len_min(pos + 1));
    
    offset = OFFSET(de, pos);
    
    /* There must be enough space for the entry in the item. */
    if (offset != item->len - UNIT_LEN_MIN(S_NAME) && 
	offset > item->len - UNIT_LEN_MIN(L_NAME))
	return 1;

    /* There must be enough space for item header, set of unit headers and set 
     * of keys of objects entries point to. */
    return pos != 0 ?
	(offset < sizeof(direntry40_t) + sizeof(entry40_t) * (pos + 1) + 
	    sizeof(objid_t) * pos) :
        (offset - sizeof(direntry40_t)) % (sizeof(entry40_t)) == 0;
	
    /* If item was shorten, left entries should be recovered also. 
     * So this check is excessive as it can avoid such recovering 
     * if too many entry headers are left.
    if (pos == 0) {
	uint32_t count;
	
	count = (offset - sizeof(direntry40_t)) / sizeof(entry40_t);
		
	if (offset + count * ENTRY_LEN_MIN(S_NAME) > item->len)
	    return 1;
	
    }
    */
}

static uint32_t direntry40_count_estimate(item_entity_t *item, uint32_t pos) {
    direntry40_t *de = direntry40_body(item);
    uint32_t offset = OFFSET(de, pos);

    aal_assert("vpf-761", item->len >= de40_len_min(pos + 1));

    return pos == 0 ?
	(offset - sizeof(direntry40_t)) / sizeof(entry40_t) :
        (offset - pos * sizeof(objid_t) - sizeof(direntry40_t)) / 
	    sizeof(entry40_t);
}

/* Check that 2 neighbour offsets look coorect. */
static errno_t direntry40_pair_offsets_check(item_entity_t *item, 
    uint32_t start_pos, uint32_t end_pos) 
{    
    direntry40_t *de = direntry40_body(item);
    uint32_t offset, end_offset;
    uint32_t count = end_pos - start_pos;
    
    aal_assert("vpf-753", start_pos < end_pos);
    aal_assert("vpf-752", item->len >= de40_len_min(end_pos + 1));
    
    end_offset = OFFSET(de, end_pos);
    
    if (end_offset == OFFSET(de, start_pos) + ENTRY_LEN_MIN(S_NAME) * count)
	return 0;
    
    offset = OFFSET(de, start_pos) + ENTRY_LEN_MIN(L_NAME);
    
    return (end_offset < offset + (count - 1) * ENTRY_LEN_MIN(S_NAME));
}

static uint32_t direntry40_name_end(char *body, uint32_t start, uint32_t end) {
    uint32_t i;
    
    aal_assert("vpf-759", start < end);

    for (i = start; i < end; i++) {
	if (body[i] == '\0')
	    break;
    }

    return i;
}

/* Returns amount of entries detected. */
static uint8_t direntry40_short_entry_detect(item_entity_t *item, uint32_t start_pos, 
    uint32_t length, uint8_t mode)
{
    direntry40_t *de = direntry40_body(item);
    uint32_t offset, limit;
    
    limit = OFFSET(de, start_pos);
    
    aal_assert("vpf-770", length < item->len);
    aal_assert("vpf-769", limit <= item->len - length);

    if (length % ENTRY_LEN_MIN(S_NAME))
	return 0;
    
    if (!mode)
	return length / ENTRY_LEN_MIN(S_NAME);
    
    for (offset = ENTRY_LEN_MIN(S_NAME); offset < length; 
	offset += ENTRY_LEN_MIN(S_NAME), start_pos++) 
    {
	aal_exception_error("Node (%llu), item (%u), unit (%u): unit offset "
	    "(%u) is wrong, should be (%u). %s", item->context.blk, item->pos,
	    start_pos, OFFSET(de, start_pos), limit + offset, 
	    mode == REPAIR_REBUILD ? "Fixed." : "");

	if (mode == REPAIR_REBUILD)
	    en40_set_offset(&de->entry[start_pos], limit + offset);
    }

    return length / ENTRY_LEN_MIN(S_NAME);
}

/* Returns amount of entries detected. */
static uint8_t direntry40_long_entry_detect(item_entity_t *item, uint32_t start_pos, 
    uint32_t length, uint8_t mode)
{
    direntry40_t *de = direntry40_body(item);
    uint32_t offset, l_limit, r_limit;
    int count = 0;
    
    aal_assert("vpf-771", length < item->len);
    aal_assert("vpf-772", OFFSET(de, start_pos) <= item->len - length);

    r_limit = OFFSET(de, start_pos) + length;
    l_limit = OFFSET(de, start_pos);
	
    while (l_limit < r_limit) {
	offset = direntry40_name_end(item->body, 
	    l_limit + sizeof(objid_t), r_limit);
	
	if (offset == r_limit)
	    return 0;
	
	if (offset - l_limit < ENTRY_LEN_MIN(L_NAME))
	    return 0;
	
	count++;
	
	if (offset + 1 == l_limit)
	    return count;
	
	if (mode) {
	    aal_exception_error("Node %llu, item %u, unit (%u): unit "
		"offset (%u) is wrong, should be (%u). %s", item->context.blk, 
		item->pos, start_pos + count, OFFSET(de, start_pos + count), 
		l_limit, mode == REPAIR_REBUILD ? "Fixed." : "");
	    
	    if (mode == REPAIR_REBUILD) {
		en40_set_offset(&de->entry[start_pos + count], l_limit);
	    }
	}
	
	l_limit = offset + 1;
    }
    
    return 0;
}

static inline uint8_t direntry40_entry_detect(item_entity_t *item, 
    uint32_t start_pos, uint32_t end_pos, uint8_t mode)
{
    direntry40_t *de = direntry40_body(item);
    uint8_t count;

    count = direntry40_short_entry_detect(item, start_pos, 
	OFFSET(de, end_pos), 0);
    
    if (count == end_pos - start_pos) {
	direntry40_short_entry_detect(item, start_pos, 
	    OFFSET(de, end_pos), mode);
	
	return count;
    }

    count = direntry40_long_entry_detect(item, start_pos, 
	OFFSET(de, end_pos), 0);
    
    if (count == end_pos - start_pos) {
	direntry40_long_entry_detect(item, start_pos, 
	    OFFSET(de, end_pos), mode);
	
	return count;
    }

    return 0;
}

/* Build a bitmap of not R offstes. */
static errno_t direntry40_offsets_range_check(item_entity_t *item, 
    aux_bitmap_t *flags, uint8_t mode) 
{
    direntry40_t *de = direntry40_body(item);
    uint32_t i, j, count, to_compare;
    errno_t res = REPAIR_OK;
    
    aal_assert("vpf-757", flags != NULL);

    count = flags->total / 2;
    to_compare = ~0ul;

    for (i = 0; i < count; i++) {
	/* Check if the offset is valid. */
	if (direntry40_offset_check(item, i)) {
	    aal_exception_error("node %llu, item %u, unit %d: unit offset "
		"(%u) is wrong.", item->context.blk, item->pos, i, OFFSET(de, i));

	    /* mark offset wrong. */
	    aux_bitmap_mark(flags, NR(i));
	    continue;
	}
	
	/* If there was not any R offset, skip pair comparing. */
	if (to_compare == ~0ul) {
	    if ((i == 0) && (direntry40_count_estimate(item, i) == 
		de40_get_units(de)))
	    {
		count = de40_get_units(de);
		aux_bitmap_mark(flags, R(i));
	    }
	    to_compare = i;
	    continue;
	}
	
	for (j = to_compare; j < i; j++) {
	    /* If to_compare is a R element, do just 1 comparing.
	     * Otherwise, compare with all not NR elements. */
	    if (aux_bitmap_test(flags, NR(j)))
		continue;
	    
	    /* Check that a pair of offsets is valid. */
	    if (!direntry40_pair_offsets_check(item, j, i)) {
		/* Pair looks ok. Try to recover it. */
		if (direntry40_entry_detect(item, j, i, mode)) {
		    uint32_t limit;
		    
		    /* Do not compair with elements before the last R. */
		    to_compare = i;
		    
		    /* It is possible to decrease the count when first R found. */
		    limit = direntry40_count_estimate(item, j);

		    if (count > limit)
			count = limit;
		    
		    /* Mark all recovered elements as R. */
		    for (; j <= i; j++)
			aux_bitmap_mark(flags, R(j));

		    /* Problems were detected. */
		    if (i - j - 1) {
			if (mode == REPAIR_REBUILD)
			    res |= REPAIR_FIXED;
			else
			    res |= REPAIR_FATAL;
		    }
		    
		    /*
		    if (info->mode == REPAIR_REBUILD) 
			info->fixed += i - j - 1;
		    else 
			info->fatal += i - j - 1;
		    */

		    break;
		}
		continue;
	    }
	    
	    /* Pair does not look ok, if left is a R element */
	    if (aux_bitmap_test(flags, R(j))) {
		aux_bitmap_mark(flags, NR(i));
		break;
	    }
	}
    }

    /* Mark redundant elements as NR. */
    if (count < flags->total / 2)
	aux_bitmap_mark_region(flags, count * 2, flags->total - count * 2);
    
    return res;
}

static errno_t direntry40_filter(item_entity_t *item, aux_bitmap_t *flags, 
    uint8_t mode) 
{
    direntry40_t *de = direntry40_body(item);
    uint32_t count, e_count, offset, i, last;
    errno_t res = REPAIR_OK;
    
    aal_assert("vpf-757", flags != NULL);

    for (last = flags->total / 2; aux_bitmap_test(flags, NR(last)) || 
	!aux_bitmap_test(flags, R(last)); last--) {}
    
    /* The last offset is correct, but the last entity was not checked yet. */
    offset = direntry40_name_end(item->body, OFFSET(de, last) + sizeof(objid_t),
	item->len);

    /* If the last unit is valid also, count is the last + 1. */
    count = last;
        
    /* Find the first relable. */
    for (i = 0; i < count && !aux_bitmap_test(flags, R(i)); i++) {}

    /* Estimate the amount of units on the base of the first R element. */
    e_count = direntry40_count_estimate(item, i);
    
    /* count estimated for the first relable must be less then count found on
     * the base of the last valid offset. */
    aal_assert("vpf-765", e_count >= count);
    
    if (offset == item->len - 1) {
	count++;
	/* If there is enough space for another entry header, set its offset 
	 * to the item lenght. */
	if (e_count > count && mode == REPAIR_REBUILD)
	    en40_set_offset(&de->entry[count], item->len);
    } 
    
    if (i) {
	/* First offset is not relable. */
	e_count = count;
	if (count == last) 
	    /* Last unit is not valid. */
	    item->len = OFFSET(de, last);

	/* First few units should be deleted - set the first offset just after 
	 * the last unit. */
	if (mode == REPAIR_REBUILD)
	    en40_set_offset(&de->entry[0], sizeof(direntry40_t) + 
		sizeof(entry40_t) * count);
    }
    
    if (e_count != de40_get_units(de)) {
	aal_exception_error("Node %llu, item %u: unit count (%u) is not "
	    "correct. Should be (%u). %s", item->context.blk, item->pos, 
	    de40_get_units(de), e_count, mode == REPAIR_CHECK ? "" : 
	    "Fixed.");
	
	if (mode == REPAIR_CHECK)
	    res |= REPAIR_FIXABLE;
	else {
	    de40_set_units(de, count);
	    res |= REPAIR_FIXED;
	}
    }

    if (i) {
	/* Some first units should be removed. */
	aal_exception_error("Node %llu, item %u: units [%lu..%lu] do not seem "
	    " to be a valid entries. %s", item->context.blk, item->pos, 0, i - 1, 
	    mode == REPAIR_REBUILD ? "Removed." : "");
	
	if (mode == REPAIR_REBUILD) {
	    if (direntry40_remove(item, 0, i) < 0) {
		aal_exception_error("Node %llu, item %u: remove of the unit "
		    "(%u), count (%u) failed.", item->context.blk, item->pos, 0, i);
		return -1;
	    }
	    res |= REPAIR_FIXED;
	} else
	    res |= REPAIR_FATAL;
    } else if (e_count != count) {
	/* Some last units should be removed. */
	aal_exception_error("Node %llu, item %u: units [%lu..%lu] do not seem "
	    " to be a valid entries. %s", item->context.blk, item->pos, count, 
	    e_count - 1, mode == REPAIR_REBUILD ? "Removed." : "");
	
	if (mode == REPAIR_REBUILD) {
	    if (direntry40_remove(item, count, e_count - count) < 0) {
		aal_exception_error("Node %llu, item %u: remove of the unit "
		    "(%u), count (%u) failed.", item->context.blk, item->pos, count, 
		    e_count - count);
		return -1;
	    }
	    res |= REPAIR_FIXED;
	} else
	    res |= REPAIR_FATAL;
    }

    /* First and the last units are ok. Remove all not relable units in the 
     * midle of the item. */
    last = ~0ul;
    for (i = 0; i < de40_get_units(de); i++) {
	if (last == ~0ul) {
	    /* Looking for the problem interval start. */
	    if (!aux_bitmap_test(flags, R(i))) {
		if (mode != REPAIR_REBUILD)
		    return REPAIR_FATAL;
		last = i - 1;
	    }
	} else {
	    /* Looking for the problem interval end. */
	    if (aux_bitmap_test(flags, R(i))) {
		if (direntry40_remove(item, last, i - last) < 0) {
		    aal_exception_error("Node %llu, item %u: remove of the "
			"unit (%u), count (%u) failed.", item->context.blk, 
			item->pos, last, i - last);
		    return -1;
		}
		i = last;
		last = ~0ul;
		res |= REPAIR_FIXED;
	    }
	}
    }

    aal_assert("vpf-766", de40_get_units(de));

    return res;
}

errno_t direntry40_check(item_entity_t *item, uint8_t mode) {
    aux_bitmap_t *flags;
    direntry40_t *de;
    uint32_t count;
    errno_t res = REPAIR_OK;
    int i, j;

    aal_assert("vpf-267", item != NULL);

    if (item->len < de40_len_min(1)) {
	aal_exception_error("Node %llu, item %u: item length (%u) is too "
	    "small to contain a valid item.", item->context.blk, item->pos, 
	    item->len);
	return REPAIR_FATAL;
    }
    
    de = direntry40_body(item);
    
    /* Try to recover even if item was shorten and not all entries exist. */
    count = (item->len - sizeof(direntry40_t)) / (sizeof(entry40_t));
    
    /* map consists of bit pairs - [not relable -R, relable - R] */
    flags = aux_bitmap_create(count * 2);
    
    res |= direntry40_offsets_range_check(item, flags, mode);
    
    if (repair_error_exists(res))
	goto error;
    
    /* FIXME: direntry40_offsets_range_check should shrink bitmap at the 
     * end of the work. Just take total after that. */
    for (; aux_bitmap_test(flags, NR(count)) || 
	!aux_bitmap_test(flags, R(count)); count--) {}
    
    if (++count == 0) {
	/* No one R unit was found */
	aal_exception_error("Node %llu, item %u: no one valid unit has been "
	    "found. Does not look like a valid `%s` item.", item->context.blk, 
	    item->pos, item->plugin->item_ops.h.label);

	aux_bitmap_close(flags);
	return REPAIR_FATAL;
    }
    
    /* Filter units with relable offsets from others. */
    res |= direntry40_filter(item, flags, mode);
    
    aux_bitmap_close(flags);

    return res;
    
error:
    aux_bitmap_close(flags);
    return res;    
}

#if 0
{
    if ((res = direntry40_count_check(item)))
	return res;
    
    for (i = 1; i <= de40_get_units(de); i++) {
	uint16_t interval = start_pos ? i - start_pos + 1 : 1;

	offset = (i == de40_get_units(de) ? item->len : en40_get_offset(&de->entry[i]));

	/* Check the offset. */
	if ((offset - ENTRY_MIN_LEN * interval < en40_get_offset(&de->entry[i - 1])) || 
	    (offset + ENTRY_MIN_LEN * (de40_get_units(de) - i) > (int)item->len))
	{
	    if (info->mode == FSCK_CHECK) {
		aal_exception_error("Node %llu, item %u: wrong offset (%llu).",
		    item->context.blk, item->pos, offset);

	    }
	    
	    /* Wrong offset occured. */
	    aal_exception_error("Node %llu, item %u: wrong offset (%llu).",
		item->context.blk, item->pos, offset);
	    if (!start_pos)
		start_pos = i;
	} else if (start_pos) {
	    /* First correct offset after the problem interval. */
	    direntry40_region_delete(de, start_pos, i);
	    de40_set_count(de, de40_get_units(de) - interval);
	    start_pos = 0;
	    i -= interval;
	}
    }

    return 0;
}
static errno_t direntry40_count_check(item_entity_t *item, 
    repair_plugin_info_t *info) 
{
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
    
    if (de40_min_length(de40_get_units(de)) > item->len) {
	if (de40_min_length(count) > item->len) {
	    info->fatal++;
	    aal_exception_error("Node %llu, item %u: unit array is not "
		"recognized.", item->context.blk, item->pos);
	    return 1;
	} 

	if (info->mode == FSCK_CHECK) {
	    aal_exception_error("Node %llu, item %u: unit count (%u) is "
		"wrong. Should be (%u).", item->context.blk, item->pos,
		de40_get_units(de), count);
	    info->fixable++;
	    return 1;
	} 
	
	aal_exception_error("Node %llu, item %u: unit count (%u) is "
	    "wrong. Fixed to (%u).", item->context.blk, item->pos,
	    de40_get_units(de), count);
	
	de40_set_count(de, count);
    } else {
	if ((de40_min_length(count) > item->len) || (count != de40_get_units(de))) 
	{
	    if (info->mode == FSCK_CHECK) {
		info->fixable++;
		aal_exception_error("Node %llu, item %u: wrong offset "
		    "(%llu). Should be (%llu).", item->context.blk, item->pos,
		    de->entry[0].offset, de40_get_units(de) * sizeof(entry40_t) + 
		    sizeof(direntry40_t));
		return 1;
	    }
	    
	    aal_exception_error("Node %llu, item %u: wrong offset "
		"(%llu). Fixed to (%llu).", item->context.blk, item->pos,
		de->entry[0].offset, de40_get_units(de) * sizeof(entry40_t) + 
		sizeof(direntry40_t));
	    
	    de->entry[0].offset = de40_get_units(de) * sizeof(entry40_t) + 
		sizeof(direntry40_t);
	}
    }
	
    return 0;
}

static errno_t direntry40_bad_range_check(item_entity_t *item, uint32_t start_pos, 
    uint32_t end_pos, repair_plugin_info_t *info) 
{
    direntry40_t *de = direntry40_body(item);
    uint32_t offset, l_limit, r_limit, i;
    
    aal_assert("vpf-756", end > start_pos + 1);
    aal_assert("vpf-760", item->len >= de40_len_min(end_pos + 1));
    
    r_limit = OFFSET(de, end);
    
    if (r_limit == OFFSET(de, start_pos) + ENTRY_LEN_MIN(S_NAME) * count) {
	/* Fix. */
	for (i = 1; i < end - start_pos - 1; i++) {
	    offset = OFFSET(de, start_pos) + ENTRY_LEN_MIN(S_NAME) * i;
	    
	    aal_exception_error("Node (%llu), item (%u), unit (%u): unit offset "
		"(%u) is wrong, should be (%u). %s", item->context.blk, item->pos,
		i, OFFSET(de, start_pos + i), offset, info->mode == REPAIR_REBUILD ? 
		"Fixed." : "");
	    
	    if (info->mode == REPAIR_REBUILD) {
		en40_set_offset(&de->entry[i], offset);
		info->fixed++;
	    } else
		info->fatal++;
	}
	
	return 0;
    }
    
    if (ENTRY_LEN_MIN(L_NAME) * (end - start_pos) + 
	OFFSET(de, start_pos) <= OFFSET(de, end)) 
    {
	bool_t recoverable = 0;
    
    start_again:
	l_limit = OFFSET(de, start_pos) + sizeof(objid_t);
	
	/* Check if it is possible first and if so - fix offsets. */
	for (i = 1; i < end - start; i++) {
	    offset = direntry40_name_end(item->body, l_limit, r_limit);
	    if (offset == r_limit)
		break;
	    
	    if (offset - l_limit <= NAME_LEN_MIN(L_NAME) - 1)
		break;
		
	    if (recoverable) {
		aal_exception_error("Node %llu, item %u, unit (%u): unit "
		    "offset (%u) is wrong, should be (%u). %s", item->context.blk, 
		    item->pos, i, OFFSET(de, start + i), offset, 
		    info->mode == REPAIR_REBUILD ? "Fixed." : "");
	    
		if (info->mode == REPAIR_REBUILD) {
		    en40_set_offset(&de->entry[i], offset);
		    info->fixed++;
		} else
		    info->fatal++;
	    }
	    
	    l_limit = offset + 1 + sizeof(objid_t);
	}

	if (offset != r_limit - 1)
	    return 1;

	if (recoverable)
	    return 0;
	
	recoverable = 1;
	goto start_again;
    }

    return 1;
}


#endif

#endif
