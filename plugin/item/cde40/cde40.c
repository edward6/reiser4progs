/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde40.c -- reiser4 directory entry plugin. */

#include "cde40.h"
#include "cde40_repair.h"

reiser4_core_t *cde40_core = NULL;

inline uint32_t cde40_key_pol(reiser4_place_t *place) {
	return plug_call(place->key.plug->o.key_ops, bodysize);
}

/* Returns pointer to entry */
inline void *cde40_entry(reiser4_place_t *place, uint32_t pos) {
	return cde_get_entry(place, pos, cde40_key_pol(place));
}

/* Returns pointer to the objectid entry component. */
inline void *cde40_objid(reiser4_place_t *place, uint32_t pos) {
	return (place->body + en_get_offset(cde40_entry(place, pos),
					    cde40_key_pol(place)));
}

/* Returns pointer to entry offset */
static inline void *cde40_hash(reiser4_place_t *place, uint32_t pos) {
	return cde40_entry(place, pos);
}

/* Returns statdata key of the object entry points to */
static void cde40_get_obj(reiser4_place_t *place, uint32_t pos,
			  reiser4_key_t *key)
{
	key->plug = place->key.plug;
	plug_call(key->plug->o.key_ops, clean, key);

	aal_memcpy(key->body, cde40_objid(place, pos),
		   ob_size(cde40_key_pol(place)));
}

/* Stores entry offset (hash) to passed @key */
errno_t cde40_get_hash(reiser4_place_t *place, uint32_t pos,
		       reiser4_key_t *key)
{
	void *hash;
	uint32_t pol;
	uint64_t locality;

	pol = cde40_key_pol(place);
	hash = cde40_hash(place, pos);

	/* Getting item key params */
	locality = plug_call(place->key.plug->o.key_ops,
			     get_locality, &place->key);

	/* Building the full key from entry at @place */
	plug_call(place->key.plug->o.key_ops, build_generic, key,
		  KEY_FILENAME_TYPE, locality, ha_get_ordering(hash, pol),
		  ha_get_objectid(hash, pol), ha_get_offset(hash, pol));

	return 0;
}

/* Set the key for the entry->offset. It is needed for fixing entry keys if
   repair code detects it is wrong. */
errno_t cde40_set_hash(reiser4_place_t *place, uint32_t pos,
		       reiser4_key_t *key)
{
	void *hash;
	uint32_t pol;
	uint64_t offset;
	uint64_t objectid;
	uint64_t ordering;

	pol = cde40_key_pol(place);
	hash = cde40_hash(place, pos);
	
	ordering = plug_call(place->key.plug->o.key_ops,
			     get_ordering, &place->key);
	
	objectid = plug_call(place->key.plug->o.key_ops,
			     get_fobjectid, &place->key);
	
	offset = plug_call(place->key.plug->o.key_ops,
			   get_offset, &place->key);

	ha_set_ordering(hash, ordering, pol);
	ha_set_objectid(hash, objectid, pol);
	ha_set_offset(hash, offset, pol);
	
	return 0;
}

/* Extracts entry name from the passed @entry to passed @buff */
char *cde40_get_name(reiser4_place_t *place, uint32_t pos,
		     char *buff, uint32_t len)
{
        reiser4_key_t key;
                                                                                        
        cde40_get_hash(place, pos, &key);
                                                                                        
        /* If name is long, we just copy it from the area after
           objectid. Otherwise we extract it from the entry hash. */
        if (plug_call(key.plug->o.key_ops, hashed, &key)) {
                char *ptr = (char *)(cde40_objid(place, pos) +
				     ob_size(cde40_key_pol(place)));
                aal_strncpy(buff, ptr, len);
        } else {
		plug_call(key.plug->o.key_ops, get_name, &key, buff);
        }
                                                                                        
        return buff;
}

#ifndef ENABLE_STAND_ALONE
/* Calculates entry length. This function is widely used in shift code and
   modification code. */
