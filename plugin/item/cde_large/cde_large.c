/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde_large.c -- reiser4 cde with large keys. */

#ifdef ENABLE_LARGE_KEYS
#include "cde_large.h"

static reiser4_core_t *core = NULL;

/* Returns pointer to the objectid entry component in passed @cde at pased
   @pos. It is used in code bellow. */
static objid_t *cde_large_objid(place_t *place,	uint32_t pos) {
	uint32_t offset = cde_large_body(place)->entry[pos].offset;
	return (objid_t *)(place->body + offset);
}

/* Returns statdata key of the object entry points to */
static void cde_large_get_obj(place_t *place,
			      uint32_t pos,
			      key_entity_t *key)
{
	objid_t *objid = cde_large_objid(place, pos);
	
	key->plug = place->key.plug;
	plug_call(key->plug->o.key_ops, clean, key);
	aal_memcpy(key->body, objid, sizeof(*objid));
}

/* Builds full key by entry components. It is needed for updating keys after
   shift, insert, etc. Also library requires unit keys sometims. */
errno_t cde_large_get_key(place_t *place, uint32_t pos,
			  key_entity_t *key)
{
	oid_t locality;
	entry_t *entry;

	aal_assert("umka-1606", key != NULL);
	aal_assert("umka-1607", place != NULL);
	aal_assert("umka-1605", place->body != NULL);
	
	entry = &cde_large_body(place)->entry[pos];

	/* Getting item key params */
	locality = plug_call(place->key.plug->o.key_ops,
			     get_locality, &place->key);

	/* Building the full key from entry at @pos */
	plug_call(place->key.plug->o.key_ops, build_gener,
		  key, KEY_FILENAME_TYPE, locality,
		  ha_get_ordering(&entry->hash),
		  ha_get_objectid(&entry->hash),
		  ha_get_offset(&entry->hash));

	return 0;
}

