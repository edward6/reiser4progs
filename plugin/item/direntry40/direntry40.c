/*
  direntry40.c -- reiser4 default direntry plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "direntry40.h"

static reiser4_core_t *core = NULL;

static inline objid40_t *direntry40_unit(direntry40_t *direntry, 
					 uint32_t pos)
{
	uint32_t offset = direntry->entry[pos].offset;
	return (objid40_t *)((void *)direntry + offset);
}

static errno_t direntry40_unit_key(item_entity_t *item,
				   uint32_t pos,
				   key_entity_t *key)
{
	uint64_t locality;
	uint64_t objectid;

	objid40_t *objid;
	direntry40_t *direntry;
	reiser4_plugin_t *plugin;
	
	direntry = direntry40_body(item);
	objid = direntry40_unit(direntry, pos);
	
	objectid = oid40_get_objectid(objid);

	/*
	  FIXME-UMKA: Here is cutting out the minor from the locality. It is not
	  very good direntry plugin knows about key inetrnals.
	*/
	locality = (oid40_get_locality(objid) & 0xfffffffffffffff0ull) >> 4;
		
	key->plugin = item->key.plugin;
	
	return plugin_call(return -1, key->plugin->key_ops, build_generic,
			   key, KEY_FILENAME_TYPE, locality, objectid, 0);
}

static inline entry40_t *direntry40_entry(direntry40_t *direntry, 
					  uint32_t pos)
{
	return &direntry->entry[pos];
}

#define OID_CHARS (sizeof(uint64_t) - 1)

static inline int direntry40_name_long(char *name) {
	return (aal_strlen(name) > OID_CHARS + sizeof(uint64_t));
}

static inline int direntry40_entry_long(direntry40_t *direntry,
					entry40_t *entry)
{
	uint64_t objectid = eid40_get_objectid(&entry->entryid);
	return (objectid & 0x0100000000000000ull) ? 1 : 0;
}

static inline uint32_t direntry40_entry_len(direntry40_t *direntry,
					    entry40_t *entry)
{
	uint32_t len;
	objid40_t *objid;

	len = sizeof(objid40_t);
	
	if (direntry40_entry_long(direntry, entry)) {
		
		objid = (objid40_t *)((void *)direntry +
				      entry->offset);
		
		len += aal_strlen((char *)(objid + 1)) + 1;
	}
	
	return len;
}

static char *direntry40_unpack_string(uint64_t value, char *buff) {
	do {
		*buff = value >> (64 - 8);
		if (*buff)
			buff++;
		value <<= 8;
		
	} while (value != 0);

	*buff = '\0';
	return buff; 
}

static char *direntry40_entry_name(direntry40_t *direntry,
				   entry40_t *entry, char *buff)
{
	char *cont;
	uint64_t offset;
	objid40_t *objid;
	uint64_t objectid;

	objid = (objid40_t *)((void *)direntry +
			      entry->offset);
	
	if (direntry40_entry_long(direntry, entry)) {
		char *name = (char *)(objid + 1);
		uint32_t len = aal_strlen(name);
		aal_strncpy(buff, name, len);
		*(buff + len) = '\0';
	} else {
		objectid = (eid40_get_objectid(&entry->entryid) &
			    ~0x0100000000000000ull);
		
		offset = eid40_get_offset(&entry->entryid);

		if (objectid == 0ull && offset == 0ull) {
			*buff = '.';
			*(buff + 1) = '\0';
		} else {
			cont = direntry40_unpack_string(objectid, buff);
			direntry40_unpack_string(offset, cont);
		}
	}

	return buff;
}

static uint32_t direntry40_units(item_entity_t *item) {
	direntry40_t *direntry;
    
	aal_assert("umka-865", item != NULL, return 0);

	direntry = direntry40_body(item);
	return de40_get_count(direntry);
}