static uint32_t cde40_get_len(reiser4_place_t *place, uint32_t pos) {
	uint32_t len;
	uint32_t pol;
	reiser4_key_t key;

	/* Counting objid size */
	pol = cde40_key_pol(place);
	len = ob_size(pol);

	/* Getting entry key */
	cde40_get_hash(place, pos, &key);
	
	/* If entry contains long name it is stored just after objectid.
	   Otherwise, entry name is stored in objectid and offset of the
	   entry. This trick saves a lot of space in directories, because the
	   average name is shorter than 15 symbols. */
	if (plug_call(key.plug->o.key_ops, hashed, &key)) {
		len += aal_strlen((char *)(cde40_objid(place, pos) +
					   ob_size(pol))) + 1;
	}
	
	return len;
}
#endif

/* Builds full key by entry components. It is needed for updating keys after
   shift, insert, etc. Also library requires unit keys sometims. */
errno_t cde40_fetch_key(reiser4_place_t *place, reiser4_key_t *key) {
	aal_assert("umka-1606", key != NULL);
	aal_assert("umka-1607", place != NULL);
	aal_assert("umka-1605", place->body != NULL);

	return cde40_get_hash(place, place->pos.unit, key);
}

/* Updates entry offset. It is needed for fixing entry keys if repair code
   detects it is wrong. */
errno_t cde40_update_key(reiser4_place_t *place, reiser4_key_t *key) {
	aal_assert("vpf-1228", key != NULL);
	aal_assert("vpf-1229", place != NULL);
	aal_assert("vpf-1230", place->body != NULL);

	return cde40_set_hash(place, place->pos.unit, key);
}

/* Returns the number of units. */
uint32_t cde40_units(reiser4_place_t *place) {
	aal_assert("umka-865", place != NULL);
	return cde_get_units(place);
}

/* Fetches some number of cde items to passed @hint. */
static int64_t cde40_fetch_units(reiser4_place_t *place, trans_hint_t *hint) {
	uint32_t i, pos;
	entry_hint_t *entry;
    
	aal_assert("umka-1418", hint != NULL);
	aal_assert("umka-866", place != NULL);

	pos = place->pos.unit;
	entry = (entry_hint_t *)hint->specific;
	
	for (i = pos; i < pos + hint->count; i++, entry++) {
		/* Get object stat data key. */
		cde40_get_obj(place, i, &entry->object);

		/* Get entry key (hash). */
		cde40_get_hash(place, i, &entry->offset);

		/* Extract entry name. */
		cde40_get_name(place, i, entry->name,
			       sizeof(entry->name));
	}
	
	return hint->count;
}

#ifndef ENABLE_STAND_ALONE
uint16_t cde40_overhead() {
	return sizeof(cde40_t);
}

/* Returns 1 if items are mergeable, 0 -- otherwise. That is if they belong to
   the same directory. This function is used in shift code from the node plugin
   in order to determine are two items may be merged or not. */
static int cde40_mergeable(reiser4_place_t *place1, reiser4_place_t *place2) {
	aal_assert("umka-1581", place1 != NULL);
	aal_assert("umka-1582", place2 != NULL);

	/* Items mergeable if their short keys match. */
	return !plug_call(place1->key.plug->o.key_ops,
			  compshort, &place1->key, &place2->key);
}

/* Calculates the size of @count units (entries) in passed @place at passed
   @pos. */
uint32_t cde40_regsize(reiser4_place_t *place, uint32_t pos, uint32_t count) {
	uint32_t pol;
	uint32_t size;
	void *entry_end;
	void *entry_start;

	if (count == 0)
		return 0;

	pol = cde40_key_pol(place);
	entry_start = cde40_entry(place, pos);

	if (pos + count < cde_get_units(place)) {
		size = 0;
		entry_end = cde40_entry(place, pos + count);
	} else {
		entry_end = cde40_entry(place, pos + count - 1);
		size = cde40_get_len(place, pos + count - 1);

	}
	
	size += (en_get_offset(entry_end, pol) -
		 en_get_offset(entry_start, pol));

	return size;
}