/* Extracts entry name from the passed @entry to passed @buff */
static char *cde_large_get_name(place_t *place, uint32_t pos,
				char *buff, uint32_t len)
{
	key_entity_t key;

	cde_large_get_key(place, pos, &key);

	if (plug_call(key.plug->o.key_ops, tall, &key)) {
		char *ptr = (char *)((cde_large_objid(place, pos)) + 1);
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
static uint32_t cde_large_get_len(place_t *place, uint32_t pos) {
	uint32_t len;
	key_entity_t key;

	/* Counting objid size */
	len = sizeof(objid_t);

	/* Getting entry key */
	cde_large_get_key(place, pos, &key);
	
	/* If entry contains long name it is stored just after objectid.
	   Otherwise, entry name is stored in objectid and offset of the
	   entry. This trick saves a lot of space in directories, because the
	   average name is shorter than 15 symbols. */
	if (plug_call(key.plug->o.key_ops, tall, &key)) {
		objid_t *objid = cde_large_objid(place, pos);
		len += aal_strlen((char *)(objid + 1)) + 1;
	}
	
	return len;
}
#endif

/* Returns the number of usets passed cde item contains */
uint32_t cde_large_units(place_t *place) {
	aal_assert("umka-865", place != NULL);
	return de_get_units(cde_large_body(place));
}

/* Reads @count of the entries starting from @pos into passed @buff */
static int32_t cde_large_read(place_t *place, void *buff,
			      uint32_t pos, uint32_t count)
{
	uint32_t i, units;
	entry_hint_t *hint;
    
	aal_assert("umka-866", place != NULL);
	aal_assert("umka-1418", buff != NULL);
	aal_assert("umka-1598", pos < cde_large_units(place));
    
	hint = (entry_hint_t *)buff;

#ifndef ENABLE_STAND_ALONE
	units = cde_large_units(place);
	
	/* Check if count is valid one */
	if (count > units - pos)
		count = units - pos;
#endif

	for (i = pos; i < pos + count; i++, hint++) {
		cde_large_get_obj(place, i, &hint->object);
		cde_large_get_key(place, i, &hint->offset);

		cde_large_get_name(place, i, hint->name,
				   sizeof(hint->name));
	}
    
	return count;
}

static int cde_large_data(place_t *place) {
	return 1;
}

/* Returns TRUE if items are mergeable. That is if they belong to the same
   directory. This function is used in shift code from the node plugin in order
   to determine are two items may be merged or not. */
static int cde_large_mergeable(place_t *place1, place_t *place2) {
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
static uint16_t cde_large_overhead(place_t *place) {
	return sizeof(cde_large_t);
}

/* Estimates how much bytes will be needed to prepare in node in odrer to make
   room for inserting new entries. */
static errno_t cde_large_estimate_insert(place_t *place,
					 create_hint_t *hint,
					 uint32_t pos)
{
	uint32_t i;
	entry_hint_t *entry;
	    
	aal_assert("vpf-095", hint != NULL);
	aal_assert("umka-2229", hint->count > 0);
    
	entry = (entry_hint_t *)hint->type_specific;
	hint->len = hint->count * sizeof(entry_t);
    
	for (i = 0; i < hint->count; i++, entry++) {
		hint->len += sizeof(objid_t);

		/* Calling key plugin for in odrer to find out is passed name is
		   long one or not. */
		if (plug_call(hint->key.plug->o.key_ops,
			      tall, &entry->offset))
		{
			/* Okay, name is long, so we need add its length to
			   estimated length. */
			hint->len += aal_strlen(entry->name) + 1;
		}
	}

	/* If the pos we are going to insert new units is MAX_UINT32, we assume
	   it is the attempt to insert new directory item. In this case we
	   should also count item overhead, that is cde_large header which
	   contains the number of entries in item. */
	if (pos == MAX_UINT32)
		hint->len += cde_large_overhead(place);
    
	return 0;
}

/* Calculates the size of @count units in passed @place at passed @pos */
uint32_t cde_large_size_units(place_t *place, uint32_t pos, uint32_t count) {
	uint32_t size = 0;
	cde_large_t *cde;
	entry_t *entry_end;
	entry_t *entry_start;

	if (count == 0)
		return 0;
	
	cde = cde_large_body(place);
	entry_start = &cde->entry[pos];

	if (pos + count < de_get_units(cde)) {
		entry_end = &cde->entry[pos + count];
	} else {
		entry_end = &cde->entry[pos + count - 1];
		size = cde_large_get_len(place, pos + count - 1);

	}
	
	size += (en_get_offset(entry_end) -
		 en_get_offset(entry_start));

	return size;
}

/* Makes copy of @count amount of units from @src_place to @dst_place */
errno_t cde_large_rep(place_t *dst_place, uint32_t dst_pos,
		      place_t *src_place, uint32_t src_pos,
		      uint32_t count)
{
	uint32_t i;
	uint32_t size;
	uint32_t offset;
	void *dst, *src;
	uint32_t headers;
	uint32_t dst_units;

	entry_t *entry;
	cde_large_t *dst_cde;
	cde_large_t *src_cde;
	
	aal_assert("umka-2069", dst_place != NULL);
	aal_assert("umka-2070", src_place != NULL);

	dst_cde = cde_large_body(dst_place);
	src_cde = cde_large_body(src_place);

	dst_units = cde_large_units(dst_place);
	aal_assert("umka-2077", dst_pos <= dst_units);
	
	/* Getting offset of body in dst place */
	offset = cde_large_size_units(dst_place, 0, dst_pos);
	
	/* Copying entry headers */
	src = (void *)src_cde + sizeof(cde_large_t) +
		(src_pos * sizeof(entry_t));

	dst = (void *)dst_cde + sizeof(cde_large_t) +
		(dst_pos * sizeof(entry_t));

	headers = count * sizeof(entry_t);
		
	aal_memcpy(dst, src, headers);

	/* Copying entry bodies */
	src = (void *)src_cde +
		en_get_offset((entry_t *)src);

	dst = (void *)dst_cde + sizeof(cde_large_t) +
		(dst_units * sizeof(entry_t)) + headers + offset;

	size = cde_large_size_units(src_place, src_pos, count);
	
	aal_memcpy(dst, src, size);

	/* Updating offset of dst cde */
	entry = &dst_cde->entry[dst_pos];

	offset += sizeof(cde_large_t) +
		(dst_units * sizeof(entry_t)) + headers;

	for (i = 0; i < count; i++, entry++) {
		en_set_offset(entry, offset);
		offset += cde_large_get_len(src_place, src_pos + i);
	}
		
	/* Updating cde count field */
	de_inc_units(dst_cde, count);

	/* Updating item key by unit key if the first unit was changed. It is
	   needed for correct updating left delimiting keys. */
	if (dst_pos == 0)
		cde_large_get_key(dst_place, 0, &dst_place->key);
	
	return 0;
}

/* Shrinks cde item in order to delete some entries */
static uint32_t cde_large_shrink(place_t *place, uint32_t pos,
				 uint32_t count, uint32_t len)
{
	uint32_t first;
	uint32_t second;
	uint32_t remove;
	uint32_t headers;
	uint32_t i, units;

	entry_t *entry;
	cde_large_t *cde;

	aal_assert("umka-1959", place != NULL);
	
	cde = cde_large_body(place);
	units = de_get_units(cde);
	
	aal_assert("umka-1681", pos < units);

	if (pos + count > units)
		count = units - pos;

	if (count == 0)
		return 0;

	headers = count * sizeof(entry_t);
	
	/* Getting how many bytes should be moved before passed @pos */
	first = (units - (pos + count)) *
		sizeof(entry_t);
	
	first += cde_large_size_units(place, 0, pos);

	/* Getting how many bytes should be moved after passed @pos */
	second = cde_large_size_units(place, pos + count,
				      units - (pos + count));

	/* Calculating how many bytes will be moved out */
	remove = cde_large_size_units(place, pos, count);

	/* Moving headers and first part of bodies (before passed @pos) */
	entry = &cde->entry[pos];
	aal_memmove(entry, entry + count, first);

	/* Setting up the entry offsets */
	entry = &cde->entry[0];
	
	for (i = 0; i < pos; i++, entry++)
		en_dec_offset(entry, headers);

	/* We also move the rest of the data (after insert point) if needed */
	if (second > 0) {
		void *src, *dst;

		entry = &cde->entry[pos];

		src = (void *)cde +
			en_get_offset(entry);
		
		dst = src - (headers + remove);
		
		aal_memmove(dst, src, second);

		/* Setting up entry offsets */
		for (i = pos; i < units - count; i++) {
			entry = &cde->entry[i];
			en_dec_offset(entry, (headers + remove));
		}
	}

	de_dec_units(cde, count);
	return 0;
}

/* Prepares cde_large for insert new entries */
uint32_t cde_large_expand(place_t *place, uint32_t pos,
			  uint32_t count, uint32_t len)
{
	void *src, *dst;
	entry_t *entry;

	uint32_t first;
	uint32_t second;
	uint32_t offset;
	uint32_t headers;
	uint32_t i, units;

	cde_large_t *cde;

	aal_assert("umka-1724", len > 0);
	aal_assert("umka-1724", count > 0);
	aal_assert("umka-1723", place != NULL);

	cde = cde_large_body(place);
	units = de_get_units(cde);
	headers = count * sizeof(entry_t);

	aal_assert("umka-1722", pos <= units);

	/* Getting the offset of the place new entries will be inserted at. It
	   will be used later in this function. */
	if (units > 0) {
		if (pos < units) {
			entry = &cde->entry[pos];
			offset = en_get_offset(entry) + headers;
		} else {
			entry = &cde->entry[units - 1];
			
			offset = en_get_offset(entry) + sizeof(entry_t) +
				cde_large_get_len(place, units - 1);
		}
	} else
		offset = sizeof(cde_large_t) + headers;

	/* Calculating length bytes to be moved before insert point */
	first = (units - pos) * sizeof(entry_t);
	first += cde_large_size_units(place, 0, pos);
	
	/* Calculating length bytes to be moved after insert point */
	second = cde_large_size_units(place, pos, units - pos);
	
	/* Updating offset of entries which lie before insert point */
	entry = &cde->entry[0];
	
	for (i = 0; i < pos; i++, entry++)
		en_inc_offset(entry, headers);
    
	/* Updating offset of entries which lie after insert point */
	entry = &cde->entry[pos];
	
	for (i = pos; i < units; i++, entry++)
		en_inc_offset(entry, len);
    
	/* Moving entry bodies if it is needed */
	if (pos < units) {
		src = (void *)cde + offset -
			headers;
		
		dst = (void *)cde + offset +
			len - headers;
		
		aal_memmove(dst, src, second);
	}
    
	/* Moving unit headers if it is needed */
	if (first) {
		src = &cde->entry[pos];
		dst = src + headers;
		aal_memmove(dst, src, first);
	}
    
	return offset;
}

/* Predicts how many entries and bytes can be shifted from the @src_place to
   @dst_place. The behavior of the function depends on the passed @hint. */
static errno_t cde_large_estimate_shift(place_t *src_place,
					place_t *dst_place,
					shift_hint_t *hint)
{
	int check;
	uint32_t curr;
	uint32_t flags;
	uint32_t src_units;
	uint32_t dst_units;
	uint32_t space, len;
	
	aal_assert("umka-1592", hint != NULL);
	aal_assert("umka-1591", src_place != NULL);

	src_units = cde_large_units(src_place);
	dst_units = dst_place ? cde_large_units(dst_place) : 0;

	space = hint->rest;

	/* If hint's create flag is present, we need to create new cde
	   item, so we should count its overhead. */
	if (hint->create) {
		if (space < sizeof(cde_large_t))
			return 0;
		
		space -= sizeof(cde_large_t);
	}

	flags = hint->control;
	
	curr = (hint->control & SF_LEFT ? 0 : src_units - 1);
	
	check = (src_place->pos.item == hint->pos.item &&
		 hint->pos.unit != MAX_UINT32);

	while (!(hint->result & SF_MOVIP) &&
	       curr < cde_large_units(src_place))
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
		len = cde_large_get_len(src_place, curr);

		if (space < len + sizeof(entry_t))
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
				} else
					hint->pos.unit--;
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
						} else
							break;
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
		space -= (len + sizeof(entry_t));
	}

	/* Updating @hint->rest. It is needed for unit shifting. This value is
	   number of bytes to be moved from @src_place to @dst_place. */
	if (hint->units > 0)
		hint->rest -= space;
	
	return 0;
}

