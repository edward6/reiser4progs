/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde_short.c -- reiser4 directory entry with short keys. */

#ifdef ENABLE_SHORT_KEYS
#include "cde_short.h"

/* Returns pointer to the objectid entry component in passed @direntry at pased
   @pos. It is used in code bellow. */
static objid_t *cde_short_objid(item_entity_t *item,
				uint32_t pos)
{
	uint32_t offset = cde_short_body(item)->entry[pos].offset;
	return (objid_t *)(item->body + offset);
}

/* Returns statdata key of the object entry points to */
static void cde_short_get_obj(item_entity_t *item,
			       uint32_t pos,
			       key_entity_t *key)
{
	objid_t *objid = cde_short_objid(item, pos);
	
	key->plugin = item->key.plugin;
	plugin_call(key->plugin->o.key_ops, clean, key);
	aal_memcpy(key->body, objid, sizeof(*objid));
}

/* Builds full key by entry components. It is needed for updating keys after
   shift, insert, etc. Also library requires unit keys sometims. */
errno_t cde_short_get_key(item_entity_t *item,
			   uint32_t pos,
			   key_entity_t *key)
{
	oid_t locality;
	entry_t *entry;

	aal_assert("umka-1606", key != NULL);
	aal_assert("umka-1607", item != NULL);
	aal_assert("umka-1605", item->body != NULL);
	
	entry = &cde_short_body(item)->entry[pos];

	/* Getting item key params */
	locality = plugin_call(item->key.plugin->o.key_ops,
			       get_locality, &item->key);

	/* Building the full key from entry at @pos */
	plugin_call(item->key.plugin->o.key_ops, build_gener,
		    key, KEY_FILENAME_TYPE, locality, 0,
		    ha_get_objectid(&entry->hash),
		    ha_get_offset(&entry->hash));

	return 0;
}

/* Extracts entry name from the passed @entry to passed @buff */
static char *cde_short_get_name(item_entity_t *item,
				 uint32_t pos, char *buff,
				 uint32_t len)
{
        key_entity_t key;
                                                                                        
        cde_short_get_key(item, pos, &key);
                                                                                        
        /* If name is long, we just copy it from the area after
           objectid. Otherwise we extract it from the entry hash. */
        if (plugin_call(key.plugin->o.key_ops, tall, &key)) {
                char *ptr = (char *)((cde_short_objid(item, pos)) + 1);
                aal_strncpy(buff, ptr, len);
        } else {
		plugin_call(key.plugin->o.key_ops, get_name,
			    &key, buff);
        }
                                                                                        
        return buff;
}

#ifndef ENABLE_STAND_ALONE
/* Calculates entry length. This function is widely used in shift code and
   modification code. */
static uint32_t cde_short_get_len(item_entity_t *item,
				   uint32_t pos)
{
	uint32_t len;
	key_entity_t key;

	/* Counting objid size */
	len = sizeof(objid_t);

	/* Getting entry key */
	cde_short_get_key(item, pos, &key);
	
	/* If entry contains long name it is stored just after objectid.
	   Otherwise, entry name is stored in objectid and offset of the
	   entry. This trick saves a lot of space in directories, because the
	   average name is shorter than 15 symbols. */
	if (plugin_call(key.plugin->o.key_ops, tall, &key)) {
		objid_t *objid = cde_short_objid(item, pos);
		len += aal_strlen((char *)(objid + 1)) + 1;
	}
	
	return len;
}
#endif

/* Returns the number of usets passed direntry item contains */
uint32_t cde_short_units(item_entity_t *item) {
	aal_assert("umka-865", item != NULL);
	return de_get_units(cde_short_body(item));
}

/* Reads @count of the entries starting from @pos into passed @buff */
static int32_t cde_short_read(item_entity_t *item, void *buff,
			       uint32_t pos, uint32_t count)
{
	uint32_t i, units;
	entry_hint_t *hint;
    
	aal_assert("umka-866", item != NULL);
	aal_assert("umka-1418", buff != NULL);

	aal_assert("umka-1598",
		   pos < cde_short_units(item));
    
	hint = (entry_hint_t *)buff;

#ifndef ENABLE_STAND_ALONE
	units = cde_short_units(item);
	
	/* Check if count is valid one */
	if (count > units - pos)
		count = units - pos;
#endif

	for (i = pos; i < pos + count; i++, hint++) {
		cde_short_get_obj(item, i, &hint->object);
		cde_short_get_key(item, i, &hint->offset);

		cde_short_get_name(item, i, hint->name,
				    sizeof(hint->name));
	}
    
	return count;
}

