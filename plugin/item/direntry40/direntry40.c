/*
  direntry40.c -- reiser4 default direntry plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "direntry40.h"

static reiser4_core_t *core = NULL;

direntry40_t *direntry40_body(item_entity_t *item) {
	return (direntry40_t *)item->body;
}

static inline objid40_t *direntry40_unit(direntry40_t *direntry, 
				  uint32_t pos)
{
	aal_assert("umka-1593", direntry != NULL, return NULL);

	return (objid40_t *)((void *)direntry + direntry->entry[pos].offset);
}

static inline entry40_t *direntry40_entry(direntry40_t *direntry, 
				  uint32_t pos)
{
	aal_assert("umka-1596", direntry != NULL, return NULL);

	return &direntry->entry[pos];
}

static inline uint32_t direntry40_unitlen(direntry40_t *direntry, 
					  uint32_t pos) 
{
	aal_assert("umka-1594", direntry != NULL, return 0);

	return aal_strlen((char *)(direntry40_unit(direntry, pos) + 1)) +
		sizeof(objid40_t) + 1;
}

/* Builds full key by entry components */
static errno_t direntry40_unitkey(item_entity_t *item,
				  entry40_t *entry,
				  reiser4_key_t *key)
{
	uint64_t offset;
	roid_t locality;
	roid_t objectid;

	aal_assert("umka-1605", entry != NULL, return -1);
	aal_assert("umka-1606", key != NULL, return -1);
	aal_assert("umka-1607", item != NULL, return -1);
	
	locality = plugin_call(return -1, item->key.plugin->key_ops,
			       get_locality, item->key.body);

	objectid = *((uint64_t *)&entry->entryid);
	offset = *((uint64_t *)&entry->entryid + 1);

	key->plugin = item->key.plugin;
	plugin_call(return -1, item->key.plugin->key_ops, build_generic,
		    key->body, KEY_FILENAME_TYPE, locality, objectid, offset);

	return 0;
}

static uint32_t direntry40_count(item_entity_t *item) {
	direntry40_t *direntry;
    
	aal_assert("umka-865", item != NULL, return 0);

	direntry = direntry40_body(item);
	return de40_get_count(direntry);
}

static errno_t direntry40_fetch(item_entity_t *item, uint32_t pos,
				void *buff, uint32_t count)
{
	entry40_t *entry;
	objid40_t *objid;
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
    
	if (pos > direntry40_count(item))
		return -1;

	entry = direntry40_entry(direntry, pos);
	hint->entryid.objectid = eid_get_objectid(&entry->entryid);
	hint->entryid.offset = eid_get_offset(&entry->entryid);

	objid = direntry40_unit(direntry, pos);
	hint->objid.objectid = oid_get_objectid(objid);
	hint->objid.locality = oid_get_locality(objid);
	hint->name = (char *)(objid + 1);
    
	return 0;
}

#ifndef ENABLE_COMPACT

static int direntry40_mergeable(item_entity_t *item1,
				item_entity_t *item2)
{
	reiser4_plugin_t *plugin;
	roid_t locality1, locality2;
	
	aal_assert("umka-1581", item1 != NULL, return -1);
	aal_assert("umka-1582", item2 != NULL, return -1);

	/* FIXME-UMKA: Here should not be hardcoded key plugin id */
	if (!(plugin = core->factory_ops.ifind(KEY_PLUGIN_TYPE,
					       KEY_REISER40_ID)))
	{
		aal_exception_error("Can't find key plugin by its id 0x%x",
				    KEY_REISER40_ID);
		return -1;
	}
	
	locality1 = plugin_call(return -1, plugin->key_ops,
				get_locality, &item1->key);

	locality2 = plugin_call(return -1, plugin->key_ops,
				get_locality, &item2->key);

	return (locality1 == locality2);
}

