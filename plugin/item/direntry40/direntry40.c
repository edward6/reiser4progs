/*
  direntry40.c -- reiser4 default direntry plugin.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "direntry40.h"

static reiser4_core_t *core = NULL;

static lookup_t direntry40_lookup(item_entity_t *item,
				  key_entity_t *key,
				  uint32_t *pos);

/*
  Returns pointer to the objectid entry component in passed @direntry at pased
  @pos. It is used in code bellow.
*/
static objid_t *direntry40_objid(item_entity_t *item,
				 uint32_t pos)
{
	uint32_t offset;
	direntry40_t *direntry;

	direntry = direntry40_body(item);
	offset = direntry->entry[pos].offset;
	
	return (objid_t *)((void *)direntry + offset);
}

/* Retutns statdata key of the file entry points to */
static errno_t direntry40_get_obj(item_entity_t *item,
				  uint32_t pos,
				  key_entity_t *key)
{
	objid_t *objid = direntry40_objid(item, pos);
	
	/* Building key by means of using key plugin */
	return plugin_call(key->plugin->key_ops, build_short,
			   key, ob40_get_locality(objid),
			   ob40_get_objectid(objid));
}

/*
  Builds full key by entry components. It is needed for updating keys after
  shift, insert, etc. Also library requires unit keys sometims.
*/
static errno_t direntry40_get_key(item_entity_t *item,
				  uint32_t pos,
				  key_entity_t *key)
{
	oid_t locality;
	entry40_t *entry;

	aal_assert("umka-1606", key != NULL);
	aal_assert("umka-1607", item != NULL);
	aal_assert("umka-1605", item->body != NULL);
	
	entry = &direntry40_body(item)->entry[pos];

	/* Getting item key params */
	locality = plugin_call(item->key.plugin->key_ops,
			       get_locality, &item->key);

	/* Building the full key from entry at @pos */
	plugin_call(item->key.plugin->key_ops, build_generic,
		    key, KEY_FILENAME_TYPE, locality,
		    ha40_get_objectid(&entry->hash),
		    ha40_get_offset(&entry->hash));

	return 0;
}

/* Extracts entry name from the passed @entry to passed @buff */
static char *direntry40_get_name(item_entity_t *item,
				 uint32_t pos, char *buff)
{
	char *name;
	key_entity_t key;

	direntry40_get_key(item, pos, &key);

        /*
	  If name is long, we just copy it from the area after
	  objectid. Otherwise we extract it from the entry hash.
	*/
	if (plugin_call(key.plugin->key_ops, tall, &key)) {
		uint32_t len;

		name = (char *)((direntry40_objid(item, pos)) + 1);

		len = aal_strlen(name);
		aal_strncpy(buff, name, len);
		
		*(buff + len) = '\0';
	} else {
		uint64_t offset;
		uint64_t objectid;
		
		offset = plugin_call(key.plugin->key_ops,
				     get_offset, &key);
		
		objectid = plugin_call(key.plugin->key_ops,
				       get_objectid, &key);
		
		/* Special case, handling "." entry */
		if (objectid == 0ull && offset == 0ull) {
			*buff = '.';
			*(buff + 1) = '\0';
		} else {
			name = aux_unpack_string(objectid, buff);
			aux_unpack_string(offset, name);
		}
	}

	return buff;
}

#ifndef ENABLE_STAND_ALONE

/*
  Calculates entry length. This function is widely used in shift code and
  modification code.
*/
static uint32_t direntry40_get_len(item_entity_t *item,
				   uint32_t pos)
{
	uint32_t len;
	key_entity_t key;

	/* Counting objid size */
	len = sizeof(objid_t);

	/* Getting entry key */
	direntry40_get_key(item, pos, &key);
	
	/*
	  If entry contains long name it is stored just after objectid.
	  Otherwise, entry name is stored in objectid and offset of the
	  entry. This trick saves a lot of space in directories, because the
	  average name is shorter than 15 symbols.
	*/
	if (plugin_call(key.plugin->key_ops, tall, &key)) {
		objid_t *objid = direntry40_objid(item, pos);
		len += aal_strlen((char *)(objid + 1)) + 1;
	}
	
	return len;
}

#endif

/* Returns the number of usets passed direntry item contains */
static uint32_t direntry40_units(item_entity_t *item) {
	aal_assert("umka-865", item != NULL);
	return de40_get_units(direntry40_body(item));
}

