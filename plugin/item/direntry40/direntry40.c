/*
  direntry40.c -- reiser4 default direntry plugin.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "direntry40.h"

static reiser4_core_t *core = NULL;

/*
  Returns pointer to the objectid entry component in passed @direntry at pased
  @pos. It is used in code bellow.
*/
static inline objid40_t *direntry40_unit(direntry40_t *direntry, 
					 uint32_t pos)
{
	uint32_t offset = direntry->entry[pos].offset;
	return (objid40_t *)((void *)direntry + offset);
}

/* Retutns statdata key of the file entry points to */
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
	locality = oid40_get_locality(objid);

	/* Building key by means of using key plugin */
	key->plugin = item->key.plugin;
	
	return plugin_call(key->plugin->key_ops, build_generic,
			   key, KEY_STATDATA_TYPE, locality,
			   objectid, 0);
}

/* Returns pointer to entry at passed @pos */
static inline entry40_t *direntry40_entry(direntry40_t *direntry, 
					  uint32_t pos)
{
	return &direntry->entry[pos];
}

/* Retutns TRUE if passed name is long and can't fit into objectid */
#define OID_CHARS (sizeof(uint64_t) - 1)

static inline int direntry40_name_long(char *name) {
	return (aal_strlen(name) > OID_CHARS + sizeof(uint64_t));
}

/* The same as previous function but works on entry */
static inline int direntry40_entry_long(direntry40_t *direntry,
					entry40_t *entry)
{
	uint64_t objectid = eid40_get_objectid(&entry->entryid);
	return (objectid & 0x0100000000000000ull) ? 1 : 0;
}

/*
  Calculates entry length. This function is widely used in shift code and
  modification code.
*/
static inline uint32_t direntry40_entry_len(direntry40_t *direntry,
					    entry40_t *entry)
{
	uint32_t len;
	objid40_t *objid;

	/* Counting objectid size */
	len = sizeof(objid40_t);

	/*
	  If entry contains long name it is stored just after objectid.
	  Otherwise, entry name is stored in objectid and offset of the
	  entry. This trick saves a lot of space in directories, because the
	  average name is shorter than 15 symbols.
	*/
	if (direntry40_entry_long(direntry, entry)) {

		/* Counting name length too */
		objid = (objid40_t *)((void *)direntry +
				      entry->offset);
		
		len += aal_strlen((char *)(objid + 1)) + 1;
	}
	
	return len;
}

/* Extracts the part of entry name from the 64bits value it was packed to */
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