static errno_t direntry40_estimate(item_entity_t *item, uint32_t pos,
				   reiser4_item_hint_t *hint) 
{
	int i;
	reiser4_direntry_hint_t *direntry_hint;
	    
	aal_assert("vpf-095", hint != NULL, return -1);
    
	direntry_hint = (reiser4_direntry_hint_t *)hint->hint;
	hint->len = direntry_hint->count * sizeof(entry40_t);
    
	for (i = 0; i < direntry_hint->count; i++) {
		hint->len += aal_strlen(direntry_hint->entry[i].name) + 
			sizeof(objid40_t) + 1;
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
	
	direntry40_t *direntry;
	
	aal_assert("umka-1591", src_item != NULL, return 0);
	aal_assert("umka-1592", hint != NULL, return 0);

	space = hint->part;
	
	src_units = direntry40_count(src_item);

	dst_units = 0;
	
	if (dst_item)
		dst_units = direntry40_count(dst_item);

	if (!dst_item || !direntry40_mergeable(src_item, dst_item))
		space -= sizeof(direntry40_t);

	cur = (hint->flags & SF_LEFT ? 0 : src_units - 1);
	
	if (!(direntry = direntry40_body(src_item)))
		return -1;

	flags = hint->flags;
	hint->flags &= ~SF_MOVIP;
	
	while (!(hint->flags & SF_MOVIP) && cur < direntry40_count(src_item)) {
			
		len = direntry40_unitlen(direntry, cur);

		if (space < len + sizeof(entry40_t))
			break;

		if (src_item->pos == 0 && hint->pos.unit != ~0ul) {
			
			if (!(flags & SF_MOVIP)) {
				if (flags & SF_LEFT) {
					if (hint->pos.unit == 0)
						break;
				} else {
					if (hint->pos.unit == src_units - 1)
						break;
				}
			}

			if (flags & SF_LEFT) {
				if (hint->pos.unit == 0) {
					hint->flags |= SF_MOVIP;
					hint->pos.unit = dst_units;
				} else
					hint->pos.unit--;
			} else {
				if (hint->pos.unit >= src_units - 1) {
					hint->flags |= SF_MOVIP;
					hint->pos.unit = 0;

					if (hint->pos.unit > src_units - 1)
						break;
				}
			}
		}

		src_units--;
		dst_units++;
		hint->units++;

		cur += (flags & SF_LEFT ? -1 : 1);
		space -= (len + sizeof(entry40_t));
	}
	
	hint->part -= space;
	
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
	
	direntry40_t *src_direntry;
	direntry40_t *dst_direntry;
	uint32_t src_units, dst_units, units;
	
	aal_assert("umka-1586", src_item != NULL, return -1);
	aal_assert("umka-1587", dst_item != NULL, return -1);
	aal_assert("umka-1589", hint != NULL, return -1);

	units = hint->units;
	
	if (!(src_direntry = direntry40_body(src_item)))
		return -1;
	
	if (!(dst_direntry = direntry40_body(dst_item)))
		return -1;

	src_units = de40_get_count(src_direntry);
	dst_units = de40_get_count(dst_direntry);
	
	aal_assert("umka-1604", src_units >= hint->units, return -1);

	if (hint->flags & SF_LEFT) {
		
		if (dst_units > 0) {

			len = dst_item->len - hint->part;
			
			/* Moving entry headers of dst direntry */
			src = (void *)dst_direntry + sizeof(direntry40_t) +
				(dst_units * sizeof(entry40_t));
			
			dst = src + (hint->units * sizeof(entry40_t));

			size = len - sizeof(direntry40_t) -
				dst_units * sizeof(entry40_t);

			aal_memmove(dst, src, size);

			/* Updating offsets of dst direntry */
			entry = direntry40_entry(dst_direntry, 0);

			for (i = 0; i < dst_units; i++, entry++) {
				uint32_t inc = hint->units * sizeof(entry40_t);
				en40_inc_offset(entry, inc);
			}
		}

                /* Copying entry headers */
		src = (void *)src_direntry + sizeof(direntry40_t);

		dst = (void *)dst_direntry + sizeof(direntry40_t) +
			(dst_units * sizeof(entry40_t));
		
		size = hint->units * sizeof(entry40_t);

		aal_memcpy(dst, src, size);

		/* Copyings entry bodies */
		src = (void *)src_direntry + en40_get_offset((entry40_t *)src);

		dst = (void *)dst_direntry + sizeof(direntry40_t) +
			((dst_units + hint->units) * sizeof(entry40_t));
			
		size = hint->part - (hint->units * sizeof(entry40_t));

		/* FIXME-UMKA: Is this enough reliable? */
		if (dst_units == 0)
			size -= sizeof(direntry40_t);

		aal_memcpy(dst, src, size);

		/* Updating offset of dst direntry */
		entry = direntry40_entry(dst_direntry, dst_units);
		offset = dst - (void *)dst_direntry;
			
		for (i = 0; i < hint->units; i++, entry++) {
			en40_set_offset(entry, offset);
			offset += direntry40_unitlen(dst_direntry, i);
		}

		if (src_units > hint->units) {
			
			/* Moving headers of the src direntry */
			src = (void *)src_direntry + sizeof(direntry40_t) +
				(hint->units * sizeof(entry40_t));
			
			dst = (void *)src_direntry + sizeof(direntry40_t);

			size = src_item->len - sizeof(direntry40_t) -
				(hint->units * sizeof(entry40_t));

			offset = ((entry40_t *)dst)->offset;
			
			aal_memmove(dst, src, size);

			/* Moving bodies of the src direntry */
			src = (void *)src_direntry + offset;

			dst = direntry40_entry(src_direntry,
					       src_units - hint->units);

			size = src_item->len - sizeof(direntry40_t) -
				(src_units * sizeof(entry40_t)) -
				(hint->part - (hint->units * sizeof(entry40_t)));

			if (dst_units == 0)
				size += sizeof(direntry40_t);
			
			aal_memmove(dst, src, size);
			
			/* Updating offsets of src direntry */
			entry = direntry40_entry(src_direntry, 0);
			
			for (i = 0; i < src_units - hint->units; i++, entry++) {
				uint32_t dec = hint->part -
					(dst_units == 0 ? sizeof(direntry40_t) : 0);
				
				en40_dec_offset(entry, dec);

				aal_assert("umka-1641", en40_get_offset(entry) < src_item->len,
					   return -1);
			}
		}
		
		/* Updating items key */
		entry = direntry40_entry(src_direntry, 0);

		if (direntry40_unitkey(src_item, entry, &src_item->key))
			return -1;
	} else {

		if (dst_units > 0) {

			len = dst_item->len - hint->part;
			
			/* Moving entry headers of dst direntry */
			src = (void *)dst_direntry + sizeof(direntry40_t);
			dst = src + (hint->units * sizeof(entry40_t));
			size = len - sizeof(direntry40_t);

			aal_memmove(dst, src, size);

			/* Updating offsets of dst direntry */
			entry = (entry40_t *)dst;

			for (i = 0; i < dst_units; i++, entry++)
				en40_inc_offset(entry, hint->part);
			
			/* Moving entry bodies of dst direntry */
			src = dst + (dst_units * sizeof(entry40_t));

			dst = src + (hint->part - (hint->units *
						   sizeof(entry40_t)));
				
			size -= (dst_units * sizeof(entry40_t));
			
			aal_memmove(dst, src, size);
		}
		
		/* Copying entry headers */
		src = (void *)src_direntry + sizeof(direntry40_t) +
			((src_units - hint->units) * sizeof(entry40_t));

		dst = (void *)dst_direntry + sizeof(direntry40_t);
		size = hint->units * sizeof(entry40_t);

		aal_memcpy(dst, src, size);

		/* Copyings entry bodies */
		src = (void *)src_direntry + en40_get_offset((entry40_t *)src);

		dst = (void *)dst_direntry + sizeof(direntry40_t) +
			((hint->units + dst_units) * sizeof(entry40_t));
			
		size = hint->part - (hint->units * sizeof(entry40_t));

		/* FIXME-UMKA: Is this enough reliable? */
		if (dst_units == 0)
			size -= sizeof(direntry40_t);

		aal_memcpy(dst, src, size);

		/* Updating offset of dst direntry */
		entry = direntry40_entry(dst_direntry, 0);
		offset = dst - (void *)dst_direntry;
			
		for (i = 0; i < hint->units; i++, entry++) {
			en40_set_offset(entry, offset);
			offset += direntry40_unitlen(dst_direntry, i);
		}

		if (src_units > hint->units) {
			
			/* Moving bodies of the src direntry */
			src = (void *)src_direntry + sizeof(direntry40_t) +
				(src_units * sizeof(entry40_t));
			
			dst = src - (hint->units * sizeof(entry40_t));

			size = src_item->len - sizeof(direntry40_t) -
				(src_units * sizeof(entry40_t));
			
			aal_memmove(dst, src, size);

			/* Updating offsets of src direntry */
			entry = direntry40_entry(src_direntry, 0);
			
			for (i = 0; i < src_units - hint->units; i++, entry++) {
				uint32_t offset = hint->units * sizeof(entry40_t);
				en40_dec_offset(entry, offset);
			}
		}
		
		/* Updating items key */
		entry = direntry40_entry(dst_direntry, 0);

		if (direntry40_unitkey(dst_item, entry, &dst_item->key))
			return -1;
	}

	if (dst_units == 0)
		hint->part -= sizeof(direntry40_t);

	de40_inc_count(dst_direntry, hint->units);
	de40_dec_count(src_direntry, hint->units);

	return 0;
}

static errno_t direntry40_insert(item_entity_t *item, uint32_t pos,
				 reiser4_item_hint_t *hint)
{
	uint32_t units;
	void *src, *dst;
	uint32_t i, offset;
	uint32_t headers_size;

	uint32_t len_after = 0;
	uint32_t len_before = 0;
    
	direntry40_t *direntry;
	reiser4_direntry_hint_t *dh;
    
	aal_assert("umka-791", item != NULL, return -1);
	aal_assert("umka-792", hint != NULL, return -1);
	aal_assert("umka-897", pos != ~0ul, return -1);
	aal_assert("umka-1600", hint->len > 0, return -1);

	if (!(direntry = direntry40_body(item)))
		return -1;
    
	dh = (reiser4_direntry_hint_t *)hint->hint;
    
	if (pos > de40_get_count(direntry))
		return -1;

	units = de40_get_count(direntry);
	headers_size = dh->count * sizeof(entry40_t);
		
	/* Getting offset of new entry body will be created at */
	if (units > 0) {
		if (pos < de40_get_count(direntry)) {
			entry40_t *entry = direntry40_entry(direntry, pos);
			offset = en40_get_offset(entry) + headers_size;
		} else {
			entry40_t *entry = direntry40_entry(direntry, units - 1);
			offset = en40_get_offset(entry) + sizeof(entry40_t) +
				direntry40_unitlen(direntry, units - 1);
		}
	} else
		offset = sizeof(direntry40_t) + headers_size;

	/* Calculating length of areas to be moved */
	len_before = (units - pos) * sizeof(entry40_t);
	
	for (i = 0; i < pos; i++)
		len_before += direntry40_unitlen(direntry, i);
	
	for (i = pos; i < units; i++)
		len_after += direntry40_unitlen(direntry, i);
	
	/* Updating offsets */
	for (i = 0; i < pos; i++)
		en40_inc_offset(&direntry->entry[i], headers_size);
    
	for (i = pos; i < units; i++)
		en40_inc_offset(&direntry->entry[i], hint->len);
    
	/* Moving unit bodies */
	if (pos < units) {
		dst = (void *)direntry + offset + hint->len -
			headers_size;

		src = (void *)direntry + offset - headers_size;
		
		aal_memmove(dst, src, len_after);
	}
    
	/* Moving unit headers headers */
	if (len_before) {
		dst = &direntry->entry[pos] + dh->count;
		src = &direntry->entry[pos];
		
		aal_memmove(dst, src, len_before);
	}
    
	/* Creating new entries */
	for (i = 0; i < dh->count; i++) {
		uint32_t len = aal_strlen(dh->entry[i].name);
		entry40_t *entry = direntry40_entry(direntry, pos + i);
		objid40_t *objid = (objid40_t *)((void *)direntry + offset);
		
		en40_set_offset(entry, offset);

		aal_memcpy(&entry->entryid, &dh->entry[i].entryid,
			   sizeof(entryid40_t));
	
		aal_memcpy(objid, &dh->entry[i].objid, sizeof(objid40_t));

		aal_memcpy((void *)direntry + offset + sizeof(objid40_t),
			   dh->entry[i].name, len);

		offset += len + sizeof(objid40_t);
		*((char *)(direntry) + offset++) = '\0';
	}
    
	/* Updating direntry count field */
	de40_inc_count(direntry, dh->count);

	if (pos == 0) {
		entry40_t *entry = direntry40_entry(direntry, 0);

		if (direntry40_unitkey(item, entry, &item->key))
			return -1;
	}
    
	return 0;
}

static errno_t direntry40_init(item_entity_t *item, 
			       reiser4_item_hint_t *hint)
{
	direntry40_t *direntry;
    
	aal_assert("umka-1010", item != NULL, return -1);

	if (!(direntry = direntry40_body(item)))
		return -1;
    
	de40_set_count(direntry, 0);
	return direntry40_insert(item, 0, hint);
}

static uint16_t direntry40_remove(item_entity_t *item, 
				  uint32_t pos)
{
	void *src, *dst;
	uint16_t unit_len;
	uint32_t head_len;
	uint32_t foot_len;
	uint32_t i, offset;

	entry40_t *entry;
	direntry40_t *direntry;
    
	aal_assert("umka-934", item != NULL, return 0);

	if (!(direntry = direntry40_body(item)))
		return -1;
    
	if (pos >= de40_get_count(direntry))
		return 0;

	offset = en40_get_offset(&direntry->entry[pos]);
    
	head_len = offset - sizeof(entry40_t) -
		(((void *)&direntry->entry[pos]) - ((void *)direntry));

	unit_len = direntry40_unitlen(direntry, pos);

	entry = direntry40_entry(direntry, pos);
	aal_memmove(entry, entry + 1, head_len);

	for (i = 0; i < pos; i++)
		en40_dec_offset(&direntry->entry[i], sizeof(entry40_t));
    
	if (pos < (uint32_t)de40_get_count(direntry) - 1) {
		uint32_t dec_len = sizeof(entry40_t) + unit_len;
		offset = en40_get_offset(&direntry->entry[pos]);
	
		foot_len = 0;
		for (i = pos; i < (uint32_t)(de40_get_count(direntry) - 1); i++)
			foot_len += direntry40_unitlen(direntry, i);

		src = (void *)direntry + offset;
		dst = ((void *)direntry + offset) - dec_len;
		
		aal_memmove(dst, src, foot_len);

		for (i = pos; i < (uint32_t)(de40_get_count(direntry) - 1); i++)
			en40_dec_offset(&direntry->entry[i], dec_len);
	}
    
	de40_dec_count(direntry, 1);

	if (pos == 0) {
		entry40_t *entry = direntry40_entry(direntry, 0);

		if (direntry40_unitkey(item, entry, &item->key))
			return -1;
	}
	
	return unit_len + sizeof(entry40_t);
}

static errno_t direntry40_print(item_entity_t *item, aal_stream_t *stream,
				uint16_t options) 
{
	uint32_t i, width;
	direntry40_t *direntry;

	char *name;
	uint64_t objid, offset;
	uint64_t locality, objectid;
	
	aal_assert("umka-548", item != NULL, return -1);
	aal_assert("umka-549", stream != NULL, return -1);

	if (!(direntry = direntry40_body(item)))
		return -1;
	
	aal_stream_format(stream, "count:\t\t%u\n", de40_get_count(direntry));

	for (i = 0; i < de40_get_count(direntry); i++) {
		entry40_t *entry = &direntry->entry[i];

		objid = *((uint64_t *)entry->entryid.objectid);
		offset = *((uint64_t *)entry->entryid.offset);
		name = (void *)direntry + entry->offset + sizeof(objid40_t);

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
				       reiser4_key_t *key) 
{
	uint64_t offset;
	roid_t objectid;
	
	reiser4_body_t *maxkey;
    
	aal_assert("umka-716", key->plugin != NULL, return -1);

	plugin_call(return -1, key->plugin->key_ops,
		    assign, key->body, item->key.body);

	maxkey = plugin_call(return -1, key->plugin->key_ops,
			     maximal,);
    
	objectid = plugin_call(return -1, key->plugin->key_ops,
			       get_objectid, maxkey);
    
	offset = plugin_call(return -1, key->plugin->key_ops, 
			     get_offset, maxkey);
    
	plugin_call(return -1, key->plugin->key_ops, set_objectid, 
		    key->body, objectid);
    
	plugin_call(return -1, key->plugin->key_ops, set_offset, 
		    key->body, offset);
    
	return 0;
}

/* 
   Helper function that is used by lookup method for getting n-th element of 
   direntry.
*/
static inline void *callback_get_entry(void *array,
				       uint32_t pos,
				       void *data) 
{
	direntry40_t *direntry = (direntry40_t *)array;
	return &direntry->entry[pos];
}

/* 
   Helper function that is used by lookup method for comparing given key with
   passed dirid.
*/
static inline int callback_comp_entry(void *entry,
	void *key, void *data)
{
	item_entity_t *item;
	reiser4_key_t *lookkey;
	reiser4_key_t entrykey;

	item = (item_entity_t *)data;
	lookkey = (reiser4_key_t *)key;
	
	if (direntry40_unitkey(item, (entry40_t *)entry, &entrykey))
		return -1;
    
	return plugin_call(return -1, item->key.plugin->key_ops, compare,
			   entrykey.body, lookkey->body);
}

static int direntry40_lookup(item_entity_t *item,
			     reiser4_key_t *key, uint32_t *pos)
{
	int lookup;
	uint64_t unit;
	uint32_t units;
    
	direntry40_t *direntry;
	reiser4_key_t maxkey, minkey;

	aal_assert("umka-610", key != NULL, return -1);
	aal_assert("umka-717", key->plugin != NULL, return -1);
    
	aal_assert("umka-609", item != NULL, return -1);
	aal_assert("umka-629", pos != NULL, return -1);
    
	if (!(direntry = direntry40_body(item)))
		return -1;
    
	maxkey.plugin = key->plugin;
	plugin_call(return -1, maxkey.plugin->key_ops,
		    assign, maxkey.body, item->key.body);
	
	if (direntry40_max_poss_key(item, &maxkey))
		return -1;

	units = direntry40_count(item);
	
	if (plugin_call(return -1, key->plugin->key_ops,
			compare, key->body, maxkey.body) > 0)
	{
		*pos = units;
		return 0;
	}
    
	minkey.plugin = key->plugin;
	plugin_call(return -1, minkey.plugin->key_ops,
		    assign, minkey.body, item->key.body);

	if (plugin_call(return -1, key->plugin->key_ops,
			compare, minkey.body, key->body) > 0)
	{
		*pos = 0;
		return 0;
	}
    
	lookup = aux_binsearch((void *)direntry, units, key, callback_get_entry,
			       callback_comp_entry, (void *)item, &unit);

	if (lookup != -1) {
		*pos = (uint32_t)unit;
		if (lookup == 0) (*pos)++;
	}
    
	return lookup;
}

static reiser4_plugin_t direntry40_plugin = {
	.item_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = ITEM_CDE40_ID,
				.group = DIRENTRY_ITEM,
				.type = ITEM_PLUGIN_TYPE
			},
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
#endif
		.valid		= NULL,
		.open           = NULL,
		.update         = NULL,
		
		.lookup		= direntry40_lookup,
		.count		= direntry40_count,
		.fetch          = direntry40_fetch,
		
		.max_poss_key	= direntry40_max_poss_key,
		.max_real_key   = NULL
	}
};

static reiser4_plugin_t *direntry40_start(reiser4_core_t *c) {
	core = c;
	return &direntry40_plugin;
}

plugin_register(direntry40_start, NULL);