/* Builds full key by entry components */
static errno_t direntry40_get_key(item_entity_t *item,
				  uint32_t pos,
				  key_entity_t *key)
{
	uint64_t offset;
	roid_t locality;
	roid_t objectid;

	uint32_t units;
	entry40_t *entry;
	direntry40_t *direntry;

	aal_assert("umka-1606", key != NULL, return -1);
	aal_assert("umka-1607", item != NULL, return -1);
	aal_assert("umka-1605", item->body != NULL, return -1);

	direntry = direntry40_body(item);
	units = direntry40_units(item);
	
	aal_assert("umka-1647", pos < units, return -1);
	entry = direntry40_entry(direntry40_body(item), pos);

	locality = plugin_call(return -1, item->key.plugin->key_ops,
			       get_locality, &item->key);

	objectid = *((uint64_t *)&entry->entryid);
	offset = *((uint64_t *)&entry->entryid + 1);

	key->plugin = item->key.plugin;
	
	plugin_call(return -1, item->key.plugin->key_ops, build_generic,
		    key, KEY_FILENAME_TYPE, locality, objectid, offset);

	return 0;
}

static int32_t direntry40_fetch(item_entity_t *item, void *buff,
				uint32_t pos, uint32_t count)
{
	uint32_t i;
	reiser4_plugin_t *plugin;

	direntry40_t *direntry;
	reiser4_entry_hint_t *hint;
    
	aal_assert("umka-866", item != NULL, return -1);
	aal_assert("umka-1418", buff != NULL, return -1);
    
	hint = (reiser4_entry_hint_t *)buff;
	
	aal_assert("umka-1599", hint != NULL, return -1);
	
	if (!(direntry = direntry40_body(item)))
		return -1;

	aal_assert("umka-1608", direntry != NULL, return 0);
	aal_assert("umka-1598", pos < de40_get_count(direntry), return -1);

	if (count > direntry40_units(item) - pos)
		count = direntry40_units(item) - pos;

	plugin = item->key.plugin;
		
	for (i = pos; i < pos + count; i++, hint++) {
		entry40_t *entry = direntry40_entry(direntry, i);

		direntry40_get_key(item, i, &hint->offset);
		direntry40_entry_name(direntry, entry, hint->name);

		direntry40_unit_key(item, i, &hint->object);
	}
    
	return i - pos;
}

#ifndef ENABLE_COMPACT

static errno_t direntry40_layout(item_entity_t *item,
				 data_func_t func,
				 void *data)
{
	errno_t res;
	
	aal_assert("umka-1747", item != NULL, return -1);
	aal_assert("umka-1748", func != NULL, return -1);

	if ((res = func(item, item->con.blk, data)))
		return res;

	return 0;
}

static int direntry40_mergeable(item_entity_t *item1,
				item_entity_t *item2)
{
	reiser4_plugin_t *plugin;
	roid_t locality1, locality2;
	
	aal_assert("umka-1581", item1 != NULL, return -1);
	aal_assert("umka-1582", item2 != NULL, return -1);

	plugin = item1->key.plugin;
	
	locality1 = plugin_call(return -1, plugin->key_ops,
				get_locality, &item1->key);

	locality2 = plugin_call(return -1, plugin->key_ops,
				get_locality, &item2->key);

	return (locality1 == locality2);
}

static errno_t direntry40_estimate(item_entity_t *item,
				   reiser4_item_hint_t *hint,
				   uint32_t pos) 
{
	uint32_t i;
	reiser4_direntry_hint_t *direntry_hint;
	    
	aal_assert("vpf-095", hint != NULL, return -1);
    
	direntry_hint = (reiser4_direntry_hint_t *)hint->hint;
	hint->len = direntry_hint->count * sizeof(entry40_t);
    
	for (i = 0; i < direntry_hint->count; i++) {
		hint->len += sizeof(objid40_t);

		if (direntry40_name_long(direntry_hint->unit[i].name))
			hint->len += aal_strlen(direntry_hint->unit[i].name) + 1;
	}

	if (pos == ~0ul)
		hint->len += sizeof(direntry40_t);
    
	return 0;
}