/* Reads @count of the entries starting from @pos into passed @buff */
static int32_t direntry40_read(item_entity_t *item, void *buff,
			       uint32_t pos, uint32_t count)
{
	uint32_t i;
	entry_hint_t *hint;
    
	aal_assert("umka-866", item != NULL);
	aal_assert("umka-1418", buff != NULL);

	aal_assert("umka-1598",
		   pos < direntry40_units(item));
    
	hint = (entry_hint_t *)buff;

	hint->object.plugin = item->key.plugin;
	hint->offset.plugin = item->key.plugin;

	/* Check if count is valid one */
	if (count > direntry40_units(item) - pos)
		count = direntry40_units(item) - pos;

	for (i = pos; i < pos + count; i++, hint++) {
		direntry40_get_obj(item, i, &hint->object);
		direntry40_get_key(item, i, &hint->offset);
		direntry40_get_name(item, i, hint->name);
	}
    
	return i - pos;
}

static int direntry40_data(void) {
	return 1;
}

/*
  Returns TRUE is two items are mergeable. That is if they have the same plugin
  id and belong to the same directory. This function is used in balancing from
  the node plugin in order to determine are two items need to be merged or not.
*/
static int direntry40_mergeable(item_entity_t *item1,
				item_entity_t *item2)
{
	aal_assert("umka-1581", item1 != NULL);
	aal_assert("umka-1582", item2 != NULL);

	/*
	  Items mergeable if they have the same locality, that is oid of the
	  directory they belong to.
	*/
	return (plugin_call(item1->key.plugin->key_ops,
			    get_locality, &item1->key) ==
		plugin_call(item1->key.plugin->key_ops,
			    get_locality, &item2->key));
}

#ifndef ENABLE_STAND_ALONE