/* Makes shift of the entries from the @src_place to the @dst_place */
static errno_t cde_large_shift(place_t *src_place,
			       place_t *dst_place,
			       shift_hint_t *hint)
{
	uint32_t src_pos, dst_pos;
	cde_large_t *src_cde;
	cde_large_t *dst_cde;
	uint32_t src_units, dst_units;
	
	aal_assert("umka-1589", hint != NULL);
	aal_assert("umka-1586", src_place != NULL);
	aal_assert("umka-1587", dst_place != NULL);

	src_cde = cde_large_body(src_place);
	dst_cde = cde_large_body(dst_place);

	src_units = de_get_units(src_cde);
	dst_units = de_get_units(dst_cde);

	if (hint->control & SF_LEFT) {
		src_pos = 0;
		dst_pos = dst_units;
	} else {
		dst_pos = 0;
		src_pos = src_units - hint->units;
	}

	/* Preparing root for copying units into it */
	cde_large_expand(dst_place, dst_pos,
			  hint->units, hint->rest);

	/* Copying units from @src item to @dst one */
	cde_large_rep(dst_place, dst_pos, src_place,
		      src_pos, hint->units);

	cde_large_shrink(src_place, src_pos,
			 hint->units, 0);

	/* Updating item key by first cde key */
	if (de_get_units(src_cde) > 0 &&
	    hint->control & SF_LEFT)
	{
		cde_large_get_key(src_place, 0,
				  &src_place->key);
	}

	return 0;
}

