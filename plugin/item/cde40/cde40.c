/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde40.c -- reiser4 directory entry plugin. */

#include "cde40.h"

reiser4_core_t *cde_core = NULL;

inline uint32_t cde40_key_pol(place_t *place) {
	return plug_call(place->key.plug->o.key_ops, bodysize);
}

static void *cde40_entry(place_t *place, uint32_t pos) {
	return cde_get_entry(place, pos, cde40_key_pol(place));
}

/* Returns pointer to the objectid entry component. */
static void *cde40_objid(place_t *place, uint32_t pos) {
	void *entry = cde40_entry(place, pos);
	return (place->body + en_get_offset(entry, cde40_key_pol(place)));
}

static void *cde40_hash(place_t *place, uint32_t pos) {
	return cde40_entry(place, pos);
}

/* Returns statdata key of the object entry points to */
static void cde40_get_obj(place_t *place, uint32_t pos,
			  key_entity_t *key)
{
	void *objid = cde40_objid(place, pos);
	
	key->plug = place->key.plug;
	plug_call(key->plug->o.key_ops, clean, key);
	aal_memcpy(key->body, objid, ob_size(cde40_key_pol(place)));
}

/* Builds full key by entry components. It is needed for updating keys after
   shift, insert, etc. Also library requires unit keys sometims. */
errno_t cde40_get_key(place_t *place, uint32_t pos,
		      key_entity_t *key)
{
	void *hash;
	uint32_t pol;
	uint64_t locality;

	aal_assert("umka-1606", key != NULL);
	aal_assert("umka-1607", place != NULL);
	aal_assert("umka-1605", place->body != NULL);

	pol = cde40_key_pol(place);
	hash = cde40_hash(place, pos);

	/* Getting item key params */
	locality = plug_call(place->key.plug->o.key_ops,
			     get_locality, &place->key);

	/* Building the full key from entry at @pos */
	plug_call(place->key.plug->o.key_ops, build_gener, key,
		  KEY_FILENAME_TYPE, locality, ha_get_ordering(hash, pol),
		  ha_get_objectid(hash, pol), ha_get_offset(hash, pol));

	return 0;
}

/* Set the key for the entry->offset. It is needed for fixing entry 
   keys if repair code detects it is wrong. */
