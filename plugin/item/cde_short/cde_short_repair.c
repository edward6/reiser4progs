/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde_short_repair.c -- reiser4 default direntry plugin.
   
   Description:
   1) If offset is obviously wrong (no space for cde_short_t,entry_t's, 
   objid40_t's) mark it as not relable - NR.
   2) If 2 offsets are not NR and entry contents between them are valid,
   (probably even some offsets between them can be recovered also) mark 
   all offsets beteen them as relable - R.
   3) If pair of offsets does not look ok and left is R, mark right as NR.
   4) If there is no R offset on the left, compare the current with 0-th 
   through the current if not NR.
   5) If there are some R offsets on the left, compare with the most right
   of them through the current if not NR. 
   6) If after 1-5 there is no one R - this is likely not to be cde_short 
   item.
   7) If there is a not NR offset and it has a neighbour which is R and
   its pair with the nearest R offset from the other side if any is ok,
   mark it as R. ?? Disabled. ??
   8) Remove all units with not R offsets.
   9) Remove the space between the last entry_t and the first R offset.
   10)Remove the space between the end of the last entry and the end of 
   the item. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE
#ifdef ENABLE_SHORT_KEYS

#include "cde_short.h"
#include <aux/bitmap.h>
#include <repair/plugin.h>

#define S_NAME	0
#define L_NAME	1

/* short name is all in the key, long is 16 + '\0' */
#define NAME_LEN_MIN(kind)	(kind ? 16 + 1: 0)
#define ENTRY_LEN_MIN(kind) 	(sizeof(objid_t) + NAME_LEN_MIN(kind))
#define UNIT_LEN_MIN(kind)	(sizeof(entry_t) + ENTRY_LEN_MIN(kind))
#define DENTRY_LEN_MIN		(UNIT_LEN_MIN(S_NAME) + sizeof(cde_short_t))

#define en_len_min(count)	((uint64_t)count * UNIT_LEN_MIN(S_NAME) + \
				sizeof(cde_short_t))

#define OFFSET(de, i)		(en_get_offset(&de->entry[i]))

#define NR	0
#define R	1

struct entry_flags {
	uint8_t *elem;
	uint8_t count;
};
    
/* Extention for repair_flag_t */
#define REPAIR_SKIP	0
    
extern int32_t cde_short_remove(place_t *place, uint32_t pos, 
				uint32_t count);

extern errno_t cde_short_get_key(place_t *place, uint32_t pos, 
				 key_entity_t *key);

extern lookup_t cde_short_lookup(place_t *place, key_entity_t *key,
				 uint32_t *pos);

extern uint32_t cde_short_size_units(place_t *place, uint32_t pos, 
				     uint32_t count);

extern uint32_t cde_short_units(place_t *place);
extern errno_t cde_short_maxposs_key(place_t *place, key_entity_t *key);

extern errno_t cde_short_rep(place_t *dst_place, uint32_t dst_pos,
			     place_t *src_place, uint32_t src_pos,
			     uint32_t count);

extern int32_t cde_short_expand(place_t *place, uint32_t pos,
				uint32_t count, uint32_t len);

/* Check the i-th offset of the unit body within the item. */
static errno_t cde_short_offset_check(place_t *place, uint32_t pos) {
	cde_short_t *de = cde_short_body(place);
	uint32_t offset, limit;
    
	if (place->len < en_len_min(pos + 1))
		return 1;
    
	offset = OFFSET(de, pos);
    
	/* There must be enough space for the entry in the item. */
	if (offset != place->len - ENTRY_LEN_MIN(S_NAME) && 
	    offset != place->len - 2 * ENTRY_LEN_MIN(S_NAME) && 
	    offset > place->len - ENTRY_LEN_MIN(L_NAME))
		return 1;
	
	/* There must be enough space for item header, set of unit headers and 
	   set of keys of objects entries point to. */
	return pos != 0 ?
		(offset < sizeof(cde_short_t) + sizeof(entry_t) * (pos + 1) + 
		 sizeof(objid_t) * pos) :
		(offset - sizeof(cde_short_t)) % (sizeof(entry_t)) != 0;
	
	/* If item was shorten, left entries should be recovered also. 
	   So this check is excessive as it can avoid such recovering 
	   if too many entry headers are left.
	 if (pos == 0) {
	 uint32_t count;
	
	 count = (offset - sizeof(cde_short_t)) / sizeof(entry_t);
		
	 if (offset + count * ENTRY_LEN_MIN(S_NAME) > place->len)
	 return 1;
	
	 }
	*/
}