/* Makes copy of @count amount of units from @src_item to @dst_one */
errno_t cde40_copy(reiser4_place_t *dst, uint32_t dst_pos,
		   reiser4_place_t *src, uint32_t src_pos,
		   uint32_t count)
{
        uint32_t i;
        void *entry;
        uint32_t pol;
        uint32_t size;
        uint32_t offset;
        uint32_t headers;
        void *dstp, *srcp;
        uint32_t dst_units;

        aal_assert("umka-2069", dst != NULL);
        aal_assert("umka-2070", src != NULL);

        pol = cde40_key_pol(dst);
        dst_units = cde40_units(dst);

        aal_assert("umka-2077", dst_pos <= dst_units);

        /* Getting offset of body in dst place */
        offset = cde40_regsize(dst, 0, dst_pos);

        /* Copying entry headers */
        srcp = src->body + sizeof(cde40_t) +
                (src_pos * en_size(pol));

        dstp = dst->body + sizeof(cde40_t) +
                (dst_pos * en_size(pol));

        headers = count * en_size(pol);
        aal_memcpy(dstp, srcp, headers);

        /* Copying entry bodies */
        srcp = src->body + en_get_offset(srcp, pol);

        dstp = dst->body + sizeof(cde40_t) +
                (dst_units * en_size(pol)) +
		headers + offset;

        size = cde40_regsize(src, src_pos, count);

        aal_memcpy(dstp, srcp, size);

        /* Updating offset of dst cde */
        entry = cde40_entry(dst, dst_pos);

        offset += sizeof(cde40_t) +
                (dst_units * en_size(pol)) + headers;

        for (i = 0; i < count; i++) {
                en_set_offset(entry, offset, pol);
                offset += cde40_get_len(src, src_pos + i);
                entry += en_size(pol);
        }

        /* Updating cde units */
        cde_inc_units(dst, count);

        /* Updating item key by unit key if the first unit waqs changed. It is
           needed for correct updating left delimiting keys. */
        if (dst_pos == 0) {
                cde40_get_hash(dst, 0, &dst->key);
        }

        place_mkdirty(dst);
        return 0;
}

/* Shrinks cde item in order to delete some entries */
static uint32_t cde40_shrink(reiser4_place_t *place, uint32_t pos,
			     uint32_t count, uint32_t len)
{
	void *entry;
	uint32_t pol;
	uint32_t first;
	uint32_t second;
	uint32_t remove;
	uint32_t headers;
	uint32_t i, units;

	aal_assert("umka-1959", place != NULL);

	pol = cde40_key_pol(place);
	units = cde_get_units(place);
	
	aal_assert("umka-1681", pos < units);

	if (pos + count > units)
		count = units - pos;

	if (count == 0)
		return 0;

	headers = count * en_size(pol);
	
	/* Getting how many bytes should be moved before passed @pos */
	first = (units - (pos + count)) *
		en_size(pol);
	
	first += cde40_regsize(place, 0, pos);

	/* Getting how many bytes should be moved after passed @pos */
	second = cde40_regsize(place, pos + count,
			       units - (pos + count));

	/* Calculating how many bytes will be moved out */
	remove = cde40_regsize(place, pos, count);

	/* Moving headers and first part of bodies (before passed @pos) */
	entry = cde40_entry(place, pos);
	aal_memmove(entry, entry + (en_size(pol) * count), first);

	/* Setting up the entry offsets */
	entry = cde40_entry(place, 0);
	
	for (i = 0; i < pos; i++) {
		en_dec_offset(entry, headers, pol);
		entry += en_size(pol);
	}

	/* We also move the rest of the data (after insert point) if needed. */
	if (second > 0) {
		void *src, *dst;

		entry = cde40_entry(place, pos);
		src = place->body + en_get_offset(entry, pol);
		dst = src - (headers + remove);
		
		aal_memmove(dst, src, second);

		/* Setting up entry offsets */
		for (i = pos; i < units - count; i++) {
			entry = cde40_entry(place, i);
			en_dec_offset(entry, (headers + remove), pol);
		}
	}

	cde_dec_units(place, count);
	place_mkdirty(place);
	
	return remove;
}