static errno_t direntry40_predict(item_entity_t *src_item,
				  item_entity_t *dst_item,
				  shift_hint_t *hint)
{
	uint32_t cur;
	uint32_t src_units;
	uint32_t dst_units;
	uint32_t space, len;
	shift_flags_t flags;

	entry40_t *entry;
	direntry40_t *direntry;
	
	aal_assert("umka-1591", src_item != NULL, return 0);
	aal_assert("umka-1592", hint != NULL, return 0);

	src_units = direntry40_units(src_item);
	dst_units = dst_item ? direntry40_units(dst_item) : 0;

	space = hint->rest;
	
	if (hint->create) {
		
		if (space < sizeof(direntry40_t))
			return 0;
		
		space -= sizeof(direntry40_t);
	}

	cur = (hint->flags & SF_LEFT ? 0 : src_units - 1);
	
	if (!(direntry = direntry40_body(src_item)))
		return -1;

	flags = hint->flags;
	hint->flags &= ~SF_MOVIP;
	
	while (!(hint->flags & SF_MOVIP) && cur < direntry40_units(src_item)) {
		
		int check = (src_item->pos.item == hint->pos.item &&
			     hint->pos.unit != ~0ul);

		/*
		  Check if we should update unit pos. we will update it if we
		  are at insert point and unit pos is not ~0ul.
		*/
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

		entry = direntry40_entry(direntry, cur);
			
		/*
		  Check is we have enough free space for shifting one more unit
		  from src item to dst item.
		*/
		len = direntry40_entry_len(direntry, entry);

		if (space < len + sizeof(entry40_t))
			break;

		/*
		  Updating unit pos. We will do so in the case item component
		  of insert point is the same as current item has and unit
		  component is not ~0ul.
		*/
		if (check && (flags & SF_UPTIP)) {
			if (flags & SF_LEFT) {

				/*
				  Insert point is near to be moved into left
				  neighbour. Checking if we are permitted to do
				  so and updating insert point.
				*/
				if (hint->pos.unit == 0) {
					if (flags & SF_MOVIP) {
						hint->flags |= SF_MOVIP;
						hint->pos.unit = dst_units;
					} else
						break;
				} else
					hint->pos.unit--;
			} else {
				if (hint->pos.unit >= src_units - 1) {

					/*
					  Insert point is near to be shifted in
					  right neighbour. Checking permissions
					  and updating unit component of insert
					  point int hint.
					*/
					if (hint->pos.unit == src_units - 1) {
						if (flags & SF_MOVIP) {
							hint->flags |= SF_MOVIP;
							hint->pos.unit = 0;
						} else
							break;
					} else {
						if (flags & SF_MOVIP) {
							hint->flags |= SF_MOVIP;
							hint->pos.unit = 0;
						}
						break;
					}
				}
			}
		}

		/*
		  Updating unit number counters and some local variables needed
		  for controlling predicting main cycle.
		*/
		src_units--;
		dst_units++;
		hint->units++;

		cur += (flags & SF_LEFT ? 1 : -1);
		space -= (len + sizeof(entry40_t));
	}

	/*
	  Updating rest field of hint. It is needed for unit shifting. This
	  value is number of bytes to be moved from src item to dst item.
	*/
	if (hint->units > 0)
		hint->rest -= space;
	
	return 0;
}