static uint32_t cde_short_count_estimate(place_t *place, uint32_t pos) {
	cde_short_t *de = cde_short_body(place);
	uint32_t offset = OFFSET(de, pos);
	
	aal_assert("vpf-761", place->len >= en_len_min(pos + 1));
	
	return pos == 0 ?
		(offset - sizeof(cde_short_t)) / sizeof(entry_t) :
		((offset - pos * sizeof(objid_t) - sizeof(cde_short_t)) / 
		 sizeof(entry_t));
}

/* Check that 2 neighbour offsets look coorect. */
static errno_t cde_short_pair_offsets_check(place_t *place, 
					    uint32_t start_pos, 
					    uint32_t end_pos) 
{    
	cde_short_t *de = cde_short_body(place);
	uint32_t offset, end_offset;
	uint32_t count = end_pos - start_pos;
	
	aal_assert("vpf-753", start_pos < end_pos);
	aal_assert("vpf-752", place->len >= en_len_min(end_pos + 1));
	
	end_offset = OFFSET(de, end_pos);
	
	if (end_offset == OFFSET(de, start_pos) + ENTRY_LEN_MIN(S_NAME) * count)
		return 0;
	
	offset = OFFSET(de, start_pos) + ENTRY_LEN_MIN(L_NAME);
	
	return (end_offset < offset + (count - 1) * ENTRY_LEN_MIN(S_NAME));
}

static uint32_t cde_short_name_end(char *body, uint32_t start, uint32_t end) {
	uint32_t i;
	
	aal_assert("vpf-759", start < end);
	
	for (i = start; i < end; i++) {
		if (body[i] == '\0')
			break;
	}
	
	return i;
}

/* Returns amount of entries detected. */
static uint8_t cde_short_short_entry_detect(place_t *place, 
					    uint32_t start_pos, 
					    uint32_t length, 
					    uint8_t mode)
{
	cde_short_t *de = cde_short_body(place);
	uint32_t offset, limit;
	
	limit = OFFSET(de, start_pos);
	
	aal_assert("vpf-770", length < place->len);
	aal_assert("vpf-769", limit <= place->len - length);
	
	if (length % ENTRY_LEN_MIN(S_NAME))
		return 0;
	
	if (mode == REPAIR_SKIP)
		return length / ENTRY_LEN_MIN(S_NAME);
	
	for (offset = ENTRY_LEN_MIN(S_NAME); offset < length; 
	     offset += ENTRY_LEN_MIN(S_NAME), start_pos++) 
	{
		aal_exception_error("Node (%llu), item (%u), unit (%u): unit "
				    "offset (%u) is wrong, should be (%u). %s", 
				    place->con.blk, place->pos.item, start_pos, 
				    OFFSET(de, start_pos), limit + offset, 
				    mode == RM_BUILD ? "Fixed." : "");
		
		if (mode == RM_BUILD)
			en_set_offset(&de->entry[start_pos], limit + offset);
	}
	
	return length / ENTRY_LEN_MIN(S_NAME);
}