static int cde_short_data(void) {
	return 1;
}

/* Returns 1 if items are mergeable, 0 -- otherwise. That is if they belong to
   the same directory. This function is used in shift code from the node plugin
   in order to determine are two items may be merged or not. */
static int cde_short_mergeable(item_entity_t *item1,
			       item_entity_t *item2)
{
	aal_assert("umka-1581", item1 != NULL);
	aal_assert("umka-1582", item2 != NULL);

	/* Items mergeable if they have the same locality, that is oid of the
	   directory they belong to. */
	return (plugin_call(item1->key.plugin->o.key_ops,
			    get_locality, &item1->key) ==
		plugin_call(item1->key.plugin->o.key_ops,
			    get_locality, &item2->key));
}

#ifndef ENABLE_STAND_ALONE
static uint16_t cde_short_overhead(item_entity_t *item) {
	return sizeof(cde_short_t);
}

/* Estimates how much bytes will be needed to prepare in node in odrer to make
   room for inserting new entries. */
static errno_t cde_short_estimate_insert(item_entity_t *item,
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
		if (plugin_call(hint->key.plugin->o.key_ops,
				tall, &entry->offset))
		{
			/* Okay, name is long, so we need add its length to
			   estimated length. */
			hint->len += aal_strlen(entry->name) + 1;
		}
	}

	/* If the pos we are going to insert new units is ~0ul, we assume it is
	   the attempt to insert new directory item. In this case we should also
	   count item overhead, that is cde_short header which contains the
	   number of entries in item. */
	if (pos == ~0ul)
		hint->len += cde_short_overhead(item);
    
	return 0;
}

/* Calculates the size of @count units in passed @item at passed @pos */
uint32_t cde_short_size(item_entity_t *item,
			uint32_t pos, uint32_t count)
{
	uint32_t size = 0;
	entry_t *entry_end;
	entry_t *entry_start;
	cde_short_t *direntry;

	if (count == 0)
		return 0;
	
	direntry = cde_short_body(item);
	entry_start = &direntry->entry[pos];

	if (pos + count < de_get_units(direntry)) {
		entry_end = &direntry->entry[pos + count];
	} else {
		entry_end = &direntry->entry[pos + count - 1];
		size = cde_short_get_len(item, pos + count - 1);

	}
	
	size += (en_get_offset(entry_end) -
		 en_get_offset(entry_start));

	return size;
}

/* Makes copy of @count amount of units from @src_item to @dst_one */
errno_t cde_short_rep(item_entity_t *dst_item, uint32_t dst_pos,
		      item_entity_t *src_item, uint32_t src_pos,
		      uint32_t count)
{
	uint32_t i;
	uint32_t size;
	uint32_t offset;
	void *dst, *src;
	uint32_t headers;
	uint32_t dst_units;

	entry_t *entry;
	cde_short_t *dst_direntry;
	cde_short_t *src_direntry;
	
	aal_assert("umka-2069", dst_item != NULL);
	aal_assert("umka-2070", src_item != NULL);

	dst_direntry = cde_short_body(dst_item);
	src_direntry = cde_short_body(src_item);

	dst_units = cde_short_units(dst_item);
	aal_assert("umka-2077", dst_pos <= dst_units);
	
	/* Getting offset of body in dst item */
	offset = cde_short_size(dst_item, 0, dst_pos);
	
	/* Copying entry headers */
	src = (void *)src_direntry + sizeof(cde_short_t) +
		(src_pos * sizeof(entry_t));

	dst = (void *)dst_direntry + sizeof(cde_short_t) +
		(dst_pos * sizeof(entry_t));

	headers = count * sizeof(entry_t);
		
	aal_memcpy(dst, src, headers);

	/* Copying entry bodies */
	src = (void *)src_direntry +
		en_get_offset((entry_t *)src);

	dst = (void *)dst_direntry + sizeof(cde_short_t) +
		(dst_units * sizeof(entry_t)) + headers + offset;

	size = cde_short_size(src_item, src_pos, count);
	
	aal_memcpy(dst, src, size);

	/* Updating offset of dst direntry */
	entry = &dst_direntry->entry[dst_pos];

	offset += sizeof(cde_short_t) +
		(dst_units * sizeof(entry_t)) + headers;

	for (i = 0; i < count; i++, entry++) {
		en_set_offset(entry, offset);
			
		offset += cde_short_get_len(src_item,
					     src_pos + i);
	}
		
	/* Updating direntry count field */
	de_inc_units(dst_direntry, count);

	/* Updating item key by unit key if the first unit waqs changed. It is
	   needed for correct updating left delimiting keys. */
	if (dst_pos == 0)
		cde_short_get_key(dst_item, 0, &dst_item->key);
	
	return 0;
}