/* Prepares cde40 for insert new entries */
uint32_t cde40_expand(reiser4_place_t *place, uint32_t pos,
		      uint32_t count, uint32_t len)
{
	void *entry;
	uint32_t pol;
	uint32_t first;
	void *src, *dst;
	uint32_t second;
	uint32_t offset;
	uint32_t headers;
	uint32_t i, units;

	aal_assert("umka-1724", len > 0);
	aal_assert("umka-1724", count > 0);
	aal_assert("umka-1723", place != NULL);

	pol = cde40_key_pol(place);
	units = cde_get_units(place);
	headers = (count * en_size(pol));

	aal_assert("umka-1722", pos <= units);

	/* Getting the offset of the place new entries will be inserted at. It
	   will be used later in this function. */
	if (units > 0) {
		if (pos < units) {
			entry = cde40_entry(place, pos);
			
			offset = en_get_offset(entry, pol) +
				headers;
		} else {
			entry = cde40_entry(place, units - 1);
			
			offset = en_get_offset(entry, pol) + en_size(pol) +
				cde40_get_len(place, units - 1);
		}
	} else {
		offset = sizeof(cde40_t) + headers;
	}

	/* Calculating length bytes to be moved before insert point. */
	first = (units - pos) * en_size(pol);
	first += cde40_regsize(place, 0, pos);
	
	/* Calculating length bytes to be moved after insert point. */
	second = cde40_regsize(place, pos, units - pos);
	
	/* Updating offset of entries which lie before insert point. */
	entry = cde40_entry(place, 0);
	
	for (i = 0; i < pos; i++) {
		en_inc_offset(entry, headers, pol);
		entry += en_size(pol);
	}
    
	/* Updating offset of entries which lie after insert point. */
	entry = cde40_entry(place, pos);
	
	for (i = pos; i < units; i++) {
		en_inc_offset(entry, len, pol);
		entry += en_size(pol);
	}
    
	/* Moving entry bodies if it is needed. */
	if (pos < units) {
		src = (place->body + offset) - headers;
		dst = (place->body + offset + len) - headers;
		aal_memmove(dst, src, second);
	}
    
	/* Moving unit headers if it is needed. */
	if (first > 0) {
		src = cde40_entry(place, pos);
		dst = src + headers;
		aal_memmove(dst, src, first);
	}
    
	place_mkdirty(place);
	return offset;
}

/* Predicts how many entries and bytes can be shifted from the @src_place to
   @dst_place. The behavior of the function depends on the passed @hint. */
static errno_t cde40_prep_shift(reiser4_place_t *src_place, reiser4_place_t *dst_place,
				shift_hint_t *hint)
{
	int check;
	uint32_t pol;
	uint32_t curr;
	uint32_t flags;
	uint32_t src_units;
	uint32_t dst_units;
	uint32_t space, len;
	
	aal_assert("umka-1592", hint != NULL);
	aal_assert("umka-1591", src_place != NULL);

	hint->units_number = 0;
	space = hint->units_bytes;

	pol = cde40_key_pol(src_place);
	src_units = cde40_units(src_place);
	dst_units = dst_place ? cde40_units(dst_place) : 0;

	/* If hint's create flag is present, we need to create new cde item, so
	   we should count its overhead. */
	if (hint->create) {
		if (space < sizeof(cde40_t))
			return 0;

		/* There is only space for header. Getting us out. */
		if (!(space -= sizeof(cde40_t)))
			return 0;
	}

	flags = hint->control;

	/* Getting start item header needed inside loop. */
	curr = (hint->control & SF_ALLOW_LEFT ? 0 :
		src_units - 1);

	/* If we need to check insert point. */
	check = (src_place->pos.item == hint->pos.item &&
		 hint->pos.unit != MAX_UINT32);

	while (!(hint->result & SF_MOVE_POINT) &&
	       curr < cde40_units(src_place))
	{

		/* Check if we have already moved everything possible and are on
		   edge of item and will be moved to neighbour item. */
		if (check && (flags & SF_UPDATE_POINT)) {
			if (!(flags & SF_MOVE_POINT)) {
				if (flags & SF_ALLOW_LEFT) {
					if (hint->pos.unit == 0)
						break;
				} else {
					if (hint->pos.unit == src_units)
						break;
				}
			}
		}

		/* Check is we have enough free space for shifting one more unit
		   from src item to dst item. */
		len = cde40_get_len(src_place, curr);

		if (space < len + en_size(pol))
			break;

		/* Updating unit pos. We will do so in the case item component
		   of insert point is the same as current item has and unit
		   component is not MAX_UINT32. */
		if (check && (flags & SF_UPDATE_POINT)) {
			if (flags & SF_ALLOW_LEFT) {
				/* Insert point is near to be moved into left
				   neighbour. Checking if we are permitted to do
				   so and updating insert point. */
				if (hint->pos.unit == 0) {
					if (flags & SF_MOVE_POINT) {
						hint->result |= SF_MOVE_POINT;
						hint->pos.unit = dst_units;
					}
					
					break;
				} else {
					hint->pos.unit--;
				}
			} else {
				if (hint->pos.unit >= src_units - 1) {
					/* Insert point is near to be shifted in
					   right neighbour. Checking permissions
					   and updating unit component of insert
					   point int hint. */
					if (hint->pos.unit == src_units - 1) {
						if (flags & SF_MOVE_POINT) {
							hint->result |= SF_MOVE_POINT;
							hint->pos.unit = 0;
						} else {
							break;
						}
					} else {
						if (flags & SF_MOVE_POINT) {
							hint->result |= SF_MOVE_POINT;
							hint->pos.unit = 0;
						}
						
						break;
					}
				}
			}
		}

		/* Updating unit number counters and some local variables needed
		   for controlling predicting main cycle. */
		src_units--;
		dst_units++;
		hint->units_number++;

		space -= (len + en_size(pol));
		curr += (flags & SF_ALLOW_LEFT ? 1 : -1);
	}

	/* Updating @hint->units_bytes. It is needed for unit shifting. This
	   value is number of bytes to be moved from @src_place to
	   @dst_place. */
	if (hint->units_number > 0)
		hint->units_bytes -= space;
	
	return 0;
}