/* Returns amount of entries detected. */
static uint8_t cde_short_long_entry_detect(place_t *place, 
					   uint32_t start_pos, 
					   uint32_t length, 
					   uint8_t mode)
{
	cde_short_t *de = cde_short_body(place);
	uint32_t offset, l_limit, r_limit;
	int count = 0;
	
	aal_assert("vpf-771", length < place->len);
	aal_assert("vpf-772", OFFSET(de, start_pos) <= place->len - length);
	
	l_limit = OFFSET(de, start_pos);
	r_limit = l_limit + length;
	
	while (l_limit < r_limit) {
		offset = cde_short_name_end(place->body, 
					    l_limit + sizeof(objid_t), 
					    r_limit);
		
		if (offset == r_limit)
			return 0;
		
		offset++;
		
		if (offset - l_limit < ENTRY_LEN_MIN(L_NAME))
			return 0;
		
		l_limit = offset;
		count++;
		
		if (mode != REPAIR_SKIP && 
		    l_limit != OFFSET(de, start_pos + count)) 
		{
			aal_exception_error("Node %llu, item %u, unit (%u): unit "
					    "offset (%u) is wrong, should be (%u). "
					    "%s", place->con.blk, place->pos.item, 
					    start_pos + count, 
					    OFFSET(de, start_pos+count), l_limit, 
					    mode == RM_BUILD ? "Fixed." : "");
			
			if (mode == RM_BUILD)
				en_set_offset(&de->entry[start_pos + count], 
					      l_limit);
		}
	}
	
	return l_limit == r_limit ? count : 0;
}

static inline uint8_t cde_short_entry_detect(place_t *place, uint32_t start_pos,
					     uint32_t end_pos, uint8_t mode)
{	
	cde_short_t *de = cde_short_body(place);
	uint8_t count;
	
	count = cde_short_short_entry_detect(place, start_pos,
					     OFFSET(de, end_pos) - 
					     OFFSET(de, start_pos), 0);
    
	if (count == end_pos - start_pos) {
		cde_short_short_entry_detect(place, start_pos, 
					     OFFSET(de, end_pos) - 
					     OFFSET(de, start_pos), mode);
		
		return count;
	}
	
	count = cde_short_long_entry_detect(place, start_pos, 
					    OFFSET(de, end_pos) - 
					    OFFSET(de, start_pos), 0);
	
	if (count == end_pos - start_pos) {
		cde_short_long_entry_detect(place, start_pos, 
					    OFFSET(de, end_pos) - 
					    OFFSET(de, start_pos), mode);
		
		return count;
	}
	
	return 0;
}

/* Build a bitmap of not R offstes. */
static errno_t cde_short_offsets_range_check(place_t *place, 
					     struct entry_flags *flags, 
					     uint8_t mode) 
{
	cde_short_t *de = cde_short_body(place);
	uint32_t i, j, to_compare;
	errno_t res = RE_OK;
	
	aal_assert("vpf-757", flags != NULL);
	
	to_compare = MAX_UINT32;
	
	for (i = 0; i < flags->count; i++) {
		/* Check if the offset is valid. */
		if (cde_short_offset_check(place, i)) {
			aal_exception_error("Node %llu, item %u, unit %u: unit "
					    "offset (%u) is wrong.", 
					    place->con.blk, place->pos.item, 
					    i, OFFSET(de, i));
			
			/* mark offset wrong. */	    
			aal_set_bit(flags->elem + i, NR);
			continue;
		}
		
		/* If there was not any R offset, skip pair comparing. */
		if (to_compare == MAX_UINT32) {
			if ((i == 0) && (cde_short_count_estimate(place, i) == 
					 de_get_units(de)))
			{
				flags->count = de_get_units(de);
				aal_set_bit(flags->elem + i, R);
			}
			
			to_compare = i;
			continue;
		}
		
		for (j = to_compare; j < i; j++) {
			/* If to_compare is a R element, do just 1 comparing.
			   Otherwise, compare with all not NR elements. */
			if (aal_test_bit(flags->elem + j, NR))
				continue;
			
			/* Check that a pair of offsets is valid. */
			if (!cde_short_pair_offsets_check(place, j, i)) {
				/* Pair looks ok. Try to recover it. */
				if (cde_short_entry_detect(place, j, i, mode)) {
					uint32_t limit;
					
					/* Do not compair with elements before 
					   the last R. */
					to_compare = i;
					
					/* It's possible to decrease the count 
					   when first R found. */
					limit = cde_short_count_estimate(place, j);
					
					if (flags->count > limit)
						flags->count = limit;
					
					/* If more then 1 item were detected, 
					   some offsets have been recovered, 
					   set result properly. */
					if (i - j > 1) {
						if (mode == RM_BUILD)
							res |= RE_FIXED;
						else
							res |= RE_FATAL;
					}
					
					/* Mark all recovered elements as R. */
					for (j++; j <= i; j++)
						aal_set_bit(flags->elem + j, R);
					
					break;
				}
				
				continue;
			}
			
			/* Pair does not look ok, if left is R offset, this is 
			   NR offset. */
			if (aal_test_bit(flags->elem + j, R)) {
				aal_set_bit(flags->elem + i, NR);
				break;
			}
		}
	}
	
	return res;
}