static errno_t direntry40_shift(item_entity_t *src_item,
				item_entity_t *dst_item,
				shift_hint_t *hint)
{
	uint32_t size;
	uint32_t i, len;
	uint32_t offset;
	void *src, *dst;
	entry40_t *entry;
	uint32_t headers;
	
	direntry40_t *src_direntry;
	direntry40_t *dst_direntry;
	uint32_t src_units, dst_units;
	
	aal_assert("umka-1586", src_item != NULL, return -1);
	aal_assert("umka-1587", dst_item != NULL, return -1);
	aal_assert("umka-1589", hint != NULL, return -1);

	if (!(src_direntry = direntry40_body(src_item)))
		return -1;
	
	if (!(dst_direntry = direntry40_body(dst_item)))
		return -1;

	src_units = de40_get_count(src_direntry);
	dst_units = de40_get_count(dst_direntry);
	
	aal_assert("umka-1604", src_units >= hint->units, return -1);

	headers = hint->units * sizeof(entry40_t);
	hint->rest -= (hint->create ? sizeof(direntry40_t) : 0);
		
	if (hint->flags & SF_LEFT) {

		uint32_t dst_len = 0;

		/* Calculating dst item body length */
		dst_len = dst_item->len - hint->rest -
			sizeof(direntry40_t) - (dst_units * sizeof(entry40_t));
		
		if (dst_units > 0) {

			/* Moving entry bodies of dst direntry */
			src = (void *)dst_direntry + sizeof(direntry40_t) +
				(dst_units * sizeof(entry40_t));
			
			dst = src + headers;

			aal_memmove(dst, src, dst_len);

			/* Updating offsets of dst direntry */
			entry = direntry40_entry(dst_direntry, 0);

			for (i = 0; i < dst_units; i++, entry++)
				en40_inc_offset(entry, headers);
		}

                /* Copying entry headers */
		src = (void *)src_direntry + sizeof(direntry40_t);

		dst = (void *)dst_direntry + sizeof(direntry40_t) +
			(dst_units * sizeof(entry40_t));
		
		aal_memcpy(dst, src, headers);

		/* Copyings entry bodies */
		src = (void *)src_direntry +
			en40_get_offset((entry40_t *)src);

		dst = (void *)dst_direntry + sizeof(direntry40_t) +
			(dst_units * sizeof(entry40_t)) + headers + dst_len;
		
		size = hint->rest - headers;
		aal_memcpy(dst, src, size);

		/* Updating offset of dst direntry */
		offset = dst - (void *)dst_direntry;
		entry = direntry40_entry(dst_direntry, dst_units);
			
		for (i = 0; i < hint->units; i++, entry++) {
			en40_set_offset(entry, offset);
			offset += direntry40_entry_len(dst_direntry, entry);
		}

		if (src_units > hint->units) {
			
			/* Moving headers of the src direntry */
			src = (void *)src_direntry + sizeof(direntry40_t) +
				headers;
			
			dst = (void *)src_direntry + sizeof(direntry40_t);
			size = (src_units - hint->units) * sizeof(entry40_t);

			aal_memmove(dst, src, size);

			/* Moving bodies of the src direntry */
			src = (void *)src_direntry + sizeof(direntry40_t) +
				(src_units * sizeof(entry40_t)) + (hint->rest - headers);

			dst = src - hint->rest;

			size = src_item->len - sizeof(direntry40_t) -
				size - hint->rest;

			aal_memmove(dst, src, size);
			
			/* Updating offsets of src direntry */
			entry = direntry40_entry(src_direntry, 0);
			
			for (i = 0; i < src_units - hint->units; i++, entry++)
				en40_dec_offset(entry, hint->rest);
		}
	} else {

		if (dst_units > 0) {

			len = dst_item->len - hint->rest;
			
			/* Moving entry headers of dst direntry */
			src = (void *)dst_direntry + sizeof(direntry40_t);
			
			dst = src + headers;
			size = len - sizeof(direntry40_t);

			aal_memmove(dst, src, size);

			/* Updating offsets of dst direntry */
			entry = (entry40_t *)dst;

			for (i = 0; i < dst_units; i++, entry++)
				en40_inc_offset(entry, hint->rest);
			
			/* Moving entry bodies of dst direntry */
			src = dst + (dst_units * sizeof(entry40_t));

			dst = src + (hint->rest - headers);
				
			size -= (dst_units * sizeof(entry40_t));
			
			aal_memmove(dst, src, size);
		}
		
		/* Copying entry headers */
		src = (void *)src_direntry + sizeof(direntry40_t) +
			((src_units - hint->units) * sizeof(entry40_t));

		dst = (void *)dst_direntry + sizeof(direntry40_t);

		aal_memcpy(dst, src, headers);

		/* Copyings entry bodies */
		src = (void *)src_direntry + en40_get_offset((entry40_t *)src);

		dst = (void *)dst_direntry + sizeof(direntry40_t) +
			((hint->units + dst_units) * sizeof(entry40_t));
			
		size = hint->rest - headers;
		aal_memcpy(dst, src, size);

		/* Updating offset of dst direntry */
		entry = direntry40_entry(dst_direntry, 0);
		offset = dst - (void *)dst_direntry;
		
		for (i = 0; i < hint->units; i++, entry++) {
			en40_set_offset(entry, offset);
			offset += direntry40_entry_len(dst_direntry, entry);
		}

		if (src_units > hint->units) {
			
			/* Moving bodies of the src direntry */
			src = (void *)src_direntry + sizeof(direntry40_t) +
				(src_units * sizeof(entry40_t));
			
			dst = src - headers;

			size = src_item->len - sizeof(direntry40_t) -
				(src_units * sizeof(entry40_t));
			
			aal_memmove(dst, src, size);

			/* Updating offsets of src direntry */
			entry = direntry40_entry(src_direntry, 0);
			
			for (i = 0; i < src_units - hint->units; i++, entry++)
				en40_dec_offset(entry, headers);
		}
	}

	de40_inc_count(dst_direntry, hint->units);
	de40_dec_count(src_direntry, hint->units);

	/* Updating items key */
	if (hint->flags & SF_LEFT) {
		if (de40_get_count(src_direntry) > 0) {
			if (direntry40_get_key(src_item, 0, &src_item->key))
				return -1;
		}
	} else {
		if (direntry40_get_key(dst_item, 0, &dst_item->key))
			return -1;
	}
	
	return 0;
}