/* Makes shift of the entries from the @src_place to the @dst_place. */
static errno_t cde40_shift_units(reiser4_place_t *src_place, 
				 reiser4_place_t *dst_place,
				 shift_hint_t *hint)
{
	uint32_t src_pos, dst_pos;
	
	aal_assert("umka-1589", hint != NULL);
	aal_assert("umka-1586", src_place != NULL);
	aal_assert("umka-1587", dst_place != NULL);

	/* Initializing cde body if we shift data to new created item. This is
	   needed for correct work of cde plugin. */
	if (hint->create) {
		hint->units_bytes -= sizeof(cde40_t);
		((cde40_t *)dst_place->body)->units = 0;
	}

	/* Calculating src and dst positions for expanding dst item, copy data
	   to it and shrinking src item. */
	if (hint->control & SF_ALLOW_LEFT) {
		src_pos = 0;
		dst_pos = cde_get_units(dst_place);
	} else {
		dst_pos = 0;
		src_pos = cde_get_units(src_place) -
			hint->units_number;
	}

	/* Preparing root for copying units into it. */
	cde40_expand(dst_place, dst_pos, hint->units_number,
		     hint->units_bytes);

	/* Copying units from @src place to @dst one. */
	cde40_copy(dst_place, dst_pos, src_place,
		   src_pos, hint->units_number);

	cde40_shrink(src_place, src_pos,
		     hint->units_number, 0);

	/* Updating item key by first cde key. */
	if (cde_get_units(src_place) > 0 &&
	    hint->control & SF_ALLOW_LEFT)
	{
		cde40_get_hash(src_place, 0,
			       &src_place->key);
	}

	return 0;
}

/* Estimates how many bytes will be needed to make room for inserting new
   entries. */
static errno_t cde40_prep_insert(reiser4_place_t *place, trans_hint_t *hint) {
	uint32_t i, pol;
	entry_hint_t *entry;
	    
	aal_assert("vpf-095", hint != NULL);
	aal_assert("umka-2424", place != NULL);
	aal_assert("umka-2229", hint->count > 0);

	pol = plug_call(hint->offset.plug->o.key_ops,
			bodysize);
	
	entry = (entry_hint_t *)hint->specific;
	hint->len = (hint->count * en_size(pol));
    
	for (i = 0; i < hint->count; i++, entry++) {
		hint->len += ob_size(pol);

		/* Calling key plugin for in odrer to find 
		   out is passed name is long one or not. */
		if (plug_call(hint->offset.plug->o.key_ops,
			      hashed, &entry->offset))
		{
			/* Okay, name is long, so we need add 
			   its length to estimated length. */
			hint->len += aal_strlen(entry->name) + 1;
		}
	}

	hint->bytes = hint->len;
	
	/* If the pos we are going to insert new units is -1, we assume 
	   it is the attempt to insert new directory item. In this case 
	   we should also count item overhead, that is cde40 header which 
	   contains the number of entries in item. */
	hint->overhead = (place->pos.unit == MAX_UINT32 ?
		     cde40_overhead() : 0);
	
	return 0;
}

