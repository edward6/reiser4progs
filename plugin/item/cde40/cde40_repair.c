/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde40_repair.c -- reiser4 default direntry plugin.
   
   Description:
   1) If offset is obviously wrong (no space for cde40_t,entry_t's, 
   objid40_t's) mark it as not relable - NR.
   2) If 2 offsets are not NR and entry contents between them are valid,
   (probably even some offsets between them can be recovered also) mark 
   all offsets beteen them as relable - R.
   3) If pair of offsets does not look ok and left is R, mark right as NR.
   4) If there is no R offset on the left, compare the current with 0-th 
   through the current if not NR.
   5) If there are some R offsets on the left, compare with the most right
   of them through the current if not NR. 
   6) If after 1-5 there is no one R - this is likely not to be cde40
   item.
   7) If there is a not NR offset and it has a neighbour which is R and
   its pair with the nearest R offset from the other side if any is ok,
   mark it as R. ?? Disabled. ??
   8) Remove all units with not R offsets.
   9) Remove the space between the last entry_t and the first R offset.
   10)Remove the space between the end of the last entry and the end of 
   the item. */

#ifndef ENABLE_STAND_ALONE

#include "cde40.h"
#include <aux/bitmap.h>
#include <repair/plugin.h>

#define S_NAME	0
#define L_NAME	1

/* short name is all in the key, long is 16 + '\0' */
#define NAME_LEN_MIN(kind)		\
        (kind ? 16 + 1: 0)

#define ENTRY_LEN_MIN(kind, pol)	\
        (ob_size(pol) + NAME_LEN_MIN(kind))

#define UNIT_LEN_MIN(kind, pol)		\
        (en_size(pol) + ENTRY_LEN_MIN(kind, pol))

#define DENTRY_LEN_MIN			\
        (UNIT_LEN_MIN(S_NAME) + sizeof(cde40_t))

#define en_len_min(count, pol)		\
        ((uint64_t)count * UNIT_LEN_MIN(S_NAME, pol) + sizeof(cde40_t))

#define NR	0
#define R	1

struct entry_flags {
	uint8_t *elem;
	uint8_t count;
};
    
/* Extention for repair_flag_t */
#define REPAIR_SKIP	0

/* Check the i-th offset of the unit body within the item. */
static errno_t cde40_offset_check(reiser4_place_t *place, uint32_t pos) {
	uint32_t pol;
	uint32_t offset;
    
	pol = cde40_key_pol(place);
	
	if (place->len < en_len_min(pos + 1, pol))
		return 1;

	offset = cde_get_offset(place, pos, pol);
    
	/* There must be enough space for the entry in the item. */
	if (offset != place->len - ENTRY_LEN_MIN(S_NAME, pol) && 
	    offset != place->len - 2 * ENTRY_LEN_MIN(S_NAME, pol) && 
	    offset > place->len - ENTRY_LEN_MIN(L_NAME, pol))
	{
		return 1;
	}
	
	/* There must be enough space for item header, set of unit headers and 
	   set of keys of objects entries point to. */
	return pos != 0 ?
		(offset < sizeof(cde40_t) + en_size(pol) * (pos + 1) + 
		 ob_size(pol) * pos) :
		(offset - sizeof(cde40_t)) % (en_size(pol)) != 0;
	
	/* If item was shorten, left entries should be recovered also. 
	   So this check is excessive as it can avoid such recovering 
	   if too many entry headers are left.
	 if (pos == 0) {
	 uint32_t count;
	
	 count = (offset - sizeof(cde40_t)) / en_size(pol);
		
	 if (offset + count * ENTRY_LEN_MIN(S_NAME, pol) > place->len)
	 return 1;
	
	 }
	*/
}

static uint32_t cde40_count_estimate(reiser4_place_t *place, uint32_t pos) {
	uint32_t pol = cde40_key_pol(place);
	uint32_t offset = cde_get_offset(place, pos, pol);
	
	aal_assert("vpf-761", place->len >= en_len_min(pos + 1, pol));
	
	return pos == 0 ?
		(offset - sizeof(cde40_t)) / en_size(pol) :
		((offset - pos * ob_size(pol) - sizeof(cde40_t)) / 
		 en_size(pol));
}