errno_t cde40_set_key(place_t *place, uint32_t pos,
		      key_entity_t *key)
{
	void *hash;
	uint32_t pol;
	uint64_t offset;
	uint64_t objectid;
	uint64_t ordering;

	aal_assert("vpf-1228", key != NULL);
	aal_assert("vpf-1229", place != NULL);
	aal_assert("vpf-1230", place->body != NULL);
	
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
static char *cde40_get_name(place_t *place, uint32_t pos,
			    char *buff, uint32_t len)
{
        key_entity_t key;
                                                                                        
        cde40_get_key(place, pos, &key);
                                                                                        
        /* If name is long, we just copy it from the area after
           objectid. Otherwise we extract it from the entry hash. */
        if (plug_call(key.plug->o.key_ops, hashed, &key)) {
		void *objid = cde40_objid(place, pos);
                char *ptr = (char *)(objid + ob_size(cde40_key_pol(place)));
                aal_strncpy(buff, ptr, len);
        } else {
		plug_call(key.plug->o.key_ops, get_name,
			  &key, buff);
        }
                                                                                        
        return buff;
}

#ifndef ENABLE_STAND_ALONE
/* Calculates entry length. This function is widely used in shift code and
   modification code. */
static uint32_t cde40_get_len(place_t *place, uint32_t pos) {
	uint32_t len;
	uint32_t pol;
	key_entity_t key;

	/* Counting objid size */
	pol = cde40_key_pol(place);
	len = ob_size(pol);

	/* Getting entry key */
	cde40_get_key(place, pos, &key);
	
	/* If entry contains long name it is stored just after objectid.
	   Otherwise, entry name is stored in objectid and offset of the
	   entry. This trick saves a lot of space in directories, because the
	   average name is shorter than 15 symbols. */
	if (plug_call(key.plug->o.key_ops, hashed, &key)) {
		void *objid = cde40_objid(place, pos);
		len += aal_strlen((char *)(objid + ob_size(pol))) + 1;
	}
	
	return len;
}
#endif

/* Returns the number of units. */
uint32_t cde40_units(place_t *place) {
	aal_assert("umka-865", place != NULL);
	return cde_get_units(place);
}

/* Reads @count of the entries starting from @pos into passed @buff */
static int32_t cde40_read(place_t *place, void *buff,
			  uint32_t pos, uint32_t count)
{
	uint32_t i;
	entry_hint_t *hint;
    
	aal_assert("umka-866", place != NULL);
	aal_assert("umka-1418", buff != NULL);
	aal_assert("umka-1598", pos < cde40_units(place));
    
	hint = (entry_hint_t *)buff;

#ifndef ENABLE_STAND_ALONE
	{
		uint32_t units = cde40_units(place);
	
		/* Check if count is valid one */
		if (count > units - pos)
			count = units - pos;
	}
#endif

	for (i = pos; i < pos + count; i++, hint++) {
		cde40_get_obj(place, i, &hint->object);
		cde40_get_key(place, i, &hint->offset);

		cde40_get_name(place, i, hint->name,
			       sizeof(hint->name));
	}
    
	return count;
}

/* Returns 1 if items are mergeable, 0 -- otherwise. That is if they belong to
   the same directory. This function is used in shift code from the node plugin
   in order to determine are two items may be merged or not. */
static int cde40_mergeable(place_t *place1, place_t *place2) {
	aal_assert("umka-1581", place1 != NULL);
	aal_assert("umka-1582", place2 != NULL);

	/* Items mergeable if they have the same locality, that is oid of the
	   directory they belong to. */
	return (plug_call(place1->key.plug->o.key_ops,
			  get_locality, &place1->key) ==
		plug_call(place1->key.plug->o.key_ops,
			  get_locality, &place2->key));
}

#ifndef ENABLE_STAND_ALONE
static uint16_t cde40_overhead(place_t *place) {
	return sizeof(cde40_t);
}

/* Estimates how much bytes will be needed to prepare in node in odrer to make
   room for inserting new entries. */
static errno_t cde40_estimate_insert(place_t *place, uint32_t pos,
				     insert_hint_t *hint)
{
	uint32_t i, pol;
	entry_hint_t *entry;
	    
	aal_assert("vpf-095", hint != NULL);
	aal_assert("umka-2229", hint->count > 0);

	entry = (entry_hint_t *)hint->specific;
	
	pol = plug_call(hint->key.plug->o.key_ops,
			bodysize);
	
	hint->len = (hint->count * en_size(pol));
    
	for (i = 0; i < hint->count; i++, entry++) {
		hint->len += ob_size(pol);

		/* Calling key plugin for in odrer to find out is passed name is
		   long one or not. */
		if (plug_call(hint->key.plug->o.key_ops,
			      hashed, &entry->offset))
		{
			/* Okay, name is long, so we need add its length to
			   estimated length. */
			hint->len += aal_strlen(entry->name) + 1;
		}
	}

	/* If the pos we are going to insert new units is MAX_UINT32, we assume
	   it is the attempt to insert new directory item. In this case we
	   should also count item overhead, that is cde40 header which
	   contains the number of entries in item. */
	hint->ohd = (pos == MAX_UINT32 ? cde40_overhead(place) : 0);
	return 0;
}

/* Calculates the size of @count units in passed @place at passed @pos */
uint32_t cde40_size_units(place_t *place, uint32_t pos, uint32_t count) {
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
errno_t cde40_rep(place_t *dst_place, uint32_t dst_pos,
		  place_t *src_place, uint32_t src_pos,
		  uint32_t count)
{
	uint32_t i;
	void *entry;
	uint32_t pol;
	uint32_t size;
	uint32_t offset;
	void *dst, *src;
	uint32_t headers;
	uint32_t dst_units;
	
	aal_assert("umka-2069", dst_place != NULL);
	aal_assert("umka-2070", src_place != NULL);

	pol = cde40_key_pol(dst_place);
	dst_units = cde40_units(dst_place);
	
	aal_assert("umka-2077", dst_pos <= dst_units);
	
	/* Getting offset of body in dst place */
	offset = cde40_size_units(dst_place, 0, dst_pos);
	
	/* Copying entry headers */
	src = src_place->body + sizeof(cde40_t) +
		(src_pos * en_size(pol));

	dst = dst_place->body + sizeof(cde40_t) +
		(dst_pos * en_size(pol));

	headers = count * en_size(pol);
	aal_memcpy(dst, src, headers);

	/* Copying entry bodies */
	src = src_place->body + en_get_offset(src, pol);

	dst = dst_place->body + sizeof(cde40_t) +
		(dst_units * en_size(pol)) + headers + offset;

	size = cde40_size_units(src_place, src_pos, count);
	
	aal_memcpy(dst, src, size);

	/* Updating offset of dst cde */
	entry = cde40_entry(dst_place, dst_pos);

	offset += sizeof(cde40_t) +
		(dst_units * en_size(pol)) + headers;

	for (i = 0; i < count; i++) {
		en_set_offset(entry, offset, pol);
			
		offset += cde40_get_len(src_place,
					src_pos + i);

		entry += en_size(pol);
	}
		
	/* Updating cde units */
	cde_inc_units(dst_place, count);

	/* Updating item key by unit key if the first unit waqs changed. It is
	   needed for correct updating left delimiting keys. */
	if (dst_pos == 0) {
		cde40_get_key(dst_place, 0, &dst_place->key);
	}
	
	place_mkdirty(dst_place);
	return 0;
}

/* Shrinks cde item in order to delete some entries */
static uint32_t cde40_shrink(place_t *place, uint32_t pos,
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
	
	first += cde40_size_units(place, 0, pos);

	/* Getting how many bytes should be moved after passed @pos */
	second = cde40_size_units(place, pos + count,
				  units - (pos + count));

	/* Calculating how many bytes will be moved out */
	remove = cde40_size_units(place, pos, count);

	/* Moving headers and first part of bodies (before passed @pos) */
	entry = cde40_entry(place, pos);
	aal_memmove(entry, entry + (en_size(pol) * count), first);

	/* Setting up the entry offsets */
	entry = cde40_entry(place, 0);
	
	for (i = 0; i < pos; i++) {
		en_dec_offset(entry, headers, pol);
		entry += en_size(pol);
	}

	/* We also move the rest of the data (after insert point) if needed */
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
	
	return 0;
}

/* Prepares cde40 for insert new entries */
uint32_t cde40_expand(place_t *place, uint32_t pos,
		      uint32_t count, uint32_t len)
{
	uint32_t pol;
	uint32_t first;
	uint32_t second;
	uint32_t offset;
	uint32_t headers;
	uint32_t i, units;
	void *entry, *src, *dst;

	aal_assert("umka-1724", len > 0);
	aal_assert("umka-1724", count > 0);
	aal_assert("umka-1723", place != NULL);

	pol = cde40_key_pol(place);
	units = cde_get_units(place);
	headers = count * en_size(pol);

	aal_assert("umka-1722", pos <= units);

	/* Getting the offset of the place new entries will be inserted at. It
	   will be used later in this function. */
	if (units > 0) {
		if (pos < units) {
			entry = cde40_entry(place, pos);
			offset = en_get_offset(entry, pol) + headers;
		} else {
			entry = cde40_entry(place, units - 1);
			offset = en_get_offset(entry, pol) + en_size(pol) +
				cde40_get_len(place, units - 1);
		}
	} else {
		offset = sizeof(cde40_t) + headers;
	}

	/* Calculating length bytes to be moved before insert point */
	first = (units - pos) * en_size(pol);
	first += cde40_size_units(place, 0, pos);
	
	/* Calculating length bytes to be moved after insert point */
	second = cde40_size_units(place, pos, units - pos);
	
	/* Updating offset of entries which lie before insert point */
	entry = cde40_entry(place, 0);
	
	for (i = 0; i < pos; i++) {
		en_inc_offset(entry, headers, pol);
		entry += en_size(pol);
	}
    
	/* Updating offset of entries which lie after insert point */
	entry = cde40_entry(place, pos);
	
	for (i = pos; i < units; i++) {
		en_inc_offset(entry, len, pol);
		entry += en_size(pol);
	}
    
	/* Moving entry bodies if it is needed */
	if (pos < units) {
		src = place->body + offset - headers;
		dst = place->body + offset + len - headers;
		
		aal_memmove(dst, src, second);
	}
    
	/* Moving unit headers if it is needed */
	if (first) {
		src = cde40_entry(place, pos);
		dst = src + headers;
		aal_memmove(dst, src, first);
	}
    
	place_mkdirty(place);
	return offset;
}

/* Predicts how many entries and bytes can be shifted from the @src_item to
   @dst_item. The behavior of the function depends on the passed @hint. */
static errno_t cde40_estimate_shift(place_t *src_place,
				    place_t *dst_place,
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

	pol = cde40_key_pol(src_place);
	src_units = cde40_units(src_place);
	dst_units = dst_place ? cde40_units(dst_place) : 0;

	space = hint->rest;

	/* If hint's create flag is present, we need to create new cde item, so
	   we should count its overhead. */
	if (hint->create) {
		if (space < sizeof(cde40_t))
			return 0;
		
		space -= sizeof(cde40_t);
	}

	flags = hint->control;
	
	curr = (hint->control & SF_LEFT ? 0 : src_units - 1);
	
	check = (src_place->pos.item == hint->pos.item &&
		 hint->pos.unit != MAX_UINT32);

	while (!(hint->result & SF_MOVIP) &&
	       curr < cde40_units(src_place))
	{

		/* Check if we should update unit pos. we will update it if we
		   are at insert point and unit pos is not MAX_UINT32. */
		if (check && (flags & SF_UPTIP)) {
			
			if (!(flags & SF_MOVIP)) {
				if (flags & SF_LEFT) {
					if (hint->pos.unit == 0)
						break;
				} else {
					if (hint->pos.unit == src_units - 1)
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
		if (check && (flags & SF_UPTIP)) {
			if (flags & SF_LEFT) {
				/* Insert point is near to be moved into left
				   neighbour. Checking if we are permitted to do
				   so and updating insert point. */
				if (hint->pos.unit == 0) {
					if (flags & SF_MOVIP) {
						hint->result |= SF_MOVIP;
						hint->pos.unit = dst_units;
					} else
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
						if (flags & SF_MOVIP) {
							hint->result |= SF_MOVIP;
							hint->pos.unit = 0;
						} else {
							break;
						}
					} else {
						if (flags & SF_MOVIP) {
							hint->result |= SF_MOVIP;
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
		hint->units++;

		curr += (flags & SF_LEFT ? 1 : -1);
		space -= (len + en_size(pol));
	}

	/* Updating @hint->rest. It is needed for unit shifting. This value is
	   number of bytes to be moved from @src_place to @dst_place. */
	if (hint->units > 0) {
		hint->rest -= space;
	}
	
	return 0;
}

/* Makes shift of the entries from the @src_place to the @dst_place */
static errno_t cde40_shift(place_t *src_place,
			   place_t *dst_place,
			   shift_hint_t *hint)
{
	uint32_t src_pos, dst_pos;
	uint32_t src_units, dst_units;
	
	aal_assert("umka-1589", hint != NULL);
	aal_assert("umka-1586", src_place != NULL);
	aal_assert("umka-1587", dst_place != NULL);

	src_units = cde_get_units(src_place);
	dst_units = cde_get_units(dst_place);

	if (hint->control & SF_LEFT) {
		src_pos = 0;
		dst_pos = dst_units;
	} else {
		dst_pos = 0;
		src_pos = src_units - hint->units;
	}

	/* Preparing root for copying units into it */
	cde40_expand(dst_place, dst_pos,
		     hint->units, hint->rest);

	/* Copying units from @src place to @dst one */
	cde40_rep(dst_place, dst_pos, src_place,
		  src_pos, hint->units);

	cde40_shrink(src_place, src_pos,
		     hint->units, 0);

	/* Updating item key by first cde key */
	if (cde_get_units(src_place) > 0 &&
	    hint->control & SF_LEFT)
	{
		cde40_get_key(src_place, 0,
			      &src_place->key);
	}

	return 0;
}

/* Inserts new entries to cde item */
static errno_t cde40_insert(place_t *place, uint32_t pos,
			    insert_hint_t *hint)
{
	void *entry;
	uint32_t pol, i, offset;
	entry_hint_t *entry_hint;
    
	aal_assert("umka-792", hint != NULL);
	aal_assert("umka-791", place != NULL);
	aal_assert("umka-897", pos != MAX_UINT32);

	pol = cde40_key_pol(place);
	entry_hint = (entry_hint_t *)hint->specific;

	/* Expanding direntry in order to prepare the room for new entries. The
	   function cde40_expand() returns the offset of where new unit will
	   be inserted at. */
	offset = cde40_expand(place, pos, hint->count, hint->len);
	
	/* Creating new entries */
	entry = cde40_entry(place, pos);
	
	for (i = 0; i < hint->count; i++, entry_hint++)	{
		void *objid;
		uint64_t oid;
		uint64_t off, ord;

		key_entity_t *hash;
		key_entity_t *object;

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
	if (pos == 0) {
		cde40_get_key(place, 0, &place->key);
	}

	place_mkdirty(place);
    
	return 0;
}

/* Removes @count entries at @pos from passed @place */
errno_t cde40_remove(place_t *place, uint32_t pos,
		     remove_hint_t *hint)
{
	uint32_t len;
	uint32_t pol;

	aal_assert("umka-934", place != NULL);
	aal_assert("umka-2400", hint != NULL);

	pol = cde40_key_pol(place);
	len = hint->count * en_size(pol);
	len += cde40_size_units(place, pos, hint->count);
	
	/* Shrinking cde */
	cde40_shrink(place, pos, hint->count, 0);
	
	/* Updating item key */
	if (pos == 0 && cde40_units(place) > 0) {
		cde40_get_key(place, 0, &place->key);
	}

	hint->ohd = (pos == MAX_UINT32 ?
		     cde40_overhead(place) : 0);

	return 0;
}

/* Prepares area new item will be created at */
static errno_t cde40_init(place_t *place) {
	aal_assert("umka-1010", place != NULL);
	aal_assert("umka-2215", place->body != NULL);
	
	((cde40_t *)place->body)->units = 0;
	place_mkdirty(place);
	
	return 0;
}

/* Prints cde item into passed @stream */
static errno_t cde40_print(place_t *place,
			   aal_stream_t *stream,
			   uint16_t options) 
{
	uint32_t pol;
	uint32_t i, j;
	char name[256];
	uint64_t locality;
	uint64_t objectid;
	uint32_t namewidth;
	
	aal_assert("umka-548", place != NULL);
	aal_assert("umka-549", stream != NULL);

	aal_stream_format(stream, "DIRENTRY PLUGIN=%s LEN=%u, KEY=[%s] "
			  "UNITS=%u\n", place->plug->label, place->len, 
			  cde_core->key_ops.print(&place->key, PO_DEF), 
			  cde_get_units(place));
		
	aal_stream_format(stream, "NR  NAME%*s OFFSET HASH%*s "
			  "SDKEY%*s\n", 13, " ", 29, " ", 13, " ");
	
	aal_stream_format(stream, "----------------------------"
			  "------------------------------------"
			  "--------------\n");
	
	/* Loop though the all entries */
	pol = cde40_key_pol(place);
		
	for (i = 0; i < cde_get_units(place); i++) {
		uint64_t offset, haobj;
		void *objid = cde40_objid(place, i);
		void *entry = cde40_entry(place, i);

		cde40_get_name(place, i, name, sizeof(name));

		/* Cutting name by 16 symbols */
		if (aal_strlen(name) > 16) {
			for (j = 0; j < 3; j++)
				name[13 + j] = '.';

			name[13 + j] = '\0';
		}

		locality = ob_get_locality(objid, pol);
		objectid = ob_get_objectid(objid, pol);
		namewidth = 16 - aal_strlen(name) + 1;

		offset = ha_get_offset(entry, pol);
		haobj = ha_get_objectid(entry, pol);
		
		aal_stream_format(stream, "%*d %s%*s %*u %.16llx:%.16llx "
				  "%.7llx:%.7llx\n", 3, i, name, namewidth,
				  " ", 6, en_get_offset(entry, pol), haobj,
				  offset, locality, objectid);
	}

	return 0;
}

/* Returns real maximal key in cde item */
static errno_t cde40_maxreal_key(place_t *place, 
				     key_entity_t *key) 
{
	aal_assert("umka-1651", key != NULL);
	aal_assert("umka-1650", place != NULL);

	return cde40_get_key(place, cde40_units(place) - 1, key);
}

static uint64_t cde40_size(place_t *place) {
	return cde40_units(place);
}

static uint64_t cde40_bytes(place_t *place) {
	aal_assert("vpf-1211", place != NULL);
	return place->len;
}

extern errno_t cde40_merge(place_t *dst, uint32_t dst_pos, 
			   place_t *src, uint32_t src_pos, 
			   merge_hint_t *hint);

extern errno_t cde40_check_struct(place_t *place, uint8_t mode);

extern errno_t cde40_estimate_merge(place_t *dst, uint32_t dst_pos,
				    place_t *src, uint32_t src_pos,
				    merge_hint_t *hint);
#endif

/* Returns maximal possible key in cde item. It is needed for lookuping needed
   entry by entry key. */
errno_t cde40_maxposs_key(place_t *place, 
			  key_entity_t *key) 
{
	key_entity_t *maxkey;

	aal_assert("umka-1649", key != NULL);
	aal_assert("umka-1648", place != NULL);

	plug_call(place->key.plug->o.key_ops,
		  assign, key, &place->key);

	maxkey = plug_call(key->plug->o.key_ops,
			   maximal);
    
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

/* Helper function that is used by lookup method for comparing given key with
   passed entry hash. */
static int callback_comp_entry(void *array, uint32_t pos,
			       void *key, void *data)
{
	key_entity_t curr;

	cde40_get_key((place_t *)data, pos, &curr);

	return plug_call(((place_t *)data)->key.plug->o.key_ops,
			 compfull, &curr, (key_entity_t *)key);
}

/* Performs lookup inside cde. Found pos is stored in @pos */
lookup_res_t cde40_lookup(place_t *place, key_entity_t *key,
			  uint32_t *pos)
{
	int32_t i;
	key_entity_t maxkey;

	aal_assert("umka-610", key != NULL);
	aal_assert("umka-717", key->plug != NULL);
    
	aal_assert("umka-609", place != NULL);
	aal_assert("umka-629", pos != NULL);
    
	/* Getting maximal possible key */
	cde40_maxposs_key(place, &maxkey);

	/* If looked key is greater that maximal possible one then we going out
	   and return FALSE, that is the key not found. */
	if (plug_call(key->plug->o.key_ops, compfull,
		      key, &maxkey) > 0)
	{
		*pos = cde40_units(place);
		return ABSENT;
	}

	/* Comparing looked key with minimal one (that is with item key) */
	if (plug_call(key->plug->o.key_ops, compfull,
		      &place->key, key) > 0)
	{
		*pos = 0;
		return ABSENT;
	}

	/* Performing binary search inside the cde in order to find position of
	   the looked key. */
	switch (aux_bin_search(place->body, cde40_units(place), key,
			       callback_comp_entry, place, pos))
	{
	case 1:
#ifdef ENABLE_COLLISIONS
		/* Making sure, that we have found right unit. This is needed
		   because of possible key collition. We go to left until we
		   find, that we found key smaller than passed one. */
		for (i = *pos - 1; i >= 0; i--) {
			key_entity_t ekey;

			/* Getting entry key */
			cde40_get_key(place, i, &ekey);

			/* Comparing keys. We break the loop when keys as not
			 * equal, that means, that we have found needed pos. */
			if (!plug_call(key->plug->o.key_ops,
				       compfull, key, &ekey))
			{
				*pos = i;
			} else {
				return PRESENT;
			}
		}
#endif
		return PRESENT;
	case 0:
		return ABSENT;
	default:
		return FAILED;
	}
}

static reiser4_item_ops_t cde40_ops = {
#ifndef ENABLE_STAND_ALONE	    
	.init		   = cde40_init,
	.merge		   = cde40_merge,
	.rep		   = cde40_rep,
	.expand		   = cde40_expand,
	.shrink		   = cde40_shrink,
	.insert		   = cde40_insert,
	.remove		   = cde40_remove,
	.overhead          = cde40_overhead,
	.check_struct	   = cde40_check_struct,
	.print		   = cde40_print,
	.shift             = cde40_shift,
	.size		   = cde40_size,
	.bytes		   = cde40_bytes,
	
	.set_key	   = cde40_set_key,
	.maxreal_key       = cde40_maxreal_key,
	.estimate_merge	   = cde40_estimate_merge,
	.estimate_shift    = cde40_estimate_shift,
	.estimate_insert   = cde40_estimate_insert,
	
	.layout		   = NULL,
	.check_layout	   = NULL,
#endif
	.branch            = NULL,
	.plugid		   = NULL,

	.lookup		   = cde40_lookup,
	.units		   = cde40_units,
	.read              = cde40_read,
	.get_key	   = cde40_get_key,
	.mergeable         = cde40_mergeable,
	.maxposs_key	   = cde40_maxposs_key
};

static reiser4_plug_t cde40_plug = {
	.cl    = CLASS_INIT,
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
	cde_core = c;
	return &cde40_plug;
}

plug_register(cde40, cde40_start, NULL);