static int32_t direntry40_shrink(direntry40_t *direntry, uint32_t pos,
				 uint32_t count)
{
	entry40_t *entry;

	uint32_t headers;
	uint32_t i, units;
	uint32_t after = 0;
	uint32_t offset = 0;
	uint32_t before = 0;
	uint32_t remove = 0;
	
	units = de40_get_count(direntry);
	aal_assert("umka-1681", pos < units, return -1);

	if (pos + count > units)
		count = units - pos;

	if (count == 0)
		return 0;

	headers = count * sizeof(entry40_t);
	
	/* Getting how many bytes should be moved before insert point */
	before = (units - (pos + count)) * sizeof(entry40_t);

	entry = direntry40_entry(direntry, 0);
	
	for (i = 0; i < pos; i++, entry++)
		before += direntry40_entry_len(direntry, entry);

	/* Getting how many bytes shopuld be moved after insert point. */
	entry = direntry40_entry(direntry, pos + count);
	offset = en40_get_offset(entry);
	
	for (i = pos + count; i < units - 1; i++, entry++)
		after += direntry40_entry_len(direntry, entry);

	if (after > 0) {
		
		/* Calculating how many bytes will be removed */
		entry = direntry40_entry(direntry, pos);
		
		for (i = pos; i < pos + count; i++, entry++)
			remove += direntry40_entry_len(direntry, entry);
	}
	
	/* Moving headers and first part of bodies (before insert point) */
	entry = direntry40_entry(direntry, pos);
	aal_memmove(entry, entry + count, before);

	/* Setting up the entry offsets */
	for (i = 0; i < pos; i++) {
		entry = direntry40_entry(direntry, i);
		en40_dec_offset(entry, headers);
	}

	/*
	  If it is needed, we also move the rest of the data (after insert
	  point).
	*/
	if (after > 0) {
		void *src, *dst;
		uint32_t size = 0;

		/* Moving the rest of data */
		src = (void *)direntry + offset;
		dst = src - (remove + headers);
		aal_memmove(dst, src, after);

		/* Setting up the offsets */
		for (i = pos; i < pos + count; i++) {
			entry = direntry40_entry(direntry, i);
			en40_dec_offset(entry, (headers + size));
		}
	}
	
	de40_dec_count(direntry, count);
	
	return (remove + headers);
}

static int32_t direntry40_remove(item_entity_t *item, uint32_t pos,
				 uint32_t count)
{
	uint32_t len;
	direntry40_t *direntry;
    
	aal_assert("umka-934", item != NULL, return -1);

	if (!(direntry = direntry40_body(item)))
		return -1;

	if ((len = direntry40_shrink(direntry, pos, count)) <= 0) {
		aal_exception_error("Can't shrink direntry at pos "
				    "%u by %u entries.", pos, count);
		return -1;
	}
	
	if (pos == 0 && de40_get_count(direntry) > 0) {
		if (direntry40_get_key(item, 0, &item->key))
			return -1;
	}

	return len;
}