static errno_t cde_short_filter(place_t *place, struct entry_flags *flags,
				 uint8_t mode)
{
	cde_short_t *de = cde_short_body(place);
	uint32_t e_count, i, last;
	errno_t res = RE_OK;
	
	aal_assert("vpf-757", flags != NULL);
	
	for (last = flags->count; 
	     last && (aal_test_bit(flags->elem + last - 1, NR) || 
		      !aal_test_bit(flags->elem + last - 1, R)); last--) {}
	
	if (last == 0) {
		/* No one R unit was found */
		aal_exception_error("Node %llu, item %u: no one valid unit has "
				    "been found. Does not look like a valid `%s` "
				    "item.", place->con.blk, place->pos.item, 
				    place->plug->label);
		
		return RE_FATAL;
	}
	
	flags->count = --last;
	
	/* Last is the last valid offset. If the last unit is valid also, count 
	   is the last + 1. */
	if (OFFSET(de, last) + sizeof(objid_t) == place->len) {
		flags->count++;
	} else if (OFFSET(de, last) + sizeof(objid_t) < place->len) {
		uint32_t offset;
		
		/* The last offset is correct,but the last entity is not checked yet. */
		offset = cde_short_name_end(place->body, OFFSET(de, last) + 
					     sizeof(objid_t), place->len);
		if (offset == place->len - 1)
			flags->count++;
	}
	
	/* Count is the amount of recovered elements. */
	
	/* Find the first relable. */
	for (i = 0; i < flags->count && !aal_test_bit(flags->elem + i, R); i++) {}
	
	/* Estimate the amount of units on the base of the first R element. */
	e_count = cde_short_count_estimate(place, i);
	
	/* Estimated count must be less then count found on the base of the last 
	 * valid offset. */
	aal_assert("vpf-765", e_count >= flags->count);
	
	/* If there is enough space for another entry header, and the @last entry 
	   is valid also, set @count unit offset to the item length. */
	if (e_count > flags->count && last != flags->count) {
		if (mode == RM_BUILD) {
			en_set_offset(&de->entry[flags->count], place->len);
			res |= RE_FIXED;
		} else {
			res |= RE_FATAL;
		}
	}
 	
	if (flags->count == last && mode == RM_BUILD) {
		/* Last unit is not valid. */
		if (mode == RM_BUILD) {
			place->len = OFFSET(de, last);
			res |= RE_FIXED;
		} else {
			res |= RE_FATAL;
		}
	}
	
	if (i) {
		/* Some first offset are not relable. Consider count as the 
		   correct count and set the first offset just after the last 
		   unit.*/
		e_count = flags->count;
		
		if (mode == RM_BUILD) {	    
			en_set_offset(&de->entry[0], sizeof(cde_short_t) + 
					sizeof(entry_t) * flags->count);
			res |= RE_FIXED;
		}
	}
	
	if (e_count != de_get_units(de)) {
		aal_exception_error("Node %llu, item %u: unit count (%u) is not "
				    "correct. Should be (%u). %s", place->con.blk, 
				    place->pos.item, de_get_units(de), e_count, 
				    mode == RM_CHECK ? "" : "Fixed.");
		
		if (mode == RM_CHECK) {
			res |= RE_FIXABLE;
		} else {
			de_set_units(de, e_count);
			res |= RE_FIXED;
		}
	}
	
	if (flags->count != e_count) {
		/* Estimated count is greater then the recovered count, in other 
		   words there are some last unit headers should be removed. */
		aal_exception_error("Node %llu, item %u: entries [%u..%u] look "
				    "corrupted. %s", place->con.blk, place->pos.item, 
				    flags->count, e_count - 1, 
				    mode == RM_BUILD ? "Removed." : "");
		
		if (mode == RM_BUILD) {
			if (cde_short_remove(place, flags->count, 
					     e_count - flags->count) < 0) 
			{
				aal_exception_error("Node %llu, item %u: remove "
						    "of the unit (%u), count (%u) "
						    "failed.", place->con.blk, 
						    place->pos.item, flags->count, 
						    e_count - flags->count);
				return -EINVAL;
			}
			
			res |= RE_FIXED;
		} else {
			res |= RE_FATAL;
		}
	}
	
	if (i) {
		/* Some first units should be removed. */
		aal_exception_error("Node %llu, item %u: entries [%u..%u] look "
				    " corrupted. %s", place->con.blk, 
				    place->pos.item, 0, i - 1, 
				    mode == RM_BUILD ? "Removed." : "");
		
		if (mode == RM_BUILD) {
			if (cde_short_remove(place, 0, i) < 0) {
				aal_exception_error("Node %llu, item %u: remove of "
						    "the unit (%u), count (%u) "
						    "failed.", place->con.blk,
						    place->pos.item, 0, i);
				return -EINVAL;
			}
			
			res |= RE_FIXED;
			aal_memmove(flags->elem, flags->elem + i, 
				    flags->count - i);
			
			flags->count -= i;
			i = 0;
		} else {
			res |= RE_FATAL;
		}
	}
	
	/* Units before @i and after @count were handled, do not care about them 
	   anymore. Handle all not relable units between them. */
	last = MAX_UINT32;
	for (; i < flags->count; i++) {
		if (last == MAX_UINT32) {
			/* Looking for the problem interval start. */
			if (!aal_test_bit(flags->elem + i, R))
				last = i - 1;
		} else {
			/* Looking for the problem interval end. */
			if (aal_test_bit(flags->elem + i, R)) {
				aal_exception_error("Node %llu, item %u: entries "
						    "[%u..%u] look corrupted. %s", 
						    place->con.blk, 
						    place->pos.item,
						    last, i - 1, 
						    mode == RM_BUILD ? 
						    "Removed." : "");
				
				if (mode == RM_BUILD) {
					if (cde_short_remove(place, last, 
							     i - last) < 0) 
					{
						aal_exception_error("Node %llu, "
								    "item %u: "
								    "remove of "
								    "the unit "
								    "(%u), count "
								    "(%u) failed.", 
								    place->con.blk, 
								    place->pos.item, 
								    last, i - last);
						return -EINVAL;
					}
					
					aal_memmove(flags->elem + last,
						    flags->elem + i,
						    flags->count - i);
					
					flags->count -= (i - last);
					
					i = last;
					
					res |= RE_FIXED;
				} else {
					res |= RE_FATAL;
				}
				
				last = MAX_UINT32;
			}
		}
	}
	
	aal_assert("vpf-766", de_get_units(de));
	
	return res;
}