/* Inserts new entries to cde item. */
static int64_t cde40_insert_units(reiser4_place_t *place, trans_hint_t *hint) {
	void *entry;
	uint32_t pol, i, offset;
	entry_hint_t *entry_hint;
    
	aal_assert("umka-792", hint != NULL);
	aal_assert("umka-791", place != NULL);

	pol = cde40_key_pol(place);
	entry_hint = (entry_hint_t *)hint->specific;

	/* Initialize body and normalize unit pos in the case of insert new
	   item. */
	if (place->pos.unit == MAX_UINT32) {
		((cde40_t *)place->body)->units = 0;
		place->pos.unit = 0;
	}
	
	/* Expanding direntry in order to prepare the room for new entries. The
	   function cde40_expand() returns the offset of where new unit will be
	   inserted at. */
	offset = cde40_expand(place, place->pos.unit,
			      hint->count, hint->len);

	entry = cde40_entry(place, place->pos.unit);
	
	/* Creating new entries */
	for (i = 0; i < hint->count; i++, entry_hint++) {
		void *objid;
		uint64_t oid;
		uint64_t off;
		uint64_t ord;

		reiser4_key_t *hash;
		reiser4_key_t *object;

		objid = place->body + offset;
		
		/* Setting up the offset of new entry */
		en_set_offset(entry, offset, pol);
		hash = &entry_hint->offset;
		
		/* Setting up ordering component of hash */
		ord = plug_call(hash->plug->o.key_ops,
				get_ordering, hash);
		
		ha_set_ordering(entry, ord, pol);
		
		/* Setting up objectid component of hash */
		oid = plug_call(hash->plug->o.key_ops,
				get_fobjectid, hash);
		
		ha_set_objectid(entry, oid, pol);

		/* Setting up offset component of hash */
		off = plug_call(hash->plug->o.key_ops,
				get_offset, hash);

		ha_set_offset(entry, off, pol);

		/* Setting up objid fields */
		object = &entry_hint->object;

		aal_memcpy(objid, object->body,
			   ob_size(pol));

		offset += ob_size(pol);

		/* If key is long one we also count name length */
		if (plug_call(place->key.plug->o.key_ops,
			      hashed, &entry_hint->offset))
		{
			uint32_t len = aal_strlen(entry_hint->name);

			aal_memcpy(place->body + offset,
				   entry_hint->name, len);

			offset += len;
			*((char *)place->body + offset) = '\0';
			offset++;
		}

		entry += en_size(pol);
	}
	
	cde_inc_units(place, hint->count);
	
	/* Updating item key by unit key if the first unit was changed. It is
	   needed for correct updating left delimiting keys. */
	if (place->pos.unit == 0)
		cde40_get_hash(place, 0, &place->key);

	place_mkdirty(place);
	return hint->count;
}

/* Remove some number of entries (based on passed @hint) from cde40 item
   starting from @pos. */
errno_t cde40_delete(reiser4_place_t *place, uint32_t pos,
		     trans_hint_t *hint)
{
	uint32_t pol;
	uint32_t bytes;

	aal_assert("umka-3024", pos < cde40_units(place));

	pol = cde40_key_pol(place);
	bytes = (hint->count * en_size(pol));
	
	/* Shrinking cde item */
	bytes += cde40_shrink(place, pos, hint->count, 0);
	
	/* Updating item key */
	if (pos == 0 && cde40_units(place) > 0)
		cde40_get_hash(place, 0, &place->key);

	hint->overhead = 0;
	hint->bytes = hint->len = bytes;

	/* Setting up cde item overhead in the case we removed all units. */
	if (cde40_units(place) == 0)
		hint->overhead = cde40_overhead();
	
	return 0;
}

/* Removes @count entries at @pos from passed @place */
static errno_t cde40_remove_units(reiser4_place_t *place, trans_hint_t *hint) {
	aal_assert("umka-934", place != NULL);
	aal_assert("umka-2400", hint != NULL);

	return cde40_delete(place, place->pos.unit, hint);
}