/* Check that 2 neighbour offsets look coorect. */
static errno_t cde40_pair_offsets_check(reiser4_place_t *place, 
					uint32_t start_pos, 
					uint32_t end_pos) 
{    
	uint32_t pol;
	uint32_t offset, end_offset;
	uint32_t count = end_pos - start_pos;
	
	pol = cde40_key_pol(place);
	
	aal_assert("vpf-753", start_pos < end_pos);
	aal_assert("vpf-752", place->len >= en_len_min(end_pos + 1, pol));

	end_offset = cde_get_offset(place, end_pos, pol);
	
	if (end_offset == cde_get_offset(place, start_pos, pol) +
	    ENTRY_LEN_MIN(S_NAME, pol) * count)
	{
		return 0;
	}
	
	offset = cde_get_offset(place, start_pos, pol) +
		ENTRY_LEN_MIN(L_NAME, pol);
	
	return (end_offset < offset + (count - 1) * ENTRY_LEN_MIN(S_NAME, pol));
}

static uint32_t cde40_name_len(char *body, uint32_t start, uint32_t end) {
	uint32_t i;
	
	aal_assert("vpf-759", start < end);
	
	for (i = start; i < end; i++) {
		if (body[i] == '\0')
			break;
	}
	
	return i - start;
}

/* Returns amount of entries detected. */
static uint8_t cde40_short_entry_detect(reiser4_place_t *place, 
					uint32_t start_pos, 
					uint32_t length, 
					uint8_t mode)
{
	uint32_t pol;
	uint32_t offset, limit;

	pol = cde40_key_pol(place);
	limit = cde_get_offset(place, start_pos, pol);
	
	aal_assert("vpf-770", length < place->len);
	aal_assert("vpf-769", limit <= place->len - length);
	
	if (length % ENTRY_LEN_MIN(S_NAME, pol))
		return 0;
	
	if (mode == REPAIR_SKIP)
		return length / ENTRY_LEN_MIN(S_NAME, pol);
	
	for (offset = ENTRY_LEN_MIN(S_NAME, pol); offset < length; 
	     offset += ENTRY_LEN_MIN(S_NAME, pol), start_pos++) 
	{
		aal_error("Node (%llu), item (%u), unit (%u): unit "
			  "offset (%u) is wrong, should be (%u). %s", 
			  place->node->block->nr, place->pos.item,
			  start_pos, cde_get_offset(place, start_pos, pol),
			  limit + offset,  mode == RM_BUILD ? "Fixed." : "");
		
		if (mode == RM_BUILD)
			cde_set_offset(place, start_pos, limit + offset, pol);
	}
	
	return length / ENTRY_LEN_MIN(S_NAME, pol);
}

/* Returns amount of entries detected. */
static uint8_t cde40_long_entry_detect(reiser4_place_t *place, 
				       uint32_t start_pos, 
				       uint32_t length, 
				       uint8_t mode)
{
	uint32_t pol;
	int count = 0;
	uint32_t offset;
	uint32_t l_limit;
	uint32_t r_limit;

	pol = cde40_key_pol(place);
	
	aal_assert("umka-2405", place != NULL);
	aal_assert("vpf-771", length < place->len);
	
	aal_assert("vpf-772", cde_get_offset(place, start_pos, pol)
		   <= (place->len - length));
	
	l_limit = cde_get_offset(place, start_pos, pol);
	r_limit = l_limit + length;
	
	while (l_limit + ob_size(pol) < r_limit) {
		offset = l_limit + ob_size(pol) +
			cde40_name_len(place->body, l_limit +
				       ob_size(pol), r_limit);
		
		if (offset == r_limit)
			return 0;
		
		offset++;
		
		if (offset - l_limit < ENTRY_LEN_MIN(L_NAME, pol))
			return 0;
		
		l_limit = offset;
		count++;
		
		if (mode != REPAIR_SKIP && 
		    l_limit != cde_get_offset(place, start_pos + count, pol)) 
		{
			aal_error("Node %llu, item %u, unit (%u): unit "
				  "offset (%u) is wrong, should be (%u). "
				  "%s", place->node->block->nr, place->pos.item,
				  start_pos + count,
				  cde_get_offset(place, start_pos + count, pol),
				  l_limit, mode == RM_BUILD ? "Fixed." : "");
			
			if (mode == RM_BUILD) {
				cde_set_offset(place, start_pos + count, 
					       l_limit, pol);
			}
		}
	}
	
	return l_limit == r_limit ? count : 0;
}