/* Prepares direntry40 for insert new entries */
static int32_t direntry40_expand(direntry40_t *direntry, uint32_t pos,
				 uint32_t count, uint32_t len)
{
	void *src, *dst;
	entry40_t *entry;

	uint32_t offset;
	uint32_t headers;
	uint32_t i, units;
	uint32_t after = 0;
	uint32_t before = 0;

	aal_assert("umka-1724", len > 0, return -1);
	aal_assert("umka-1724", count > 0, return -1);
	aal_assert("umka-1723", direntry != NULL, return -1);
	
	units = de40_get_count(direntry);
	headers = count * sizeof(entry40_t);

	aal_assert("umka-1722", pos <= units, return -1);

	/*
	  Getting the offset of the place new entries will be inserted at. It
	  will be used later in this function.
	*/
	if (units > 0) {
		if (pos < units) {
			entry = direntry40_entry(direntry, pos);
			offset = en40_get_offset(entry) + headers;
		} else {
			entry = direntry40_entry(direntry, units - 1);
			offset = en40_get_offset(entry) + sizeof(entry40_t) +
				direntry40_entry_len(direntry, entry);
		}
	} else
		offset = sizeof(direntry40_t) + headers;

	/* Calculating length bytes to be moved before insert point */
	before = (units - pos) * sizeof(entry40_t);
	
	for (i = 0; i < pos; i++) {
		entry = direntry40_entry(direntry, i);
		before += direntry40_entry_len(direntry, entry);
	}
	
	/* Calculating length bytes to be moved after insert point */
	for (i = pos; i < units; i++) {
		entry = direntry40_entry(direntry, i);
		after += direntry40_entry_len(direntry, entry);
	}
	
	/* Updating offset of entries which lie before insert point */
	for (i = 0; i < pos; i++) {
		entry = direntry40_entry(direntry, i);
		en40_inc_offset(entry, headers);
	}
    
	/* Updating offset of entries which lie after insert point */
	for (i = pos; i < units; i++) {
		entry = direntry40_entry(direntry, i);
		en40_inc_offset(entry, len);
	}
    
	/* Moving entry bodies if it is needed */
	if (pos < units) {
		src = (void *)direntry + offset - headers;
		dst = (void *)direntry + offset + len - headers;
		aal_memmove(dst, src, after);
	}
    
	/* Moving unit headers if it is needed */
	if (before) {
		src = direntry40_entry(direntry, pos);
		dst = src + (count * sizeof(entry40_t));
		aal_memmove(dst, src, before);
	}
    
	return offset;
}

static errno_t direntry40_insert(item_entity_t *item,
				 reiser4_item_hint_t *hint,
				 uint32_t pos)
{
	uint32_t offset;
	uint32_t i, count;
	
	entry40_t *entry;
	direntry40_t *direntry;

	reiser4_direntry_hint_t *direntry_hint;
    
	aal_assert("umka-791", item != NULL, return -1);
	aal_assert("umka-792", hint != NULL, return -1);
	aal_assert("umka-897", pos != ~0ul, return -1);

	if (!(direntry = direntry40_body(item)))
		return -1;
    
	direntry_hint = (reiser4_direntry_hint_t *)hint->hint;

	/*
	  Expanding direntry in order to prepare the room for new
	  units. Function direntry40_expand returns the offset of
	  where new unit will be inserted.
	*/
	count = direntry_hint->count;
	
	if ((offset = direntry40_expand(direntry, pos, count, hint->len)) <= 0) {
		aal_exception_error("Can't expand direntry item at "
				    "pos %u by %u entries.", pos, count);
		return -1;
	}
	
	/* Creating new entries */
	entry = direntry40_entry(direntry, pos);
		
	for (i = 0; i < count; i++, entry++) {
		objid40_t *objid;
		entryid40_t *entryid;
		uint64_t oid, loc, off;

		key_entity_t *hash;
		key_entity_t *object;

		entryid = (entryid40_t *)&entry->entryid;
		objid = (objid40_t *)((void *)direntry + offset);
		
		/* Setting up the offset of new entry */
		en40_set_offset(entry, offset);

		hash = &direntry_hint->unit[i].offset;
		
		/* Creating proper entry identifier (hash) */
		oid = plugin_call(return -1, hash->plugin->key_ops,
				  get_objectid, hash);
		
		eid40_set_objectid(entryid, oid);

		off = plugin_call(return -1, hash->plugin->key_ops,
				  get_offset, hash);

		eid40_set_offset(entryid, off);

		/* Creating stat data key ,entry points to */
		object = &direntry_hint->unit[i].object;

		loc = plugin_call(return -1, object->plugin->key_ops,
				  get_locality, object);

		oid40_set_locality(objid, loc);

		oid = plugin_call(return -1, object->plugin->key_ops,
				  get_objectid, object);
		
		oid40_set_objectid(objid, oid);

		offset += sizeof(objid40_t);

		/*
		  If entry name is lesser than key can hold (15 symbols), then
		  entry name will be stored separately. If no, then entry name
		  will be stored in entry key.
		*/
		if (direntry40_name_long(direntry_hint->unit[i].name)) {
			uint32_t len = aal_strlen(direntry_hint->unit[i].name);

			aal_memcpy((void *)direntry + offset ,
				   direntry_hint->unit[i].name, len);

			offset += len;
			*((char *)direntry + offset) = '\0';
			offset++;
		}
	}
	
	/* Updating direntry count field */
	de40_inc_count(direntry, direntry_hint->count);

	/*
	  Updating item key by unit key if the first unit was chnaged. It is
	  needed for corrent updating left delimiting keys.
	*/
	if (pos == 0) {
		if (direntry40_get_key(item, 0, &item->key))
			return -1;
	}
    
	return 0;
}