/* Shrinks direntry item in order to delete some entries */
static uint32_t cde_short_shrink(item_entity_t *item, uint32_t pos,
				 uint32_t count, uint32_t len)
{
	uint32_t first;
	uint32_t second;
	uint32_t remove;
	uint32_t headers;
	uint32_t i, units;

	entry_t *entry;
	cde_short_t *direntry;

	aal_assert("umka-1959", item != NULL);
	
	direntry = cde_short_body(item);
	units = de_get_units(direntry);
	
	aal_assert("umka-1681", pos < units);

	if (pos + count > units)
		count = units - pos;

	if (count == 0)
		return 0;

	headers = count * sizeof(entry_t);
	
	/* Getting how many bytes should be moved before passed @pos */
	first = (units - (pos + count)) *
		sizeof(entry_t);
	
	first += cde_short_size(item, 0, pos);

	/* Getting how many bytes should be moved after passed @pos */
	second = cde_short_size(item, pos + count,
				 units - (pos + count));

	/* Calculating how many bytes will be moved out */
	remove = cde_short_size(item, pos, count);

	/* Moving headers and first part of bodies (before passed @pos) */
	entry = &direntry->entry[pos];
	aal_memmove(entry, entry + count, first);

	/* Setting up the entry offsets */
	entry = &direntry->entry[0];
	
	for (i = 0; i < pos; i++, entry++)
		en_dec_offset(entry, headers);

	/* We also move the rest of the data (after insert point) if needed */
	if (second > 0) {
		void *src, *dst;

		entry = &direntry->entry[pos];

		src = (void *)direntry +
			en_get_offset(entry);
		
		dst = src - (headers + remove);
		
		aal_memmove(dst, src, second);

		/* Setting up entry offsets */
		for (i = pos; i < units - count; i++) {
			entry = &direntry->entry[i];
			en_dec_offset(entry, (headers + remove));
		}
	}

	de_dec_units(direntry, count);
	return 0;
}

/* Prepares cde_short for insert new entries */
uint32_t cde_short_expand(item_entity_t *item, uint32_t pos,
			  uint32_t count, uint32_t len)
{
	void *src, *dst;
	entry_t *entry;

	uint32_t first;
	uint32_t second;
	uint32_t offset;
	uint32_t headers;
	uint32_t i, units;

	cde_short_t *direntry;

	aal_assert("umka-1724", len > 0);
	aal_assert("umka-1724", count > 0);
	aal_assert("umka-1723", item != NULL);

	direntry = cde_short_body(item);
	units = de_get_units(direntry);
	headers = count * sizeof(entry_t);

	aal_assert("umka-1722", pos <= units);

	/* Getting the offset of the place new entries will be inserted at. It
	   will be used later in this function. */
	if (units > 0) {
		if (pos < units) {
			entry = &direntry->entry[pos];
			offset = en_get_offset(entry) + headers;
		} else {
			entry = &direntry->entry[units - 1];
			
			offset = en_get_offset(entry) + sizeof(entry_t) +
				cde_short_get_len(item, units - 1);
		}
	} else
		offset = sizeof(cde_short_t) + headers;

	/* Calculating length bytes to be moved before insert point */
	first = (units - pos) * sizeof(entry_t);
	first += cde_short_size(item, 0, pos);
	
	/* Calculating length bytes to be moved after insert point */
	second = cde_short_size(item, pos, units - pos);
	
	/* Updating offset of entries which lie before insert point */
	entry = &direntry->entry[0];
	
	for (i = 0; i < pos; i++, entry++)
		en_inc_offset(entry, headers);
    
	/* Updating offset of entries which lie after insert point */
	entry = &direntry->entry[pos];
	
	for (i = pos; i < units; i++, entry++)
		en_inc_offset(entry, len);
    
	/* Moving entry bodies if it is needed */
	if (pos < units) {
		src = (void *)direntry + offset -
			headers;
		
		dst = (void *)direntry + offset +
			len - headers;
		
		aal_memmove(dst, src, second);
	}
    
	/* Moving unit headers if it is needed */
	if (first) {
		src = &direntry->entry[pos];
		dst = src + headers;
		aal_memmove(dst, src, first);
	}
    
	return offset;
}