errno_t cde_short_check_struct(place_t *place, uint8_t mode) {
	struct entry_flags flags;
	cde_short_t *de;
	errno_t res = RE_OK;
	int i, j;
	
	aal_assert("vpf-267", place != NULL);
	
	if (place->len < en_len_min(1)) {
		aal_exception_error("Node %llu, item %u: item length (%u) is too "
				    "small to contain a valid item.", 
				    place->con.blk, place->pos.item, place->len);
		return RE_FATAL;
	}
	
	de = cde_short_body(place);
	
	/* Try to recover even if item was shorten and not all entries exist. */
	flags.count = (place->len - sizeof(cde_short_t)) / (sizeof(entry_t));
	
	/* map consists of bit pairs - [not relable -R, relable - R] */
	flags.elem = aal_calloc(flags.count, 0);
	
	res |= cde_short_offsets_range_check(place, &flags, mode);
	
	if (repair_error_exists(res))
		goto error;
	
	/* Filter units with relable offsets from others. */
	res |= cde_short_filter(place, &flags, mode);
	
 error:
	aal_free(flags.elem);
	
	return res;
}

errno_t cde_short_estimate_copy(place_t *dst, uint32_t dst_pos,
				 place_t *src, uint32_t src_pos, 
				 copy_hint_t *hint)
{
	uint32_t units, next_pos, pos;
	key_entity_t dst_key;
	lookup_t lookup;
	
	aal_assert("vpf-957", dst  != NULL);
	aal_assert("vpf-958", src  != NULL);
	aal_assert("vpf-959", hint != NULL);
	
	units = cde_short_units(src);
	
	lookup = cde_short_lookup(src, &hint->end, &pos);
	if (lookup == FAILED)
		return -EINVAL;
	
	cde_short_get_key(dst, dst_pos, &dst_key);
	cde_short_lookup(src, &dst_key, &next_pos);
	
	if (pos < next_pos)
		next_pos = pos;
	
	aal_assert("vpf-1015", next_pos >= src_pos);
	
	/* FIXME-VITALY: Key collisions are not supported yet. */
	
	hint->src_count = next_pos - src_pos;
	hint->dst_count = 0;
	hint->len_delta = (sizeof(entry_t) * hint->src_count) +
		cde_short_size_units(src, src_pos, hint->src_count);
	
	while (next_pos < units) {
		cde_short_get_key(src, next_pos, &hint->end);
		lookup = cde_short_lookup(dst, &hint->end, &pos);
		
		if (lookup == FAILED)
			return -EINVAL;
		
		if (lookup == ABSENT)
			return 0;
		
		next_pos++;
	}
	
	cde_short_maxposs_key(src, &hint->end);
	
	return 0;
}