static errno_t direntry40_init(item_entity_t *item) {
	aal_assert("umka-1010", item != NULL, return -1);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

static errno_t direntry40_print(item_entity_t *item,
				aal_stream_t *stream,
				uint16_t options) 
{
	uint32_t i, width;
	direntry40_t *direntry;

	char name[256];
	uint64_t objid, offset;
	uint64_t locality, objectid;
	
	aal_assert("umka-548", item != NULL, return -1);
	aal_assert("umka-549", stream != NULL, return -1);

	if (!(direntry = direntry40_body(item)))
		return -1;
	
	aal_stream_format(stream, "DIRENTRY: len=%u, KEY: ", item->len);
		
	if (plugin_call(return -1, item->key.plugin->key_ops, print,
			&item->key, stream, options))
		return -1;
	
	aal_stream_format(stream, " PLUGIN: 0x%x (%s)\n",
			  item->plugin->h.id, item->plugin->h.label);
	
	aal_stream_format(stream, "count:\t\t%u\n", de40_get_count(direntry));

	for (i = 0; i < de40_get_count(direntry); i++) {
		entry40_t *entry = &direntry->entry[i];

		objid = eid40_get_objectid(&entry->entryid);
		offset = eid40_get_offset(&entry->entryid);

		direntry40_entry_name(direntry, entry, name);

		locality = *((uint64_t *)((void *)direntry + entry->offset));
		
		objectid = *((uint64_t *)((void *)direntry + entry->offset +
					  sizeof(uint64_t)));

		width = 30 > aal_strlen(name) ? 30 - aal_strlen(name) + 1 : 1;
		aal_stream_format(stream, "%.7llx:%.7llx\t%s%*s%.16llx:%.16llx\n",
				  locality, objectid, name, width, " ", objid, offset);
	}

	return 0;
}

extern errno_t direntry40_check(item_entity_t *item);

#endif

static errno_t direntry40_max_poss_key(item_entity_t *item, 
				       key_entity_t *key) 
{
	uint64_t offset;
	uint64_t objectid;
	
	key_entity_t *maxkey;

	aal_assert("umka-1648", item != NULL, return -1);
	aal_assert("umka-1649", key != NULL, return -1);
	aal_assert("umka-716", key->plugin != NULL, return -1);

	plugin_call(return -1, key->plugin->key_ops, assign,
		    key, &item->key);

	maxkey = plugin_call(return -1, key->plugin->key_ops,
			     maximal,);
    
	objectid = plugin_call(return -1, key->plugin->key_ops,
			       get_objectid, maxkey);
    
	offset = plugin_call(return -1, key->plugin->key_ops, 
			     get_offset, maxkey);
    
	plugin_call(return -1, key->plugin->key_ops, set_objectid, 
		    key, objectid);
    
	plugin_call(return -1, key->plugin->key_ops, set_offset, 
		    key, offset);
    
	return 0;
}

static errno_t direntry40_max_real_key(item_entity_t *item, 
				       key_entity_t *key) 
{
	uint32_t units;
	direntry40_t *direntry;
	
	aal_assert("umka-1650", item != NULL, return -1);
	aal_assert("umka-1651", key != NULL, return -1);
	aal_assert("umka-716", key->plugin != NULL, return -1);
	aal_assert("umka-1652", item->body != NULL, return -1);

	direntry = direntry40_body(item);
	units = de40_get_count(direntry);
	
	aal_assert("umka-1653", units > 0, return -1);
	return direntry40_get_key(item, units - 1, key);
}

/* 
   Helper function that is used by lookup method for comparing given key with
   passed dirid.
*/
static inline int callback_comp_entry(void *array,
				      uint32_t pos,
				      void *key, 
				      void *data)
{
	item_entity_t *item;
	key_entity_t *wanted;
	key_entity_t current;

	item = (item_entity_t *)data;
	wanted = (key_entity_t *)key;
	
	if (direntry40_get_key(item, pos, &current))
		return -1;
    
	return plugin_call(return -1, item->key.plugin->key_ops,
			   compare, &current, wanted);
}

static int direntry40_lookup(item_entity_t *item,
			     key_entity_t *key,
			     uint32_t *pos)
{
	int result;
	uint64_t unit;
	uint32_t units;
    
	direntry40_t *direntry;
	key_entity_t maxkey, minkey;

	aal_assert("umka-610", key != NULL, return -1);
	aal_assert("umka-717", key->plugin != NULL, return -1);
    
	aal_assert("umka-609", item != NULL, return -1);
	aal_assert("umka-629", pos != NULL, return -1);
    
	if (!(direntry = direntry40_body(item)))
		return -1;
    
	maxkey.plugin = key->plugin;
	
	plugin_call(return -1, maxkey.plugin->key_ops,
		    assign, &maxkey, &item->key);
	
	if (direntry40_max_poss_key(item, &maxkey))
		return -1;

	units = direntry40_units(item);
	
	if (plugin_call(return -1, key->plugin->key_ops,
			compare, key, &maxkey) > 0)
	{
		*pos = units;
		return 0;
	}
    
	minkey.plugin = key->plugin;
	
	plugin_call(return -1, minkey.plugin->key_ops,
		    assign, &minkey, &item->key);

	if (plugin_call(return -1, key->plugin->key_ops,
			compare, &minkey, key) > 0)
	{
		*pos = 0;
		return 0;
	}
    
	result = aux_bin_search((void *)direntry, units, key, 
				callback_comp_entry, (void *)item,
				&unit);

	if (result != -1) {
		*pos = (uint32_t)unit;

		if (result == 0)
			(*pos)++;
	}
    
	return result;
}

static reiser4_plugin_t direntry40_plugin = {
	.item_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = ITEM_CDE40_ID,
			.group = DIRENTRY_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "direntry40",
			.desc = "Compound direntry for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_COMPACT	    
		.init		= direntry40_init,
		.insert		= direntry40_insert,
		.remove		= direntry40_remove,
		.estimate	= direntry40_estimate,
		.check		= direntry40_check,
		.print		= direntry40_print,
		.mergeable      = direntry40_mergeable,

		.shift          = direntry40_shift,
		.predict        = direntry40_predict,
		.layout         = direntry40_layout,
#else
		.init		= NULL,
		.estimate	= NULL,
		.insert		= NULL,
		.remove		= NULL,
		.check		= NULL,
		.print		= NULL,
		.mergeable      = NULL,
		.shift          = NULL,
		.predict        = NULL,
		.layout         = NULL,
#endif
		.belongs        = NULL,
		.valid		= NULL,
		.open           = NULL,
		.update         = NULL,
		.set_key	= NULL,
		
		.get_key	= direntry40_get_key,
		.lookup		= direntry40_lookup,
		.units		= direntry40_units,
		.fetch          = direntry40_fetch,
		
		.max_poss_key	= direntry40_max_poss_key,
		.max_real_key   = direntry40_max_real_key,
		.gap_key	= NULL
	}
};

static reiser4_plugin_t *direntry40_start(reiser4_core_t *c) {
	core = c;
	return &direntry40_plugin;
}

plugin_register(direntry40_start, NULL);