/* Extracts entry name from the passed @entry to passed @buff */
static char *direntry40_entry_name(direntry40_t *direntry,
				   entry40_t *entry, char *buff)
{
	char *cont;
	uint64_t offset;
	objid40_t *objid;
	uint64_t objectid;

	objid = (objid40_t *)((void *)direntry +
			      entry->offset);

	/*
	  If name is long, we just copy it from the area after
	  objectid. Otherwise we extract it from the entry hash.
	*/
	if (direntry40_entry_long(direntry, entry)) {
		char *name = (char *)(objid + 1);
		uint32_t len = aal_strlen(name);
		
		aal_strncpy(buff, name, len);
		*(buff + len) = '\0';
	} else {
		objectid = (eid40_get_objectid(&entry->entryid) &
			    ~0x0100000000000000ull);
		
		offset = eid40_get_offset(&entry->entryid);

		/* Special case, handling "." entry */
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

/* Returns the number of usets passed direntry item contains */
static uint32_t direntry40_units(item_entity_t *item) {
	direntry40_t *direntry;
    
	aal_assert("umka-865", item != NULL);

	direntry = direntry40_body(item);
	return de40_get_count(direntry);
}

/*
  Builds full key by entry components. It is needed for updating keys after
  shift, insert, etc. Also library requires unit keys sometims.
*/
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

	aal_assert("umka-1606", key != NULL);
	aal_assert("umka-1607", item != NULL);
	aal_assert("umka-1605", item->body != NULL);

	direntry = direntry40_body(item);
	units = direntry40_units(item);
	
	aal_assert("umka-1647", pos < units);
	entry = direntry40_entry(direntry40_body(item), pos);

	/* Getting item key params */
	locality = plugin_call(item->key.plugin->key_ops,
			       get_locality, &item->key);

	/* Getting entry key params */
	objectid = *((uint64_t *)&entry->entryid);
	offset = *((uint64_t *)&entry->entryid + 1);

	/* Building the full key */
	key->plugin = item->key.plugin;
	
	plugin_call(item->key.plugin->key_ops, build_generic, key,
		    KEY_FILENAME_TYPE, locality, objectid, offset);

	return 0;
}

/* Reads @count of the entries starting from @pos into passed @buff */
static int32_t direntry40_read(item_entity_t *item, void *buff,
			       uint32_t pos, uint32_t count)
{
	uint32_t i;
	reiser4_plugin_t *plugin;

	direntry40_t *direntry;
	reiser4_entry_hint_t *hint;
    
	aal_assert("umka-866", item != NULL);
	aal_assert("umka-1418", buff != NULL);
    
	hint = (reiser4_entry_hint_t *)buff;
	
	aal_assert("umka-1599", hint != NULL);
	
	if (!(direntry = direntry40_body(item)))
		return -1;

	aal_assert("umka-1608", direntry != NULL);
	aal_assert("umka-1598", pos < de40_get_count(direntry));

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

static int direntry40_data() {
	return 1;
}

#ifndef ENABLE_ALONE

/*
  Returns TRUE is two items are mergeable. That is if they have the same plugin
  id and belong to the same directory. This function is used in balancing from
  the node plugin in order to determine are two items need to be merged or not.
*/
static int direntry40_mergeable(item_entity_t *item1,
				item_entity_t *item2)
{
	reiser4_plugin_t *plugin;
	roid_t locality1, locality2;
	
	aal_assert("umka-1581", item1 != NULL);
	aal_assert("umka-1582", item2 != NULL);

	plugin = item1->key.plugin;

	/*
	  Items mergeable if they have the same locality, that is oid of the
	  directory they belong to.
	*/
	locality1 = plugin_call(plugin->key_ops, get_locality, &item1->key);
	locality2 = plugin_call(plugin->key_ops, get_locality, &item2->key);

	return (locality1 == locality2);
}

/*
  Estimates how much bytes will be needed to prepare in node in odrer to make
  room for inserting new entries.
*/
static errno_t direntry40_estimate(item_entity_t *item, void *buff,
				   uint32_t pos, uint32_t count) 
{
	uint32_t i;
	reiser4_item_hint_t *hint;
	reiser4_entry_hint_t *entry_hint;
	    
	aal_assert("vpf-095", buff != NULL);
    
	hint = (reiser4_item_hint_t *)buff;
	entry_hint = (reiser4_entry_hint_t *)hint->type_specific;
	
	hint->len = count * sizeof(entry40_t);
    
	for (i = 0; i < count; i++, entry_hint++) {
		hint->len += sizeof(objid40_t);

		if (direntry40_name_long(entry_hint->name))
			hint->len += aal_strlen(entry_hint->name) + 1;
	}

	/*
	  If the pos we are going to insert new units is ~0ul, we assume it is
	  the attempt to insert new directory item. In this case we should also
	  count item overhead, that is direntry40 header which contains the
	  number of entries in item.
	*/
	if (pos == ~0ul)
		hint->len += sizeof(direntry40_t);
    
	return 0;
}

/*
  Predicts how many entries and bytes can be shifted from the @src_item to
  @dst_item. The behavior of the function depends on the passed @hint.
*/
static errno_t direntry40_predict(item_entity_t *src_item,
				  item_entity_t *dst_item,
				  shift_hint_t *hint)
{
	uint32_t cur;
	uint32_t flags;
	uint32_t src_units;
	uint32_t dst_units;
	uint32_t space, len;

	entry40_t *entry;
	direntry40_t *direntry;
	
	aal_assert("umka-1591", src_item != NULL);
	aal_assert("umka-1592", hint != NULL);

	src_units = direntry40_units(src_item);
	dst_units = dst_item ? direntry40_units(dst_item) : 0;

	space = hint->rest;

	/*
	  If hint's create flag is present, we need to create new direntry item,
	  so we should count its overhead.
	*/
	if (hint->create) {
		
		if (space < sizeof(direntry40_t))
			return 0;
		
		space -= sizeof(direntry40_t);
	}

	cur = (hint->control & SF_LEFT ? 0 : src_units - 1);
	
	if (!(direntry = direntry40_body(src_item)))
		return -1;

	flags = hint->control;
	
	while (!(hint->result & SF_MOVIP) && cur < direntry40_units(src_item)) {

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
						hint->result |= SF_MOVIP;
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

/* Makes shift of the entries from the @src_item to the @dst_item */
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
	
	aal_assert("umka-1586", src_item != NULL);
	aal_assert("umka-1587", dst_item != NULL);
	aal_assert("umka-1589", hint != NULL);

	if (!(src_direntry = direntry40_body(src_item)))
		return -1;
	
	if (!(dst_direntry = direntry40_body(dst_item)))
		return -1;

	src_units = de40_get_count(src_direntry);
	dst_units = de40_get_count(dst_direntry);
	
	aal_assert("umka-1604", src_units >= hint->units);

	headers = hint->units * sizeof(entry40_t);
	hint->rest -= (hint->create ? sizeof(direntry40_t) : 0);
		
	if (hint->control & SF_LEFT) {

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
	if (hint->control & SF_LEFT) {
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

static uint32_t direntry40_size(direntry40_t *direntry,
				uint32_t pos, uint32_t count)
{
	uint32_t i;
	entry40_t *entry;
	uint32_t size = 0;

	entry = direntry40_entry(direntry, pos);

	for (i = 0; i < count; i++, entry++)
		size += direntry40_entry_len(direntry, entry);

	return size;
}

/* Shrinks direntry item in order to delete some entries */
static int32_t direntry40_shrink(item_entity_t *item,
				 uint32_t pos, uint32_t count)
{
	uint32_t first;
	uint32_t second;
	uint32_t remove;
	uint32_t headers;
	uint32_t i, units;

	entry40_t *entry;
	direntry40_t *direntry;
	
	direntry = direntry40_body(item);
	
	units = de40_get_count(direntry);
	aal_assert("umka-1681", pos < units);

	if (pos + count > units)
		count = units - pos;

	if (count == 0)
		return 0;

	headers = count * sizeof(entry40_t);
	
	/* Getting how many bytes should be moved before passed @pos */
	first = (units - (pos + count)) * sizeof(entry40_t);
	first += direntry40_size(direntry, 0, pos);

	/* Getting how many bytes shopuld be moved after passed @pos. */
	second = direntry40_size(direntry, pos + count,
				 units - (pos + count));

	/* Calculating how many bytes will be moved out */
	remove = direntry40_size(direntry, pos, count);

	/* Moving headers and first part of bodies (before passed @pos) */
	entry = direntry40_entry(direntry, pos);
	aal_memmove(entry, entry + count, first);

	/* Setting up the entry offsets */
	entry = direntry40_entry(direntry, 0);
	
	for (i = 0; i < pos; i++, entry++)
		en40_dec_offset(entry, headers);

	/*
	  If it is needed, we also move the rest of the data (after insert
	  point).
	*/
	if (second > 0) {
		void *src, *dst;

		entry = direntry40_entry(direntry, pos);

		src = (void *)direntry +
			en40_get_offset(entry);
		
		dst = src - (headers + remove);
		
		aal_memmove(dst, src, second);

		/* Setting up entry offsets */
		for (i = pos; i < units - count; i++) {
			entry = direntry40_entry(direntry, i);
			en40_dec_offset(entry, (headers + remove));
		}
	}
	
	de40_dec_count(direntry, count);
	return (remove + headers);
}

/* Prepares direntry40 for insert new entries */
static int32_t direntry40_expand(direntry40_t *direntry, uint32_t pos,
				 uint32_t count, uint32_t len)
{
	void *src, *dst;
	entry40_t *entry;

	uint32_t first;
	uint32_t second;
	uint32_t offset;
	uint32_t headers;
	uint32_t i, units;

	aal_assert("umka-1724", len > 0);
	aal_assert("umka-1724", count > 0);
	aal_assert("umka-1723", direntry != NULL);
	
	units = de40_get_count(direntry);
	headers = count * sizeof(entry40_t);

	aal_assert("umka-1722", pos <= units);

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
	first = (units - pos) * sizeof(entry40_t);
	first += direntry40_size(direntry, 0, pos);
	
	/* Calculating length bytes to be moved after insert point */
	second = direntry40_size(direntry, pos, units - pos);
	
	/* Updating offset of entries which lie before insert point */
	entry = direntry40_entry(direntry, 0);
	
	for (i = 0; i < pos; i++, entry++)
		en40_inc_offset(entry, headers);
    
	/* Updating offset of entries which lie after insert point */
	entry = direntry40_entry(direntry, pos);
	
	for (i = pos; i < units; i++, entry++)
		en40_inc_offset(entry, len);
    
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
		src = direntry40_entry(direntry, pos);
		dst = src + (count * sizeof(entry40_t));
		aal_memmove(dst, src, first);
	}
    
	return offset;
}

/* Inserts new entries inside direntry item */
static int32_t direntry40_write(item_entity_t *item, void *buff,
				uint32_t pos, uint32_t count)
{
	entry40_t *entry;
	uint32_t i, offset;
	direntry40_t *direntry;

	reiser4_item_hint_t *hint;
	reiser4_entry_hint_t *entry_hint;
    
	aal_assert("umka-791", item != NULL);
	aal_assert("umka-792", buff != NULL);
	aal_assert("umka-897", pos != ~0ul);

	if (!(direntry = direntry40_body(item)))
		return -1;

	hint = (reiser4_item_hint_t *)buff;
	entry_hint = (reiser4_entry_hint_t *)hint->type_specific;

	/*
	  Expanding direntry in order to prepare the room for new entries. The
	  function direntry40_expand returns the offset of where new unit will
	  be inserted.
	*/
	if ((offset = direntry40_expand(direntry, pos, count, hint->len)) <= 0) {
		aal_exception_error("Can't expand direntry item at "
				    "pos %u by %u entries.", pos, count);
		return -1;
	}
	
	/* Creating new entries */
	entry = direntry40_entry(direntry, pos);
		
	for (i = 0; i < count; i++, entry++, entry_hint++) {
		objid40_t *objid;
		entryid40_t *entid;
		uint64_t oid, loc, off;

		key_entity_t *hash;
		key_entity_t *object;

		entid = (entryid40_t *)&entry->entryid;
		objid = (objid40_t *)((void *)direntry + offset);
		
		/* Setting up the offset of new entry */
		en40_set_offset(entry, offset);

		hash = &entry_hint->offset;
		
		/* Creating proper entry identifier (hash) */
		oid = plugin_call(hash->plugin->key_ops,
				  get_objectid, hash);
		
		eid40_set_objectid(entid, oid);

		off = plugin_call(hash->plugin->key_ops,
				  get_offset, hash);

		eid40_set_offset(entid, off);

		/* Creating stat data key of the object, entry will point to */
		object = &entry_hint->object;

		loc = plugin_call(object->plugin->key_ops,
				  get_locality, object);

		oid40_set_locality(objid, loc);

		oid = plugin_call(object->plugin->key_ops,
				  get_objectid, object);
		
		oid40_set_objectid(objid, oid);

		offset += sizeof(objid40_t);

		/*
		  If entry name is lesser than key can hold (15 symbols), then
		  entry name will be stored separately. If no, then entry name
		  will be stored in entry key.
		*/
		if (direntry40_name_long(entry_hint->name)) {
			uint32_t len = aal_strlen(entry_hint->name);

			aal_memcpy((void *)direntry + offset,
				   entry_hint->name, len);

			offset += len;
			*((char *)direntry + offset) = '\0';
			offset++;
		}
	}
	
	/* Updating direntry count field */
	de40_inc_count(direntry, count);

	/*
	  Updating item key by unit key if the first unit was changed. It is
	  needed for corrent updating left delimiting keys.
	*/
	if (pos == 0) {
		if (direntry40_get_key(item, 0, &item->key))
			return -1;
	}
    
	return count;
}

/* Removes @count entries at @pos from passed @item */
static int32_t direntry40_remove(item_entity_t *item,
				 uint32_t pos,
				 uint32_t count)
{
	uint32_t len;
	direntry40_t *direntry;
    
	aal_assert("umka-934", item != NULL);

	if (!(direntry = direntry40_body(item)))
		return -1;

	/* Shrinking direntry */
	if ((len = direntry40_shrink(item, pos, count)) <= 0) {
		aal_exception_error("Can't shrink direntry at pos "
				    "%u by %u entries.", pos, count);
		return -1;
	}

	/* Updating item key */
	if (pos == 0 && de40_get_count(direntry) > 0) {
		if (direntry40_get_key(item, 0, &item->key))
			return -1;
	}

	return len;
}

/* Prepares area new item will be created at */
static errno_t direntry40_init(item_entity_t *item) {
	aal_assert("umka-1010", item != NULL);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

/* Prints direntry item into passed @stream */
static errno_t direntry40_print(item_entity_t *item,
				aal_stream_t *stream,
				uint16_t options) 
{
	uint32_t i, width;
	direntry40_t *direntry;

	char name[256];
	uint64_t objid, offset;
	uint64_t locality, objectid;
	
	aal_assert("umka-548", item != NULL);
	aal_assert("umka-549", stream != NULL);

	if (!(direntry = direntry40_body(item)))
		return -1;
	
	aal_stream_format(stream, "DIRENTRY: len=%u, KEY: ", item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print, &item->key,
			stream, options))
		return -1;
	
	aal_stream_format(stream, " PLUGIN: 0x%x (%s)\n",
			  item->plugin->h.id, item->plugin->h.label);
	
	aal_stream_format(stream, "count:\t\t%u\n", de40_get_count(direntry));

	/* Loop though the all entries */
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

/*
  Returns maximal possible key in direntry item. It is needed for lookuping
  needed entry by entry key.
*/
static errno_t direntry40_maxposs_key(item_entity_t *item, 
				      key_entity_t *key) 
{
	uint64_t offset;
	uint64_t objectid;
	
	key_entity_t *maxkey;

	aal_assert("umka-1649", key != NULL);
	aal_assert("umka-1648", item != NULL);
	aal_assert("umka-716", key->plugin != NULL);

	plugin_call(key->plugin->key_ops, assign, key, &item->key);

	maxkey = plugin_call(key->plugin->key_ops, maximal,);
    
	objectid = plugin_call(key->plugin->key_ops, get_objectid, maxkey);
    	offset = plugin_call(key->plugin->key_ops, get_offset, maxkey);

    	plugin_call(key->plugin->key_ops, set_objectid, key, objectid);
	plugin_call(key->plugin->key_ops, set_offset, key, offset);
    
	return 0;
}

/* Returns real maximal key in direntry item */
static errno_t direntry40_utmost_key(item_entity_t *item, 
				     key_entity_t *key) 
{
	uint32_t units;
	direntry40_t *direntry;
	
	aal_assert("umka-1651", key != NULL);
	aal_assert("umka-1650", item != NULL);
	aal_assert("umka-716", key->plugin != NULL);
	aal_assert("umka-1652", item->body != NULL);

	direntry = direntry40_body(item);
	units = de40_get_count(direntry);
	
	aal_assert("umka-1653", units > 0);
	return direntry40_get_key(item, units - 1, key);
}

/* 
   Helper function that is used by lookup method for comparing given key with
   passed entry hash.
*/
static inline int callback_comp_entry(void *array, uint32_t pos,
				      void *key, void *data)
{
	item_entity_t *item;
	key_entity_t *wanted;
	key_entity_t current;

	item = (item_entity_t *)data;
	wanted = (key_entity_t *)key;
	
	if (direntry40_get_key(item, pos, &current))
		return -1;
    
	return plugin_call(item->key.plugin->key_ops, compare, &current, wanted);
}

/* Performs lookup inside direntry. Found pos is stored in @pos */
static lookup_t direntry40_lookup(item_entity_t *item,
				  key_entity_t *key,
				  uint32_t *pos)
{
	lookup_t res;
	uint64_t unit;
	uint32_t units;
    
	direntry40_t *direntry;
	key_entity_t maxkey, minkey;

	aal_assert("umka-610", key != NULL);
	aal_assert("umka-717", key->plugin != NULL);
    
	aal_assert("umka-609", item != NULL);
	aal_assert("umka-629", pos != NULL);
    
	if (!(direntry = direntry40_body(item)))
		return LP_FAILED;

	/* Getting maximal possible key */
	maxkey.plugin = key->plugin;

	plugin_call(maxkey.plugin->key_ops, assign, &maxkey, &item->key);
	
	if (direntry40_maxposs_key(item, &maxkey))
		return LP_FAILED;

	/*
	  If looked key is greater that maximal possible one then we going out
	  and return FALSE, that is the key not found.
	*/
	units = direntry40_units(item);
	
	if (plugin_call(key->plugin->key_ops, compare, key, &maxkey) > 0) {
		*pos = units;
		return LP_ABSENT;
	}

	/* Comparing looked key with minimal one */
	minkey.plugin = key->plugin;
	
	plugin_call(minkey.plugin->key_ops, assign, &minkey, &item->key);

	if (plugin_call(key->plugin->key_ops, compare, &minkey, key) > 0) {
		*pos = 0;
		return LP_ABSENT;
	}

	/*
	  Performing binary search inside the direntry in order to find position
	  iof the looked key.
	*/
	res = aux_bin_search((void *)direntry, units, key, 
			     callback_comp_entry, (void *)item,
			     &unit);

	/*
	  Position correcting for the case key was not found. It is needed for
	  the case when we are going to insert new entry and searching the
	  position of insertion.
	*/
	if (res == LP_FAILED)
		return res;
	
	*pos = (uint32_t)unit;

	if (res == LP_ABSENT)
		(*pos)++;
    
	return res;
}

/* Preparing direntry plugin structure */
static reiser4_plugin_t direntry40_plugin = {
	.item_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = ITEM_CDE40_ID,
			.group = DIRENTRY_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "direntry40",
			.desc = "Compound direntry for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_ALONE	    
		.init		= direntry40_init,
		.write		= direntry40_write,
		.remove		= direntry40_remove,
		.estimate	= direntry40_estimate,
		.check		= direntry40_check,
		.print		= direntry40_print,
		.mergeable      = direntry40_mergeable,

		.shift          = direntry40_shift,
		.predict        = direntry40_predict,
#else
		.init		= NULL,
		.estimate	= NULL,
		.write		= NULL,
		.remove		= NULL,
		.check		= NULL,
		.print		= NULL,
		.mergeable      = NULL,
		.shift          = NULL,
		.predict        = NULL,
#endif
		.layout		= NULL,
		.belongs        = NULL,
		.valid		= NULL,
		.insert         = NULL,
		.branch         = NULL,
		.data		= direntry40_data,

		.gap_key	= NULL,
		.set_key	= NULL,
		
		.get_key	= direntry40_get_key,
		.lookup		= direntry40_lookup,
		.units		= direntry40_units,
		.read           = direntry40_read,
		
		.maxposs_key	= direntry40_maxposs_key,
		.utmost_key     = direntry40_utmost_key,

		.layout_check	= NULL
	}
};

static reiser4_plugin_t *direntry40_start(reiser4_core_t *c) {
	core = c;
	return &direntry40_plugin;
}

plugin_register(direntry40_start, NULL);