errno_t cde_short_copy(place_t *dst, uint32_t dst_pos, 
			place_t *src, uint32_t src_pos, 
			copy_hint_t *hint)
{
	aal_assert("vpf-1014", dst != NULL);
	aal_assert("vpf-1013", src != NULL);
	aal_assert("vpf-1012", hint != NULL);
	aal_assert("vpf-1011", hint->dst_count == 0);
	
	/* Preparing root for copying units into it */
	cde_short_expand(dst, dst_pos, hint->src_count, hint->len_delta);
	
	return cde_short_rep(dst, dst_pos, src, src_pos, hint->src_count);
}

#if 0
{
	if ((res = cde_short_count_check(place)))
		return res;
    
	for (i = 1; i <= de_get_units(de); i++) {
		uint16_t interval = start_pos ? i - start_pos + 1 : 1;

		offset = (i == de_get_units(de) ? place->len : 
			  en_get_offset(&de->entry[i]));

		/* Check the offset. */
		if ((offset - ENTRY_MIN_LEN * interval < 
		     en_get_offset(&de->entry[i - 1])) || 
		    (offset + ENTRY_MIN_LEN * (de_get_units(de) - i) > 
		     (int)place->len))
			{
				if (info->mode == FSCK_CHECK) {
					aal_exception_error("Node %llu, item %u: "
							    "wrong offset (%llu).",
							    place->con.blk, 
							    place->pos.item, offset);

				}
	    
				/* Wrong offset occured. */
				aal_exception_error("Node %llu, item %u: wrong offset "
						    "(%llu).", place->con.blk, 
						    place->pos.item, offset);
				if (!start_pos)
					start_pos = i;
			} else if (start_pos) {
				/* First correct offset after the problem interval. */
				cde_short_region_delete(de, start_pos, i);
				en_set_count(de, de_get_units(de) - interval);
				start_pos = 0;
				i -= interval;
			}
	}
	
	return 0;
}