static inline uint8_t cde40_entry_detect(reiser4_place_t *place, uint32_t start_pos,
					 uint32_t end_pos, uint8_t mode)
{
	uint32_t pol;
	uint8_t count;

	pol = cde40_key_pol(place);
	count = cde40_short_entry_detect(place, start_pos,
					 cde_get_offset(place, end_pos, pol) - 
					 cde_get_offset(place, start_pos, pol), 
					 0);
    
	if (count == end_pos - start_pos) {
		cde40_short_entry_detect(place, start_pos, 
					 cde_get_offset(place, end_pos, pol) - 
					 cde_get_offset(place, start_pos, pol), 
					 mode);
		
		return count;
	}
	
	count = cde40_long_entry_detect(place, start_pos, 
					cde_get_offset(place, end_pos, pol) - 
					cde_get_offset(place, start_pos, pol), 
					0);
	
	if (count == end_pos - start_pos) {
		cde40_long_entry_detect(place, start_pos, 
					cde_get_offset(place, end_pos, pol) - 
					cde_get_offset(place, start_pos, pol), 
					mode);
		
		return count;
	}
	
	return 0;
}

/* Build a bitmap of not R offstes. */
static errno_t cde40_offsets_range_check(reiser4_place_t *place, 
					 struct entry_flags *flags, 
					 uint8_t mode) 
{
	uint32_t pol;
	uint32_t i, j;
	errno_t res = 0;
	uint32_t to_compare;
	
	aal_assert("vpf-757", flags != NULL);

	pol = cde40_key_pol(place);
	to_compare = MAX_UINT32;
	
	for (i = 0; i < flags->count; i++) {
		/* Check if the offset is valid. */
		if (cde40_offset_check(place, i)) {
			aal_error("Node %llu, item %u, unit %u: unit "
				  "offset (%u) is wrong.", 
				  place->node->block->nr, place->pos.item, 
				  i, cde_get_offset(place, i, pol));
			
			/* mark offset wrong. */	    
			aal_set_bit(flags->elem + i, NR);
			continue;
		}
		
		/* If there was not any R offset, skip pair comparing. */
		if (to_compare == MAX_UINT32) {
			if ((i == 0) && (cde40_count_estimate(place, i) == 
					 cde_get_units(place)))
			{
				flags->count = cde_get_units(place);
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
			if (!cde40_pair_offsets_check(place, j, i)) {
				/* Pair looks ok. Try to recover it. */
				if (cde40_entry_detect(place, j, i, mode)) {
					uint32_t limit;
					
					/* Do not compair with elements before 
					   the last R. */
					to_compare = i;
					
					/* It's possible to decrease the count 
					   when first R found. */
					limit = cde40_count_estimate(place, j);
					
					if (flags->count > limit)
						flags->count = limit;
					
					/* If more then 1 item were detected, 
					   some offsets have been recovered, 
					   set result properly. */
					if (i - j > 1) {
						if (mode == RM_BUILD)
							place_mkdirty(place);
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

static errno_t cde40_filter(reiser4_place_t *place, struct entry_flags *flags,
			    uint8_t mode)
{
	uint32_t i, last;
	uint32_t e_count;
	uint32_t offset;
	errno_t res = 0;
	uint32_t pol;
	
	aal_assert("vpf-757", flags != NULL);

	pol = cde40_key_pol(place);
	
	/* Get the last correct unit + 1 into @last. */
	for (last = flags->count; 
	     last && (aal_test_bit(flags->elem + last - 1, NR) || 
		      !aal_test_bit(flags->elem + last - 1, R)); last--) {}
	
	if (last == 0) {
		/* No one R unit was found */
		aal_error("Node %llu, item %u: no one valid unit has "
			  "been found. Does not look like a valid `%s` "
			  "item.", place->node->block->nr, place->pos.item, 
			  place->plug->label);
		
		return RE_FATAL;
	}
	
	/* the last unit size is not checked yet, so save - 1 into @count. */
	flags->count = --last;
	
	offset = cde_get_offset(place, last, pol) + ob_size(pol);
	
	/* Last is the last valid offset. If the last unit is valid also, count
	   is the last + 1. */
	if (offset == place->len) {
		flags->count++;
	} else if (offset < place->len) {
		/* The last offset is correct, but the entity is not checked 
		   yet. */
		offset += aal_strlen(place->body + offset) + 1;

		/* If nothing to be removed, the last unit is relable. */
		if (offset == place->len) 
			flags->count++;
	} else {
		aal_bug("vpf-1560", "Amount of detected units exceeds the "
			"item length.");
	}
	
	/* Count is the amount of recovered elements. */
	
	/* Find the first relable. */
	for (i = 0; i < flags->count && !aal_test_bit(flags->elem + i, R); i++) {}
	
	/* Estimate the amount of units on the base of the first R element. */
	e_count = cde40_count_estimate(place, i);
	
	/* Estimated count must be less then count found on the base of the last 
	 * valid offset. */
	aal_assert("vpf-765", e_count >= flags->count);
	
	if (last != flags->count && e_count > flags->count && mode == RM_BUILD) {
		/* If there is enough space for another entry header, set 
		   @count unit offset to the item length to remove item 
		   headers correctly later. */
		cde_set_offset(place, flags->count, place->len, pol);
	} 
	
	if (i) {
		/* Some first offset are not relable. Consider count as 
		   the correct count and set the first offset just after 
		   the last unit.*/
		if (mode == RM_BUILD) {
			cde_set_offset(place, 0, sizeof(cde40_t) + 
				   en_size(pol) * flags->count, pol);
			place_mkdirty(place);
		}
	}
	
	if (e_count != cde_get_units(place)) {
		aal_error("Node %llu, item %u: unit count (%u) "
			  "is not correct. Should be (%u). %s",
			  place->node->block->nr,  place->pos.item,
			  cde_get_units(place), e_count, 
			  mode == RM_CHECK ? "" : "Fixed.");
		
		if (mode == RM_CHECK) {
			res |= RE_FIXABLE;
		} else {
			cde_set_units(place, e_count);
			place_mkdirty(place);
		}
	}
	
	if (flags->count != e_count) {
		/* Estimated count is greater then the recovered count, in other 
		   words there are some last unit headers should be removed. */
		aal_error("Node %llu, item %u: entries [%u..%u] look "
			  "corrupted. %s", place->node->block->nr,
			  place->pos.item, flags->count, e_count - 1, 
			  mode == RM_BUILD ? "Removed." : "");
		
		if (mode == RM_BUILD) {
			place->len -= cde40_cut(place, flags->count, 
						e_count - flags->count, 
						place->len);
			
			place_mkdirty(place);
		} else {
			res |= RE_FATAL;
		}
	}
	
	if (i) {
		/* Some first units should be removed. */
		aal_error("Node %llu, item %u: entries [%u..%u] look "
			  " corrupted. %s", place->node->block->nr, 
			  place->pos.item, 0, i - 1, 
			  mode == RM_BUILD ? "Removed." : "");
		
		if (mode == RM_BUILD) {
			place->len -= cde40_cut(place, 0, i, place->len);

			place_mkdirty(place);
			
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
			
			continue;
		}
		
		/* Looking for the problem interval end. */
		if (aal_test_bit(flags->elem + i, R)) {
			aal_error("Node %llu, item %u: entries "
				  "[%u..%u] look corrupted. %s", 
				  place->node->block->nr, place->pos.item,
				  last, i - 1, mode == RM_BUILD ? 
				  "Removed." : "");

			if (mode != RM_BUILD) {
				res |= RE_FATAL;
				last = MAX_UINT32;
				continue;
			}
			
			place->len -= cde40_cut(place, last, i - last, 
						place->len);

			place_mkdirty(place);
			
			aal_memmove(flags->elem + last, flags->elem + i,
				    flags->count - i);

			flags->count -= (i - last);
			i = last;
			last = MAX_UINT32;
		}
	}
	
	return res;
}

errno_t cde40_check_struct(reiser4_place_t *place, uint8_t mode) {
	static reiser4_key_t pkey, ckey;
	struct entry_flags flags;
	uint32_t pol;
	errno_t res = 0;
	int i;
	
	aal_assert("vpf-267", place != NULL);

	pol = cde40_key_pol(place);
	
	if (place->len < en_len_min(1, pol)) {
		aal_error("Node %llu, item %u: item length (%u) is too "
			  "small to contain a valid item.", 
			  place->node->block->nr, place->pos.item, place->len);
		return RE_FATAL;
	}
	
	/* Try to recover even if item was shorten and not all entries exist. */
	flags.count = (place->len - sizeof(cde40_t)) / (en_size(pol));
	
	/* map consists of bit pairs - [not relable -R, relable - R] */
	flags.elem = aal_calloc(flags.count, 0);
	
	res |= cde40_offsets_range_check(place, &flags, mode);
	
	if (res) goto error;
	
	/* Filter units with relable offsets from others. */
	res |= cde40_filter(place, &flags, mode);

	aal_free(flags.elem);
	
	if (repair_error_fatal(res))
		return res;
	
	/* Structure is checked, check the unit keys and its order.
	   FIXME-VITALY: just simple order check for now, the whole 
	   item is thrown away if smth wrong, to be improved later. */
	for (i = 1; i <= flags.count; i++) {
		reiser4_key_t key;
		trans_hint_t hint;
		uint32_t offset;
		
		cde40_get_hash(place, i - 1, &key);

		offset = i == flags.count ? place->len : 
			cde_get_offset(place, i, pol);
		
		if (cde_get_offset(place, i - 1, pol) + ob_size(pol) == offset){
			/* Check that [i-1] key is not hashed. */
			if (!plug_call(place->key.plug->o.key_ops, 
				       hashed, &key))
				continue;

			/* Hashed, key is wrong, remove the entry. */
			aal_error("Node (%llu), item (%u): wrong key "
				  "[%s] of the unit (%u).%s", 
				  place->node->block->nr, place->pos.item,
				  cde40_core->key_ops.print(&key, PO_INODE),
				  i - 1, mode == RM_BUILD ? " Removed." : "");
			
			if (mode != RM_BUILD) {
				res |= RE_FATAL;
				continue;
			}

			/* Remove the entry. */
			hint.count = 1;

			if ((res |= cde40_delete(place, i - 1, &hint)) < 0)
				return res;

			place->len -= hint.len;
			flags.count--;
			i--;
			continue;
		} else {
			/* Check that [i-1] key is hashed. */
			if (plug_call(place->key.plug->o.key_ops, 
				      hashed, &key))
				continue;
			
			/* Not hashed, key is wrong, remove the entry. */
			aal_error("Node (%llu), item (%u): wrong key "
				  "[%s] of the unit (%u).%s", 
				  place->node->block->nr, place->pos.item, 
				  cde40_core->key_ops.print(&key, PO_INODE),
				  i - 1, mode == RM_BUILD ? " Removed." : "");
			
			if (mode != RM_BUILD) {
				res |= RE_FATAL;
				continue;
			}
			
			/* Remove the entry. */
			hint.count = 1;

			if ((res |= cde40_delete(place, i - 1, &hint)) < 0)
				return res;

			place->len -= hint.len;
			flags.count--;
			i--;
			continue;
		}
	}
	
	if (res & RE_FATAL)
		return res;

	aal_memset(&pkey, 0, sizeof(pkey));
	aal_memset(&ckey, 0, sizeof(ckey));
	cde40_get_hash(place, 0, &pkey);
	
	for (i = 1; i < flags.count; i++) {
		cde40_get_hash(place, i, &ckey);
		
		if (plug_call(pkey.plug->o.key_ops, compfull, 
			      &pkey, &ckey) == 1) 
		{
			aal_error("Node (%llu), item (%u): wrong order of "
				  "units {%d, %d}. The whole item to be "
				  "removed -- will be improved soon.",
				  place->node->block->nr, place->pos.item,
				  i - 1, i);
			return res | RE_FATAL;
		}
		pkey = ckey;
	}
	
	if (flags.count == 0)
		return res | RE_FATAL;
	
	cde40_get_hash(place, 0, &ckey);
	if (plug_call(ckey.plug->o.key_ops, compfull, &ckey, &place->key)) {
		aal_error("Node (%llu), item (%u): the item key [%s] does "
			  "not match the first unit key [%s].%s", 
			  place->node->block->nr, place->pos.item,
			  cde40_core->key_ops.print(&place->key, PO_INODE),
			  cde40_core->key_ops.print(&ckey, PO_INODE),
			  mode == RM_BUILD ? " Fixed." : "");
		res |= RE_FATAL;
	}
	
	return res;
	
 error:
	aal_free(flags.elem);
	return res;
}

static int cde40_comp_entry(reiser4_place_t *place1, uint32_t pos1,
			    reiser4_place_t *place2, uint32_t pos2)
{
	reiser4_key_t key1, key2;
	char *name1, *name2;
	int comp;
	
	cde40_get_hash(place1, pos1, &key1);
	cde40_get_hash(place2, pos2, &key2);

	/* Compare hashes. */
	if ((comp = plug_call(key1.plug->o.key_ops, 
			      compfull, &key1, &key2)))
		return comp;

	/* If equal, and names are hashed, compare names. */
	if (!plug_call(key1.plug->o.key_ops, hashed, &key1) || 
	    !plug_call(key2.plug->o.key_ops, hashed, &key2))
	{
		return 0;
	}
	
	name1 = (char *)(cde40_objid(place1, pos1) + 
			 ob_size(cde40_key_pol(place1)));

	name2 = (char *)(cde40_objid(place2, pos2) + 
			 ob_size(cde40_key_pol(place2)));

	return aal_strcmp(name1, name2);
}

/* Estimate the space needed for the insertion of the not overlapped part 
   of the item, overlapped part does not need any space. */
errno_t cde40_prep_merge(reiser4_place_t *place, trans_hint_t *hint) {
	uint32_t sunits, send;
	uint32_t offset, pol;
	reiser4_place_t *src;

	aal_assert("vpf-957", place != NULL);
	aal_assert("vpf-959", hint != NULL);

	src = (reiser4_place_t *)hint->specific;
	sunits = cde40_units(src);
	pol = cde40_key_pol(place);

	if (place->pos.unit != MAX_UINT32 && 
	    place->pos.unit != cde40_units(place)) 
	{
		/* What is the last to be inserted? */
		for (send = src->pos.unit; send < sunits; send++) {
			if (cde40_comp_entry(place, place->pos.unit, 
					     src, send) <= 0)
				break;
		}
	} else
		send = sunits;

	hint->bytes = 0;
	hint->count = send - src->pos.unit;
	offset = send == sunits ? src->len : cde_get_offset(src, send, pol);
	
	/* Len to be inserted is the size of header + item bodies. */
	hint->len = hint->count * en_size(pol) + offset -
		cde_get_offset(src, src->pos.unit, pol);
	
	hint->overhead = (place->pos.unit == MAX_UINT32 && hint->len) ? 
		cde40_overhead(place) : 0;

	return 0;
}

int64_t cde40_merge(reiser4_place_t *place, trans_hint_t *hint) {
	uint32_t dpos, dunits;
	uint32_t spos, sunits;
	reiser4_place_t *src;
	errno_t res;
	
	aal_assert("vpf-1370", place != NULL);
	aal_assert("vpf-1371", hint != NULL);

	src = (reiser4_place_t *)hint->specific;
	
	sunits = cde40_units(src);
	
	if (hint->count) {
		/* Expand @place & copy @hint->count units there from @src. */
		dpos = place->pos.unit == MAX_UINT32 ? 0 : place->pos.unit;
		
		if (place->pos.unit != MAX_UINT32)
			cde40_expand(place, dpos, hint->count, hint->len);
		else
			cde_set_units(place, 0);
		
		res = cde40_copy(place, dpos, src, src->pos.unit, hint->count);
		if (res) return res;

		spos = src->pos.unit + hint->count;

		place_mkdirty(place);
	} else {
		/* The first @place and @src entries match to each other. 
		   Get the very first that differ. */
		dunits = cde40_units(place);
		for (dpos = place->pos.unit + 1, spos = src->pos.unit + 1;
		     dpos < dunits; dpos++, spos++)
		{
			if (spos >= sunits)
				break;

			if (cde40_comp_entry(place, dpos, src, spos))
				break;
		}
	}
	
	/* Update the item key. */
	if (place->pos.unit == 0 && hint->count) {
		plug_call(place->key.plug->o.key_ops, assign,
			  &place->key, &hint->offset);
	}
	
	if (spos == sunits)
		return cde40_maxposs_key(src, &hint->maxkey);
	
	return cde40_get_hash(src, spos, &hint->maxkey);
}

#define PRINT_NAME_LIMIT 38

/* Prints cde item into passed @stream */
void cde40_print(reiser4_place_t *place, aal_stream_t *stream, uint16_t options) {
	uint32_t pol;
	uint32_t i, j;
	char name[256];
	uint64_t locality;
	uint64_t objectid;
	uint32_t namewidth;
	
	aal_assert("umka-548", place != NULL);
	aal_assert("umka-549", stream != NULL);

	aal_stream_format(stream, "UNITS=%u\n", cde_get_units(place));
	aal_stream_format(stream, "NR  NAME%*s OFFSET HASH%*s "
			  "SDKEY%*s\n", PRINT_NAME_LIMIT - 5, " ", 29, " ", 13, " ");
	
	pol = cde40_key_pol(place);

	/* Loop though the all entries and print them to @stream. */
	for (i = 0; i < cde_get_units(place); i++) {
		uint64_t offset, haobj;
		void *objid = cde40_objid(place, i);
		void *entry = cde40_entry(place, i);

		if (objid >= place->body + place->len || 
		    entry >= place->body + place->len)
		{
			aal_stream_format(stream, "Broken entry array "
					  "detected\n.");
			break;
		}
		
		cde40_get_name(place, i, name, sizeof(name));

		/* Cutting name by PRINT_NAME_LIMIT symbols */
		if (aal_strlen(name) > PRINT_NAME_LIMIT) {
			for (j = 0; j < 3; j++)
				name[PRINT_NAME_LIMIT - 3 + j] = '.';

			name[PRINT_NAME_LIMIT - 3 + j] = '\0';
		}

		/* Getting locality, objectid. */
		locality = ob_get_locality(objid, pol);
		objectid = ob_get_objectid(objid, pol);
		namewidth = PRINT_NAME_LIMIT - aal_strlen(name);

		offset = ha_get_offset(entry, pol);
		haobj = ha_get_objectid(entry, pol);

		/* Putting data to @stream. */
		aal_stream_format(stream, "%*d %s%*s %*u %.16llx:%.16llx "
				  "%.7llx:%.7llx\n", 3, i, name, namewidth,
				  " ", 5, en_get_offset(entry, pol), haobj,
				  offset, locality, objectid);
	}
}

#endif