/* Fuses bodies of two cde items that lie in the same node. */
static int32_t cde40_fuse(reiser4_place_t *left_place, 
			  reiser4_place_t *right_place) 
{
	void *buf;
	void *entry;
	uint32_t pol;
	uint32_t size;
	uint32_t offset;
	uint32_t i, units;
	
	aal_assert("umka-2687", left_place != NULL);
	aal_assert("umka-2689", right_place != NULL);
	
	pol = cde40_key_pol(left_place);
	
	units = cde_get_units(left_place);
	
	/* Right entry headers to be copied to buffer. */
	size = cde_get_units(right_place) * en_size(pol);
	
	/* Offset of left entry bodies. */
	offset = sizeof(cde40_t) + cde_get_units(left_place) * en_size(pol);

	/* Set the correct amount of units. */
	cde_inc_units(left_place, cde_get_units(right_place));
	
	/* Eliminating right item header and return header size as space
	   released as result of fuse. */
	aal_memmove(right_place->body, 
		    right_place->body + sizeof(cde40_t), 
		    right_place->len - sizeof(cde40_t));

	/* Allocate the buffer for the right entry headers and copy them. */
	buf = aal_malloc(size);
	aal_memcpy(buf, right_place->body, size);
	
	/* Move left item bodies to the right. */
	aal_memmove(left_place->body + offset + size, 
		    left_place->body + offset, 
		    left_place->len - offset);
	
	/* Copy the right entry headers to the freed space in the left item. */
	aal_memcpy(left_place->body + offset, buf, size);

	aal_free(buf);
	
	offset = size;
	entry = cde40_entry(left_place, 0);
	
	/* Update left offsets. */
	for (i = 0; i < units; i++) {
		en_inc_offset(entry, offset, pol);
		entry += en_size(pol);
	}
	
	offset = left_place->len - sizeof(cde40_t);
	entry = cde40_entry(left_place, units);
	
	/* Update right offsets. */
	for (i = units; i < cde_get_units(left_place); i++) {
		en_inc_offset(entry, offset, pol);
		entry += en_size(pol);
	}
	
	return sizeof(cde40_t);
}

/* Returns real maximal key in cde item */
static errno_t cde40_maxreal_key(reiser4_place_t *place, 
				 reiser4_key_t *key) 
{
	uint32_t units;
	
	aal_assert("umka-1651", key != NULL);
	aal_assert("umka-1650", place != NULL);

	units = cde40_units(place);
	return cde40_get_hash(place, units - 1, key);
}

static uint64_t cde40_size(reiser4_place_t *place) {
	aal_assert("umka-2677", place != NULL);
	return cde40_units(place);
}

static uint64_t cde40_bytes(reiser4_place_t *place) {
	aal_assert("vpf-1211", place != NULL);
	return (place->len - sizeof(uint16_t));
}
#endif

/* Returns maximal possible key in passed item. It is needed during lookup and
   in other cases. */
errno_t cde40_maxposs_key(reiser4_place_t *place, 
			  reiser4_key_t *key) 
{
	reiser4_key_t *maxkey;

	aal_assert("umka-1649", key != NULL);
	aal_assert("umka-1648", place != NULL);

	plug_call(place->key.plug->o.key_ops,
		  assign, key, &place->key);

	/* Getting maximal key from current key plugin. */
	maxkey = plug_call(key->plug->o.key_ops,
			   maximal);

	/* Setting up @key by mans of putting to it offset, ordering and
	   objectid from values from maximal key. */
    	plug_call(key->plug->o.key_ops, set_ordering,
		  key, plug_call(key->plug->o.key_ops,
				 get_ordering, maxkey));
	
    	plug_call(key->plug->o.key_ops, set_objectid,
		  key, plug_call(key->plug->o.key_ops,
				 get_objectid, maxkey));
	
	plug_call(key->plug->o.key_ops, set_offset,
		  key, plug_call(key->plug->o.key_ops,
				 get_offset, maxkey));
	
	return 0;
}

/* Compare the given key with the entry at the given pos. */
int cde40_comp_hash(reiser4_place_t *place, uint32_t pos, reiser4_key_t *key) {
	reiser4_key_t curr;

	cde40_get_hash(place, pos, &curr);

	return plug_call(place->key.plug->o.key_ops,
			 compfull, &curr, key);
}