static errno_t cde_short_count_check(place_t *place, 
				     repair_plugin_info_t *info) 
{
	cde_short_t *de;
	uint16_t count_error, count;

	aal_assert("vpf-268", place != NULL);
	aal_assert("vpf-495", place->body != NULL);
    
	de = cde_short_body(place);
    
	count_error = (de->entry[0].offset - sizeof(cde_short_t)) % 
		sizeof(entry_t);
    
	count = count_error ? MAX_UINT16 : (de->entry[0].offset - sizeof(cde_short_t)) / 
		sizeof(entry_t);
    
	if (en_min_length(de_get_units(de)) > place->len) {
		if (en_min_length(count) > place->len) {
			info->fatal++;
			aal_exception_error("Node %llu, item %u: unit array is "
					    "not recognized.", place->con.blk, 
					    place->pos.item);
			return 1;
		}

		if (info->mode == FSCK_CHECK) {
			aal_exception_error("Node %llu, item %u: unit count (%u) "
					    "is wrong. Should be (%u).", 
					    place->context.blk, place->pos.item,
					    de_get_units(de), count);
			info->fixable++;
			return 1;
		}
		
		aal_exception_error("Node %llu, item %u: unit count (%u) is "
				    "wrong. Fixed to (%u).", place->con.blk, 
				    place->pos.item, de_get_units(de), count);
		
		en_set_count(de, count);
	} else {
		if ((en_min_length(count) > place->len) || 
		    (count != de_get_units(de))) 
		{
			if (info->mode == FSCK_CHECK) {
				info->fixable++;
				aal_exception_error("Node %llu, item %u: wrong offset "
						    "(%llu). Should be (%llu).", 
						    place->context.blk, place->pos.item,
						    de->entry[0].offset, 
						    de_get_units(de) * 
						    sizeof(entry_t) + 
						    sizeof(cde_short_t));
				return 1;
			}
			
			aal_exception_error("Node %llu, item %u: wrong offset "
					    "(%llu). Fixed to (%llu).", 
					    place->context.blk, place->pos.item,
					    de->entry[0].offset, 
					    de_get_units(de) * sizeof(entry_t) + 
					    sizeof(cde_short_t));
			
			de->entry[0].offset = de_get_units(de) * sizeof(entry_t) + 
				sizeof(cde_short_t);
		}
	}
	
	return 0;
}

static errno_t cde_short_bad_range_check(place_t *place, 
					 uint32_t start_pos, 
					 uint32_t end_pos, 
					 repair_plugin_info_t *info) 
{
	cde_short_t *de = cde_short_body(place);
	uint32_t offset, l_limit, r_limit, i;
	
	aal_assert("vpf-756", end > start_pos + 1);
	aal_assert("vpf-760", place->len >= en_len_min(end_pos + 1));
	
	r_limit = OFFSET(de, end);
	
	if (r_limit == OFFSET(de, start_pos) + ENTRY_LEN_MIN(S_NAME) * count) {
		/* Fix. */
		for (i = 1; i < end - start_pos - 1; i++) {
			offset = OFFSET(de, start_pos) + ENTRY_LEN_MIN(S_NAME) * i;
	    
			aal_exception_error("Node (%llu), item (%u), unit (%u): "
					    "unit offset (%u) is wrong, should be "
					    "(%u). %s", place->context.blk, place->pos.item,
					    i, OFFSET(de, start_pos + i), offset, 
					    info->mode == RM_BUILD ? 
					    "Fixed." : "");
			
			if (info->mode == RM_BUILD) {
				en_set_offset(&de->entry[i], offset);
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
			offset = cde_short_name_end(place->body, l_limit, r_limit);
			
			if (offset == r_limit)
				break;
			
			if (offset - l_limit <= NAME_LEN_MIN(L_NAME) - 1)
				break;
			
			if (recoverable) {
				aal_exception_error("Node %llu, item %u, unit (%u): "
						    "unit offset (%u) is wrong, "
						    "should be (%u). %s", 
						    place->context.blk, 
						    place->pos.item, i, 
						    OFFSET(de, start + i), 
						    offset, 
						    info->mode == RM_BUILD ? 
						    "Fixed." : "");
				
				if (info->mode == RM_BUILD) {
					en_set_offset(&de->entry[i], offset);
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

#endif