/* Inserts new entries to cde item */
static errno_t cde_large_insert(place_t *place,
				create_hint_t *hint,
				uint32_t pos)
{
	entry_t *entry;
	uint32_t i, offset;

	cde_large_t *cde;
	entry_hint_t *entry_hint;
    
	aal_assert("umka-792", hint != NULL);
	aal_assert("umka-791", place != NULL);
	aal_assert("umka-897", pos != MAX_UINT32);

	cde = cde_large_body(place);
	entry_hint = (entry_hint_t *)hint->type_specific;

	/* Expanding cde in order to prepare the room for new entries. The
	   function cde_large_expand returns the offset of where new unit will
	   be inserted at. */
	offset = cde_large_expand(place, pos, hint->count,
				  hint->len);
	
	/* Creating new entries */
	for (i = 0, entry = &cde->entry[pos];
	     i < hint->count; i++, entry++, entry_hint++)
	{
		hash_t *entid;
		objid_t *objid;

		uint64_t oid, loc;
		uint64_t ord, off;
		key_entity_t *hash;
		key_entity_t *object;

		entid = (hash_t *)&entry->hash;

		objid = (objid_t *)((void *)cde +
				    offset);
		
		/* Setting up the offset of new entry */
		en_set_offset(entry, offset);

		/* Setting up all hash components */
		hash = &entry_hint->offset;
		
		/* Creating proper entry ordering */
		ord = plug_call(hash->plug->o.key_ops,
				get_ordering, hash);
		
		ha_set_ordering(entid, ord);
		
		/* Creating proper entry hash */
		oid = plug_call(hash->plug->o.key_ops,
				get_fobjectid, hash);
		
		ha_set_objectid(entid, oid);

		/* Setting up offset component */
		off = plug_call(hash->plug->o.key_ops,
				get_offset, hash);

		ha_set_offset(entid, off);

		/* Setting up all objid components */
		object = &entry_hint->object;
		
		aal_memcpy(objid, object->body,
			   sizeof(*objid));

		offset += sizeof(objid_t);

		/* If key is long one we also count name length */
		if (plug_call(place->key.plug->o.key_ops,
			      tall, &entry_hint->offset))
		{
			uint32_t len = aal_strlen(entry_hint->name);

			aal_memcpy((void *)cde + offset,
				   entry_hint->name, len);

			offset += len;
			*((char *)cde + offset) = '\0';
			offset++;
		}
	}
	
	de_inc_units(cde, hint->count);
	
	/* Updating item key by unit key if the first unit was changed. It is
	   needed for correct updating left delimiting keys. */
	if (pos == 0)
		cde_large_get_key(place, 0, &place->key);
    
	return 0;
}