/* Helper function that is used by lookup method for 
   comparing given key with passed entry hash. */
static int callback_comp_hash(void *array, uint32_t pos,
			      void *key, void *data)
{
	return cde40_comp_hash((reiser4_place_t *)data, pos, 
			       (reiser4_key_t *)key);
}

/* Performs lookup inside cde item. Found position is stored in @pos. */
lookup_t cde40_lookup(reiser4_place_t *place,
		      lookup_hint_t *hint,
		      lookup_bias_t bias)
{
#ifndef ENABLE_STAND_ALONE
	int32_t i;
#endif

	aal_assert("umka-610", hint != NULL);
	aal_assert("umka-609", place != NULL);
    
	/* Bin search within the cde item to get the position of 
	   the wanted key. */
	switch (aux_bin_search(place->body, cde40_units(place),
			       hint->key, callback_comp_hash,
			       place, &place->pos.unit))
	{
	case 1:
#ifndef ENABLE_STAND_ALONE
		/* Making sure, that we have found right unit. This is needed
		   because of possible key collision. We move left direction
		   until we find a key smaller than passed one. */
		for (i = place->pos.unit - 1; i >= 0; i--) {
			/* Comparing keys. We break the loop when keys as not
			   equal, that means, that we have found needed pos. */
			if (!cde40_comp_hash(place, i, hint->key))
				place->pos.unit = i;
			else
				return PRESENT;
		}
#endif
		return PRESENT;
	case 0:
		return (bias == FIND_CONV ? PRESENT : ABSENT);
	default:
		return -EIO;
	}
}

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_STAND_ALONE
	.fuse		  = cde40_fuse, 
	.mergeable	  = cde40_mergeable,
	.prep_shift	  = cde40_prep_shift,
	.shift_units	  = cde40_shift_units,
        .maxreal_key	  = cde40_maxreal_key,
	.update_key	  = cde40_update_key,
#endif
	.units		  = cde40_units,
	.lookup		  = cde40_lookup,
	.fetch_key	  = cde40_fetch_key,
	.maxposs_key	  = cde40_maxposs_key
};

static item_object_ops_t object_ops = {
	.fetch_units	  = cde40_fetch_units,
	
#ifndef ENABLE_STAND_ALONE
	.prep_insert	  = cde40_prep_insert,
	.insert_units	  = cde40_insert_units,
	.remove_units	  = cde40_remove_units,

	.size		  = cde40_size,
	.bytes		  = cde40_bytes,
	.overhead	  = cde40_overhead,
		 
	.update_units	  = NULL,
	.prep_write	  = NULL,
	.write_units	  = NULL,
	.trunc_units	  = NULL,
	.layout		  = NULL,
#endif
	.read_units	  = NULL,
	.object_plug	  = NULL
};

static item_debug_ops_t debug_ops = {
#ifndef ENABLE_STAND_ALONE
	.print		  = cde40_print
#endif
};

static item_repair_ops_t repair_ops = {
#ifndef ENABLE_STAND_ALONE
	.check_struct	  = cde40_check_struct,
	.check_layout	  = NULL,

	.prep_merge	  = cde40_prep_merge,
	.merge		  = cde40_merge,

	.pack		  = NULL,
	.unpack		  = NULL
#endif
};

static item_tree_ops_t tree_ops = {
	.down_link	  = NULL,
#ifndef ENABLE_STAND_ALONE
	.update_link	  = NULL
#endif
};

static reiser4_item_ops_t cde40_ops = {
	.tree		  = &tree_ops,
	.debug		  = &debug_ops,
	.object		  = &object_ops,
	.repair		  = &repair_ops,
	.balance	  = &balance_ops
};

static reiser4_plug_t cde40_plug = {
	.cl    = class_init,
	.id    = {ITEM_CDE40_ID, DIRENTRY_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "cde40",
	.desc  = "Compound direntry for reiser4, ver. " VERSION,
#endif
	.o = {
		.item_ops = &cde40_ops
	}
};

static reiser4_plug_t *cde40_start(reiser4_core_t *c) {
	cde40_core = c;
	return &cde40_plug;
}

plug_register(cde40, cde40_start, NULL);