/* Predicts how many entries and bytes can be shifted from the @src_item to
   @dst_item. The behavior of the function depends on the passed @hint. */
static errno_t cde_short_estimate_shift(item_entity_t *src_item,
					item_entity_t *dst_item,
					shift_hint_t *hint)
{
	int check;
	uint32_t curr;
	uint32_t flags;
	uint32_t src_units;
	uint32_t dst_units;
	uint32_t space, len;
	
	aal_assert("umka-1591", src_item != NULL);
	aal_assert("umka-1592", hint != NULL);

	src_units = cde_short_units(src_item);
	dst_units = dst_item ? cde_short_units(dst_item) : 0;

	space = hint->rest;

	/* If hint's create flag is present, we need to create new direntry
	   item, so we should count its overhead. */
	if (hint->create) {
		if (space < sizeof(cde_short_t))
			return 0;
		
		space -= sizeof(cde_short_t);
	}

	flags = hint->control;
	
	curr = (hint->control & SF_LEFT ? 0 : src_units - 1);
	
	check = (src_item->pos.item == hint->pos.item &&
		 hint->pos.unit != ~0ul);

	while (!(hint->result & SF_MOVIP) &&
	       curr < cde_short_units(src_item))
	{

		/* Check if we should update unit pos. we will update it if we
		   are at insert point and unit pos is not ~0ul. */
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
		len = cde_short_get_len(src_item, curr);

		if (space < len + sizeof(entry_t))
			break;

		/* Updating unit pos. We will do so in the case item component
		   of insert point is the same as current item has and unit
		   component is not ~0ul. */
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
	   number of bytes to be moved from @src_item to @dst_item. */
	if (hint->units > 0)
		hint->rest -= space;
	
	return 0;
}

/* Makes shift of the entries from the @src_item to the @dst_item */
static errno_t cde_short_shift(item_entity_t *src_item,
			       item_entity_t *dst_item,
			       shift_hint_t *hint)
{
	uint32_t src_pos, dst_pos;
	cde_short_t *src_direntry;
	cde_short_t *dst_direntry;
	uint32_t src_units, dst_units;
	
	aal_assert("umka-1589", hint != NULL);
	aal_assert("umka-1586", src_item != NULL);
	aal_assert("umka-1587", dst_item != NULL);

	src_direntry = cde_short_body(src_item);
	dst_direntry = cde_short_body(dst_item);

	src_units = de_get_units(src_direntry);
	dst_units = de_get_units(dst_direntry);

	if (hint->control & SF_LEFT) {
		src_pos = 0;
		dst_pos = dst_units;
	} else {
		dst_pos = 0;
		src_pos = src_units - hint->units;
	}

	/* Preparing root for copying units into it */
	cde_short_expand(dst_item, dst_pos,
			  hint->units, hint->rest);

	/* Copying units from @src item to @dst one */
	cde_short_rep(dst_item, dst_pos, src_item,
		       src_pos, hint->units);

	cde_short_shrink(src_item, src_pos,
			  hint->units, 0);

	/* Updating item key by first direntry key */
	if (de_get_units(src_direntry) > 0 &&
	    hint->control & SF_LEFT)
	{
		cde_short_get_key(src_item, 0,
				   &src_item->key);
	}

	return 0;
}

/* Inserts new entries to direntry item */
static errno_t cde_short_insert(item_entity_t *item,
				create_hint_t *hint,
				uint32_t pos)
{
	entry_t *entry;
	uint32_t i, offset;

	cde_short_t *direntry;
	entry_hint_t *entry_hint;
    
	aal_assert("umka-791", item != NULL);
	aal_assert("umka-792", hint != NULL);
	aal_assert("umka-897", pos != ~0ul);

	direntry = cde_short_body(item);
	entry_hint = (entry_hint_t *)hint->type_specific;

	/* Expanding direntry in order to prepare the room for new entries. The
	   function cde_short_expand returns the offset of where new unit will
	   be inserted at. */
	offset = cde_short_expand(item, pos, hint->count,
				   hint->len);
	
	/* Creating new entries */
	for (i = 0, entry = &direntry->entry[pos];
	     i < hint->count; i++, entry++, entry_hint++)
	{
		hash_t *entid;
		objid_t *objid;

		key_entity_t *hash;
		key_entity_t *object;
		uint64_t oid, loc, off;

		entid = (hash_t *)&entry->hash;

		objid = (objid_t *)((void *)direntry +
				    offset);
		
		/* Setting up the offset of new entry */
		en_set_offset(entry, offset);
		hash = &entry_hint->offset;
		
		/* Creating proper entry identifier (hash) */
		oid = plugin_call(hash->plugin->o.key_ops,
				  get_objectid, hash);
		
		ha_set_objectid(entid, oid);

		off = plugin_call(hash->plugin->o.key_ops,
				  get_offset, hash);

		ha_set_offset(entid, off);

		object = &entry_hint->object;
		aal_memcpy(objid, object->body, sizeof(*objid));

		offset += sizeof(objid_t);

		/* If key is long one we also count name length */
		if (plugin_call(item->key.plugin->o.key_ops,
				tall, &entry_hint->offset))
		{
			uint32_t len = aal_strlen(entry_hint->name);

			aal_memcpy((void *)direntry + offset,
				   entry_hint->name, len);

			offset += len;
			*((char *)direntry + offset) = '\0';
			offset++;
		}
	}
	
	de_inc_units(direntry, hint->count);
	
	/* Updating item key by unit key if the first unit was changed. It is
	   needed for correct updating left delimiting keys. */
	if (pos == 0)
		cde_short_get_key(item, 0, &item->key);
    
	return 0;
}

/* Removes @count entries at @pos from passed @item */
int32_t cde_short_remove(item_entity_t *item,
			 uint32_t pos, uint32_t count)
{
	uint32_t len;

	aal_assert("umka-934", item != NULL);

	len = count * sizeof(entry_t);
	len += cde_short_size(item, pos, count);
	
	/* Shrinking direntry */
	cde_short_shrink(item, pos, count, 0);
	
	/* Updating item key */
	if (pos == 0 && cde_short_units(item) > 0)
		cde_short_get_key(item, 0, &item->key);

	return len;
}

/* Prepares area new item will be created at */
static errno_t cde_short_init(item_entity_t *item) {
	aal_assert("umka-1010", item != NULL);
	aal_assert("umka-2215", item->body != NULL);
	
	((cde_short_t *)item->body)->units = 0;
	return 0;
}

/* Prints direntry item into passed @stream */
static errno_t cde_short_print(item_entity_t *item,
			       aal_stream_t *stream,
			       uint16_t options) 
{
	uint32_t i, j;
	char name[256];
	uint32_t namewidth;
	cde_short_t *direntry;
	uint64_t locality, objectid;
	
	aal_assert("umka-548", item != NULL);
	aal_assert("umka-549", stream != NULL);

	direntry = cde_short_body(item);
	
	aal_stream_format(stream, "DIRENTRY PLUGIN=%s LEN=%u, KEY=",
			  item->plugin->label, item->len);
		
	if (plugin_call(item->key.plugin->o.key_ops, print,
			&item->key, stream, options))
	{
		return -EINVAL;
	}
	
	aal_stream_format(stream, " UNITS=%u\n",
			  de_get_units(direntry));

	aal_stream_format(stream, "NR  NAME%*s OFFSET HASH%*s "
			  "SDKEY%*s\n", 13, " ", 29, " ", 13, " ");
	
	aal_stream_format(stream, "----------------------------"
			  "------------------------------------"
			  "--------------\n");
	
	/* Loop though the all entries */
	for (i = 0; i < de_get_units(direntry); i++) {
		uint64_t offset, haobjectid;
		
		entry_t *entry = &direntry->entry[i];
		objid_t *objid = cde_short_objid(item, i);

		cde_short_get_name(item, i, name, sizeof(name));

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

/* Returns real maximal key in direntry item */
static errno_t cde_short_maxreal_key(item_entity_t *item, 
				     key_entity_t *key) 
{
	uint32_t units;
	
	aal_assert("umka-1651", key != NULL);
	aal_assert("umka-1650", item != NULL);

	units = cde_short_units(item);
	return cde_short_get_key(item, units - 1, key);
}

extern errno_t cde_short_copy(item_entity_t *dst,
			      uint32_t dst_pos, 
			      item_entity_t *src,
			      uint32_t src_pos, 
			      copy_hint_t *hint);

extern errno_t cde_short_check_struct(item_entity_t *item,
				      uint8_t mode);

extern errno_t cde_short_estimate_copy(item_entity_t *dst,
				       uint32_t dst_pos,
				       item_entity_t *src,
				       uint32_t src_pos,
				       copy_hint_t *hint);
#endif

/* Returns maximal possible key in direntry item. It is needed for lookuping
   needed entry by entry key. */
errno_t cde_short_maxposs_key(item_entity_t *item, 
			      key_entity_t *key) 
{
	key_entity_t *maxkey;

	aal_assert("umka-1649", key != NULL);
	aal_assert("umka-1648", item != NULL);

	plugin_call(item->key.plugin->o.key_ops,
		    assign, key, &item->key);

	maxkey = plugin_call(key->plugin->o.key_ops,
			     maximal);
    
    	plugin_call(key->plugin->o.key_ops, set_objectid,
		    key, plugin_call(key->plugin->o.key_ops,
				     get_objectid, maxkey));
	
	plugin_call(key->plugin->o.key_ops, set_offset,
		    key, plugin_call(key->plugin->o.key_ops,
				     get_offset, maxkey));
	
	return 0;
}

/* Helper function that is used by lookup method for comparing given key with
   passed entry hash. */
static int callback_comp_entry(void *array, uint32_t pos,
			       void *key, void *data)
{
	key_entity_t curr;

	cde_short_get_key((item_entity_t *)data, pos, &curr);

	return plugin_call(((item_entity_t *)data)->key.plugin->o.key_ops,
			   compfull, &curr, (key_entity_t *)key);
}

/* Performs lookup inside direntry. Found pos is stored in @pos */
lookup_t cde_short_lookup(item_entity_t *item,
			  key_entity_t *key,
			  uint32_t *pos)
{
	lookup_t res;
	key_entity_t maxkey;

	aal_assert("umka-610", key != NULL);
	aal_assert("umka-717", key->plugin != NULL);
    
	aal_assert("umka-609", item != NULL);
	aal_assert("umka-629", pos != NULL);
    
	/* Getting maximal possible key */
	cde_short_maxposs_key(item, &maxkey);

	/* If looked key is greater that maximal possible one then we going out
	   and return FALSE, that is the key not found. */
	if (plugin_call(key->plugin->o.key_ops, compfull,
			key, &maxkey) > 0)
	{
		*pos = cde_short_units(item);
		return ABSENT;
	}

	/* Comparing looked key with minimal one (that is with item key) */
	if (plugin_call(key->plugin->o.key_ops, compfull,
			&item->key, key) > 0)
	{
		*pos = 0;
		return ABSENT;
	}

	/* Performing binary search inside the direntry in order to find
	   position of the looked key. */
	switch (aux_bin_search(item->body, cde_short_units(item), key,
			       callback_comp_entry, (void *)item, pos))
	{
	case 1:
		return PRESENT;
	case 0:
		return ABSENT;
	default:
		return FAILED;
	}
}

static reiser4_item_ops_t cde_short_ops = {
#ifndef ENABLE_STAND_ALONE	    
	.init		   = cde_short_init,
	.copy		   = cde_short_copy,
	.rep		   = cde_short_rep,
	.expand		   = cde_short_expand,
	.shrink		   = cde_short_shrink,
	.insert		   = cde_short_insert,
	.remove		   = cde_short_remove,
	.overhead          = cde_short_overhead,
	.check_struct	   = cde_short_check_struct,
	.print		   = cde_short_print,
	.shift             = cde_short_shift,
	.maxreal_key       = cde_short_maxreal_key,
	
	.estimate_copy	   = cde_short_estimate_copy,
	.estimate_shift    = cde_short_estimate_shift,
	.estimate_insert   = cde_short_estimate_insert,
		
	.set_key	   = NULL,
	.layout		   = NULL,
	.check_layout	   = NULL,
	.get_plugid	   = NULL,
#endif
	.branch            = NULL,

	.data		   = cde_short_data,
	.lookup		   = cde_short_lookup,
	.units		   = cde_short_units,
	.read              = cde_short_read,
	.get_key	   = cde_short_get_key,
	.mergeable         = cde_short_mergeable,
	.maxposs_key	   = cde_short_maxposs_key
};

static reiser4_plugin_t cde_short_plugin = {
	.cl    = CLASS_INIT,
	.id    = {ITEM_CDE_SHORT_ID, DIRENTRY_ITEM, ITEM_PLUGIN_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "cde_short",
	.desc  = "Compound direntry for reiser4, ver. " VERSION,
#endif
	.o = {
		.item_ops = &cde_short_ops
	}
};

static reiser4_plugin_t *cde_short_start(reiser4_core_t *c) {
	return &cde_short_plugin;
}

plugin_register(cde_short, cde_short_start, NULL);
#endif