/* Removes @count entries at @pos from passed @place */
int32_t cde_large_remove(place_t *place, uint32_t pos,
			 uint32_t count)
{
	uint32_t len;

	aal_assert("umka-934", place != NULL);

	len = count * sizeof(entry_t);
	len += cde_large_size_units(place, pos, count);
	
	/* Shrinking cde */
	cde_large_shrink(place, pos, count, 0);
	
	/* Updating item key */
	if (pos == 0 && cde_large_units(place) > 0)
		cde_large_get_key(place, 0, &place->key);

	return len;
}

/* Prepares area new item will be created at */
static errno_t cde_large_init(place_t *place) {
	aal_assert("umka-1010", place != NULL);
	aal_assert("umka-2215", place->body != NULL);
	
	((cde_large_t *)place->body)->units = 0;
	return 0;
}

/* Prints cde item into passed @stream */
static errno_t cde_large_print(place_t *place,
			       aal_stream_t *stream,
			       uint16_t options) 
{
	uint32_t i, j;
	char name[256];
	uint32_t namewidth;
	cde_large_t *cde;
	uint64_t locality, objectid;
	
	aal_assert("umka-548", place != NULL);
	aal_assert("umka-549", stream != NULL);

	cde = cde_large_body(place);
	
	aal_stream_format(stream, "CDE PLUGIN=%s LEN=%u, KEY=[%s] UNITS=%u\n",
			  place->plug->label, place->len, 
			  core->key_ops.print_key(&place->key, 0), 
			  de_get_units(cde));
		
	aal_stream_format(stream, "NR  NAME%*s OFFSET HASH%*s "
			  "SDKEY%*s\n", 13, " ", 29, " ", 13, " ");
	
	aal_stream_format(stream, "----------------------------"
			  "------------------------------------"
			  "--------------\n");
	
	/* Loop though the all entries */
	for (i = 0; i < de_get_units(cde); i++) {
		uint64_t offset, haobjectid;
		entry_t *entry = &cde->entry[i];
		objid_t *objid = cde_large_objid(place, i);

		cde_large_get_name(place, i, name,
				   sizeof(name));

		/* Cutting name by 16 symbols */
		if (aal_strlen(name) > 16) {
			for (j = 0; j < 3; j++)
				name[14 + j] = '.';

			name[14 + j] = '\0';
		}

		locality = ob_get_locality(objid);
		objectid = ob_get_objectid(objid);
		
		namewidth = aal_strlen(name) < 16 ? 16 -
			aal_strlen(name) + 1 : 1;

		offset = ha_get_offset(&entry->hash);
		haobjectid = ha_get_objectid(&entry->hash);
		
		aal_stream_format(stream, "%*d %s%*s %*u %.16llx:%.16llx "
				  "%.7llx:%.7llx\n", 3, i, name, namewidth,
				  " ", 6, entry->offset, haobjectid, offset,
				  locality, objectid);
	}

	return 0;
}

/* Returns real maximal key in cde item */
static errno_t cde_large_maxreal_key(place_t *place, 
				     key_entity_t *key) 
{
	uint32_t units;
	
	aal_assert("umka-1651", key != NULL);
	aal_assert("umka-1650", place != NULL);

	units = cde_large_units(place);
	return cde_large_get_key(place, units - 1, key);
}