/*
  Estimates how much bytes will be needed to prepare in node in odrer to make
  room for inserting new entries.
*/
static errno_t direntry40_estimate(item_entity_t *item, uint32_t pos,
				   uint32_t count, create_hint_t *hint) 
{
	uint32_t i;
	entry_hint_t *entry_hint;
	    
	aal_assert("vpf-095", hint != NULL);
    
	entry_hint = (entry_hint_t *)hint->type_specific;
	hint->len = count * sizeof(entry40_t);
    
	for (i = 0; i < count; i++, entry_hint++) {
		hint->len += sizeof(objid_t);

		/*
		  Calling key plugin for in odrer to find out is passed name is
		  long one or not.
		*/
		if (plugin_call(hint->key.plugin->key_ops,
				tall, &entry_hint->offset))
		{
			/*
			  Okay, name is long, so we need add its length to
			  estimated length.
			*/
			hint->len += aal_strlen(entry_hint->name) + 1;
		}
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
	uint32_t curr;
	uint32_t flags;
	uint32_t src_units;
	uint32_t dst_units;
	uint32_t space, len;
	
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

	curr = (hint->control & SF_LEFT ? 0 : src_units - 1);
	
	flags = hint->control;
	
	while (!(hint->result & SF_MOVIP) &&
	       curr < direntry40_units(src_item))
	{

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

		/*
		  Check is we have enough free space for shifting one more unit
		  from src item to dst item.
		*/
		len = direntry40_get_len(src_item, curr);

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

		curr += (flags & SF_LEFT ? 1 : -1);
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

/* Calculates the size of @count units in passed @item at passed @pos */
static uint32_t direntry40_size(item_entity_t *item,
				uint32_t pos, uint32_t count)
{
	uint32_t size = 0;
	entry40_t *entry_end;
	entry40_t *entry_start;
	direntry40_t *direntry;

	if (count == 0)
		return 0;
	
	direntry = direntry40_body(item);
	entry_start = &direntry->entry[pos];

	if (pos + count < de40_get_units(direntry)) {
		entry_end = &direntry->entry[pos + count];
	} else {
		entry_end = &direntry->entry[pos + count - 1];
		size = direntry40_get_len(item, pos + count - 1);

	}
	
	size += (en40_get_offset(entry_end) -
		 en40_get_offset(entry_start));

	return size;
}

/* Makes copy of @count amount of units from @src_item to @dst_one */
static errno_t direntry40_rep(item_entity_t *dst_item, uint32_t dst_pos,
			      item_entity_t *src_item, uint32_t src_pos,
			      uint32_t count)
{
	uint32_t i;
	uint32_t size;
	uint32_t offset;
	void *dst, *src;
	uint32_t headers;
	uint32_t dst_units;

	entry40_t *entry;
	direntry40_t *dst_direntry;
	direntry40_t *src_direntry;
	
	aal_assert("umka-2069", dst_item != NULL);
	aal_assert("umka-2070", src_item != NULL);

	dst_direntry = direntry40_body(dst_item);
	src_direntry = direntry40_body(src_item);

	dst_units = direntry40_units(dst_item);
	aal_assert("umka-2077", dst_pos <= dst_units);
	
	/* Getting offset of body in dst item */
	offset = direntry40_size(dst_item, 0, dst_pos);
	
	/* Copying entry headers */
	src = (void *)src_direntry + sizeof(direntry40_t) +
		(src_pos * sizeof(entry40_t));

	dst = (void *)dst_direntry + sizeof(direntry40_t) +
		(dst_pos * sizeof(entry40_t));

	headers = count * sizeof(entry40_t);
		
	aal_memcpy(dst, src, headers);

	/* Copying entry bodies */
	src = (void *)src_direntry +
		en40_get_offset((entry40_t *)src);

	dst = (void *)dst_direntry + sizeof(direntry40_t) +
		(dst_units * sizeof(entry40_t)) + headers + offset;

	size = direntry40_size(src_item, src_pos, count);
	
	aal_memcpy(dst, src, size);

	/* Updating offset of dst direntry */
	entry = &dst_direntry->entry[dst_pos];

	offset += sizeof(direntry40_t) +
		(dst_units * sizeof(entry40_t)) + headers;

	for (i = 0; i < count; i++, entry++) {
		en40_set_offset(entry, offset);
			
		offset += direntry40_get_len(src_item,
					     src_pos + i);
	}
		
	/* Updating direntry count field */
	de40_inc_units(dst_direntry, count);

	/*
	  Updating item key by unit key if the first unit was changed. It is
	  needed for correct updating left delimiting keys.
	*/
	if (dst_pos == 0)
		direntry40_get_key(dst_item, 0, &dst_item->key);
	
	return 0;
}

static errno_t direntry40_feel(item_entity_t *item,
			       uint32_t pos,
			       key_entity_t *start,
			       key_entity_t *end,
			       copy_hint_t *hint)
{
	uint32_t end_pos;
	
	aal_assert("umka-1992", item != NULL);
	aal_assert("umka-1993", hint != NULL);

	if (direntry40_lookup(item, end, &end_pos) != LP_PRESENT)
		return -EINVAL;

	hint->count = end_pos - pos;

	hint->len = (sizeof(entry40_t) * hint->count) +
		direntry40_size(item, pos, hint->count);

	return 0;
}

static errno_t direntry40_copy(item_entity_t *dst_item,
			       uint32_t dst_pos,
			       item_entity_t *src_item,
			       uint32_t src_pos,
			       key_entity_t *start,
			       key_entity_t *end,
			       copy_hint_t *hint)
{
	aal_assert("umka-2127", dst_item != NULL);
	aal_assert("umka-2128", src_item != NULL);
	aal_assert("umka-2128", start != NULL);
	aal_assert("umka-2128", end != NULL);
	
	return direntry40_rep(dst_item, dst_pos, src_item,
			      src_pos, hint->count);
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

	aal_assert("umka-1959", item != NULL);
	
	direntry = direntry40_body(item);
	units = de40_get_units(direntry);
	
	aal_assert("umka-1681", pos < units);

	if (pos + count > units)
		count = units - pos;

	if (count == 0)
		return 0;

	headers = count * sizeof(entry40_t);
	
	/* Getting how many bytes should be moved before passed @pos */
	first = (units - (pos + count)) * sizeof(entry40_t);
	first += direntry40_size(item, 0, pos);

	/* Getting how many bytes shopuld be moved after passed @pos. */
	second = direntry40_size(item, pos + count,
				 units - (pos + count));

	/* Calculating how many bytes will be moved out */
	remove = direntry40_size(item, pos, count);

	/* Moving headers and first part of bodies (before passed @pos) */
	entry = &direntry->entry[pos];
	aal_memmove(entry, entry + count, first);

	/* Setting up the entry offsets */
	entry = &direntry->entry[0];
	
	for (i = 0; i < pos; i++, entry++)
		en40_dec_offset(entry, headers);

	/*
	  If it is needed, we also move the rest of the data (after insert
	  point).
	*/
	if (second > 0) {
		void *src, *dst;

		entry = &direntry->entry[pos];

		src = (void *)direntry +
			en40_get_offset(entry);
		
		dst = src - (headers + remove);
		
		aal_memmove(dst, src, second);

		/* Setting up entry offsets */
		for (i = pos; i < units - count; i++) {
			entry = &direntry->entry[i];
			en40_dec_offset(entry, (headers + remove));
		}
	}
	
	return (remove + headers);
}

/* Prepares direntry40 for insert new entries */
static int32_t direntry40_expand(item_entity_t *item, uint32_t pos,
				 uint32_t count, uint32_t len)
{
	void *src, *dst;
	entry40_t *entry;

	uint32_t first;
	uint32_t second;
	uint32_t offset;
	uint32_t headers;
	uint32_t i, units;

	direntry40_t *direntry;

	aal_assert("umka-1724", len > 0);
	aal_assert("umka-1724", count > 0);
	aal_assert("umka-1723", item != NULL);

	direntry = direntry40_body(item);
	units = de40_get_units(direntry);
	headers = count * sizeof(entry40_t);

	aal_assert("umka-1722", pos <= units);

	/*
	  Getting the offset of the place new entries will be inserted at. It
	  will be used later in this function.
	*/
	if (units > 0) {
		if (pos < units) {
			entry = &direntry->entry[pos];
			offset = en40_get_offset(entry) + headers;
		} else {
			entry = &direntry->entry[units - 1];
			
			offset = en40_get_offset(entry) + sizeof(entry40_t) +
				direntry40_get_len(item, units - 1);
		}
	} else
		offset = sizeof(direntry40_t) + headers;

	/* Calculating length bytes to be moved before insert point */
	first = (units - pos) * sizeof(entry40_t);
	first += direntry40_size(item, 0, pos);
	
	/* Calculating length bytes to be moved after insert point */
	second = direntry40_size(item, pos, units - pos);
	
	/* Updating offset of entries which lie before insert point */
	entry = &direntry->entry[0];
	
	for (i = 0; i < pos; i++, entry++)
		en40_inc_offset(entry, headers);
    
	/* Updating offset of entries which lie after insert point */
	entry = &direntry->entry[pos];
	
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
		src = &direntry->entry[pos];
		dst = src + headers;
		aal_memmove(dst, src, first);
	}
    
	return offset;
}

/* Makes shift of the entries from the @src_item to the @dst_item */
static errno_t direntry40_shift(item_entity_t *src_item,
				item_entity_t *dst_item,
				shift_hint_t *hint)
{
	uint32_t src_pos, dst_pos;
	direntry40_t *src_direntry;
	direntry40_t *dst_direntry;
	uint32_t src_units, dst_units;
	
	aal_assert("umka-1589", hint != NULL);
	aal_assert("umka-1586", src_item != NULL);
	aal_assert("umka-1587", dst_item != NULL);

	src_direntry = direntry40_body(src_item);
	dst_direntry = direntry40_body(dst_item);

	src_units = de40_get_units(src_direntry);
	dst_units = de40_get_units(dst_direntry);

	if (hint->create)
		hint->rest -= sizeof(direntry40_t);
		
	if (hint->control & SF_LEFT) {
		src_pos = 0;
		dst_pos = dst_units;
	} else {
		dst_pos = 0;
		src_pos = src_units - hint->units;
	}

	/* Preparing root for copying units into it */
	direntry40_expand(dst_item, dst_pos,
			  hint->units, hint->rest);

	/* Copying units from @src item to @dst one */
	direntry40_rep(dst_item, dst_pos, src_item,
		       src_pos, hint->units);

	direntry40_shrink(src_item, src_pos, hint->units);
	de40_dec_units(src_direntry, hint->units);

	/* Updating item key by first direntry key */
	if (hint->control & SF_LEFT) {
		if (de40_get_units(src_direntry) > 0) {
			direntry40_get_key(src_item, 0,
					   &src_item->key);
		}
	}

	return 0;
}

/* Inserts new entries to direntry item */
static errno_t direntry40_insert(item_entity_t *item,
				 create_hint_t *hint,
				 uint32_t pos)
{
	entry40_t *entry;
	uint32_t i, offset;

	direntry40_t *direntry;
	entry_hint_t *entry_hint;
    
	aal_assert("umka-791", item != NULL);
	aal_assert("umka-792", hint != NULL);
	aal_assert("umka-897", pos != ~0ul);

	direntry = direntry40_body(item);
	entry_hint = (entry_hint_t *)hint->type_specific;

	/*
	  Expanding direntry in order to prepare the room for new entries. The
	  function direntry40_expand returns the offset of where new unit will
	  be inserted at.
	*/

	offset = direntry40_expand(item, pos, hint->count,
				   hint->len);
	
	if (offset <= 0) {
		aal_exception_error("Can't expand direntry by "
				    "%u bytes.", hint->len);
		return -EINVAL;
	}
	
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
		en40_set_offset(entry, offset);
		hash = &entry_hint->offset;
		
		/* Creating proper entry identifier (hash) */
		oid = plugin_call(hash->plugin->key_ops,
				  get_objectid, hash);
		
		ha40_set_objectid(entid, oid);

		off = plugin_call(hash->plugin->key_ops,
				  get_offset, hash);

		ha40_set_offset(entid, off);

		object = &entry_hint->object;
		aal_memcpy(objid, object->body, sizeof(*objid));

		offset += sizeof(objid_t);

		/* If key is long one we also count name length */
		if (plugin_call(item->key.plugin->key_ops,
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
	
	de40_inc_units(direntry, hint->count);
	
	/*
	  Updating item key by unit key if the first unit was changed. It is
	  needed for correct updating left delimiting keys.
	*/
	if (pos == 0)
		direntry40_get_key(item, 0, &item->key);
    
	return 0;
}

/* Removes @count entries at @pos from passed @item */
int32_t direntry40_remove(item_entity_t *item,
			  uint32_t pos, uint32_t count)
{
	uint32_t len;

	aal_assert("umka-934", item != NULL);

	/* Shrinking direntry */
	if ((len = direntry40_shrink(item, pos, count)) <= 0)
		return -EINVAL;

	de40_inc_units(direntry40_body(item), count);
	
	/* Updating item key */
	if (pos == 0 && direntry40_units(item) > 0)
		direntry40_get_key(item, 0, &item->key);

	return len;
}

/* Prepares area new item will be created at */
static errno_t direntry40_init(item_entity_t *item) {
	aal_assert("umka-1010", item != NULL);
	aal_assert("umka-2215", item->body != NULL);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

/* Prints direntry item into passed @stream */
static errno_t direntry40_print(item_entity_t *item,
				aal_stream_t *stream,
				uint16_t options) 
{
	uint32_t i, j;
	char name[256];
	uint32_t namewidth;
	direntry40_t *direntry;
	uint64_t locality, objectid;
	
	aal_assert("umka-548", item != NULL);
	aal_assert("umka-549", stream != NULL);

	direntry = direntry40_body(item);
	
	aal_stream_format(stream, "DIRENTRY PLUGIN=%s LEN=%u, KEY=",
			  item->plugin->h.label, item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print,
			&item->key, stream, options))
	{
		return -EINVAL;
	}
	
	aal_stream_format(stream, " UNITS=%u\n",
			  de40_get_units(direntry));

	aal_stream_format(stream, "NR  NAME%*s OFFSET HASH%*s "
			  "SDKEY%*s\n", 22, " ", 29, " ", 13, " ");
	
	aal_stream_format(stream, "----------------------------"
			  "------------------------------------"
			  "-----------------------\n");
	
	/* Loop though the all entries */
	for (i = 0; i < de40_get_units(direntry); i++) {
		entry40_t *entry = &direntry->entry[i];
		objid_t *objid = direntry40_objid(item, i);

		direntry40_get_name(item, i, name);

		/* Cutting name by 25 symbols */
		if (aal_strlen(name) > 25) {
			for (j = 0; j < 3; j++)
				name[23 + j] = '.';

			name[23 + j] = '\0';
		}

		locality = ob40_get_locality(objid);
		objectid = ob40_get_objectid(objid);
		
		namewidth = aal_strlen(name) < 25 ? 25 -
			aal_strlen(name) + 1 : 1;

		aal_stream_format(stream, "%*d %s%*s %*u %.16llx:%.16llx "
				  "%.7llx:%.7llx\n", 3, i, name, namewidth, " ", 6,
				  entry->offset, ha40_get_objectid(&entry->hash),
				  ha40_get_offset(&entry->hash), locality, objectid);
	}

	return 0;
}

/* Returns real maximal key in direntry item */
static errno_t direntry40_maxreal_key(item_entity_t *item, 
				      key_entity_t *key) 
{
	uint32_t units;

	aal_assert("umka-1651", key != NULL);
	aal_assert("umka-1650", item != NULL);

	units = direntry40_units(item);
	aal_assert("umka-1653", units > 0);
	
	return direntry40_get_key(item, units - 1, key);
}

extern errno_t direntry40_check(item_entity_t *item,
				uint8_t mode);

#endif

/*
  Returns maximal possible key in direntry item. It is needed for lookuping
  needed entry by entry key.
*/
static errno_t direntry40_maxposs_key(item_entity_t *item, 
				      key_entity_t *key) 
{
	key_entity_t *maxkey;

	aal_assert("umka-1649", key != NULL);
	aal_assert("umka-1648", item != NULL);
	aal_assert("umka-716", key->plugin != NULL);

	plugin_call(item->key.plugin->key_ops,
		    assign, key, &item->key);

	maxkey = plugin_call(key->plugin->key_ops,
			     maximal,);
    
    	plugin_call(key->plugin->key_ops, set_objectid,
		    key, plugin_call(key->plugin->key_ops,
				     get_objectid, maxkey));
	
	plugin_call(key->plugin->key_ops, set_offset,
		    key, plugin_call(key->plugin->key_ops,
				     get_offset, maxkey));
	
	return 0;
}

/* 
   Helper function that is used by lookup method for comparing given key with
   passed entry hash.
*/
static int callback_comp_entry(void *array, uint32_t pos,
			       void *key, void *data)
{
	key_entity_t current;

	direntry40_get_key((item_entity_t *)data, pos, &current);

	return plugin_call(((item_entity_t *)data)->key.plugin->key_ops,
			   compare, &current, (key_entity_t *)key);
}

/* Performs lookup inside direntry. Found pos is stored in @pos */
static lookup_t direntry40_lookup(item_entity_t *item,
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
	direntry40_maxposs_key(item, &maxkey);

	/*
	  If looked key is greater that maximal possible one then we going out
	  and return FALSE, that is the key not found.
	*/
	if (plugin_call(key->plugin->key_ops, compare, key, &maxkey) > 0) {
		*pos = direntry40_units(item);
		return LP_ABSENT;
	}

	/* Comparing looked key with minimal one (that is with item key) */
	if (plugin_call(key->plugin->key_ops, compare, &item->key, key) > 0) {
		*pos = 0;
		return LP_ABSENT;
	}

	/*
	  Performing binary search inside the direntry in order to find position
	  of the looked key.
	*/
	res = aux_bin_search(item->body, direntry40_units(item), key,
			     callback_comp_entry, (void *)item, pos);

	/*
	  Position correcting for the case key was not found. It is needed for
	  the case when we are going to insert new entry and searching the
	  position of insertion.
	*/
	if (res == LP_FAILED)
		return res;

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
#ifndef ENABLE_STAND_ALONE
			.desc = "Compound direntry for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
		
#ifndef ENABLE_STAND_ALONE	    
		.init		= direntry40_init,
		.copy		= direntry40_copy,
		.insert		= direntry40_insert,
		.remove		= direntry40_remove,
		.estimate	= direntry40_estimate,
		.check		= direntry40_check,
		.print		= direntry40_print,
		.shift          = direntry40_shift,
		.predict        = direntry40_predict,
		.feel           = direntry40_feel,
		.maxreal_key    = direntry40_maxreal_key,
		
		.write		= NULL,
		.set_key	= NULL,
		.gap_key	= NULL,
		.layout		= NULL,
		.layout_check	= NULL,
#endif
		.branch         = NULL,

		.data		= direntry40_data,
		.lookup		= direntry40_lookup,
		.units		= direntry40_units,
		.read           = direntry40_read,
		.mergeable      = direntry40_mergeable,
		
		.get_key	= direntry40_get_key,
		.maxposs_key	= direntry40_maxposs_key
	}
};

static reiser4_plugin_t *direntry40_start(reiser4_core_t *c) {
	core = c;
	return &direntry40_plugin;
}

plugin_register(direntry40, direntry40_start, NULL);