static uint64_t cde_large_size(place_t *place) {
	return cde_large_units(place);
}

static uint64_t cde_large_bytes(place_t *place) {
	aal_assert("vpf-1212", place != NULL);
	
	return place->len;
}

extern errno_t cde_large_copy(place_t *dst,
			      uint32_t dst_pos, 
			      place_t *src,
			      uint32_t src_pos, 
			      copy_hint_t *hint);

extern errno_t cde_large_check_struct(place_t *place,
				      uint8_t mode);

extern errno_t cde_large_estimate_copy(place_t *dst,
				       uint32_t dst_pos,
				       place_t *src,
				       uint32_t src_pos,
				       copy_hint_t *hint);
#endif

/* Returns maximal possible key in cde item. It is needed for lookuping
   needed entry by entry key. */
errno_t cde_large_maxposs_key(place_t *place, 
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

	cde_large_get_key((place_t *)data, pos, &curr);

	return plug_call(((place_t *)data)->key.plug->o.key_ops,
			 compfull, &curr, (key_entity_t *)key);
}

/* Performs lookup inside cde. Found pos is stored in @pos */
lookup_t cde_large_lookup(place_t *place,
			   key_entity_t *key,
			   uint32_t *pos)
{
	lookup_t res;
	key_entity_t maxkey;

	aal_assert("umka-610", key != NULL);
	aal_assert("umka-717", key->plug != NULL);
    
	aal_assert("umka-609", place != NULL);
	aal_assert("umka-629", pos != NULL);
    
	/* Getting maximal possible key */
	cde_large_maxposs_key(place, &maxkey);

	/* If looked key is greater that maximal possible one then we going out
	   and return FALSE, that is the key not found. */
	if (plug_call(key->plug->o.key_ops, compfull,
		      key, &maxkey) > 0)
	{
		*pos = cde_large_units(place);
		return ABSENT;
	}

	/* Comparing looked key with minimal one (that is with item key) */
	if (plug_call(key->plug->o.key_ops, compfull,
		      &place->key, key) > 0)
	{
		*pos = 0;
		return ABSENT;
	}

	/* Performing binary search inside the cde in order to find
	   position of the looked key. */
	switch (aux_bin_search(place->body, cde_large_units(place), key,
			       callback_comp_entry, (void *)place, pos))
	{
	case 1:
		return PRESENT;
	case 0:
		return ABSENT;
	default:
		return FAILED;
	}
}

static reiser4_item_ops_t cde_large_ops = {
#ifndef ENABLE_STAND_ALONE	    
	.init		   = cde_large_init,
	.copy		   = cde_large_copy,
	.rep		   = cde_large_rep,
	.expand		   = cde_large_expand,
	.shrink		   = cde_large_shrink,
	.insert		   = cde_large_insert,
	.remove		   = cde_large_remove,
	.overhead          = cde_large_overhead,
	.check_struct	   = cde_large_check_struct,
	.print		   = cde_large_print,
	.shift             = cde_large_shift,
	.maxreal_key       = cde_large_maxreal_key,
	
	.estimate_copy	   = cde_large_estimate_copy,
	.estimate_shift    = cde_large_estimate_shift,
	.estimate_insert   = cde_large_estimate_insert,
		
	.set_key	   = NULL,
	.layout		   = NULL,
	.check_layout	   = NULL,
	.get_plugid	   = NULL,
	
	.size		   = cde_large_size,
	.bytes		   = cde_large_bytes,
#endif
	.branch            = NULL,

	.data		   = cde_large_data,
	.lookup		   = cde_large_lookup,
	.units		   = cde_large_units,
	.read              = cde_large_read,
	.get_key	   = cde_large_get_key,
	.mergeable         = cde_large_mergeable,
	.maxposs_key	   = cde_large_maxposs_key
};

static reiser4_plug_t cde_large_plug = {
	.cl    = CLASS_INIT,
	.id    = {ITEM_CDE_LARGE_ID, DIRENTRY_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "cde_large",
	.desc  = "Compound direntry for reiser4, ver. " VERSION,
#endif
	.o = {
		.item_ops = &cde_large_ops
	}
};

static reiser4_plug_t *cde_large_start(reiser4_core_t *c) {
	core = c;
	return &cde_large_plug;
}

plug_register(cde_large, cde_large_start, NULL);
#endif
