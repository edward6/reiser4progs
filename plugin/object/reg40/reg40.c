/*
  reg40.c -- reiser4 default regular file plugin.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "reg40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t reg40_plugin;

static uint64_t reg40_size(object_entity_t *entity) {
	reg40_t *reg;

	reg = (reg40_t *)entity;
	
	/* Getting stat data item place */
	obj40_stat(&reg->obj);
	
	return obj40_get_size(&reg->obj);
}

/* Updates body place in correspond to file offset */
static lookup_t reg40_next(reg40_t *reg) {
	lookup_t res;
	place_t place;
	key_entity_t key;

	aal_assert("umka-1161", reg != NULL);
	
	/* Building key to be searched by current offset */
	key.plugin = reg->obj.key.plugin;
	
	plugin_call(key.plugin->key_ops, build_generic, &key,
		    KEY_FILEBODY_TYPE, obj40_locality(&reg->obj), 
		    obj40_objectid(&reg->obj), reg->offset);

	/* Getting the next body item from the tree */
	if ((res = obj40_lookup(&reg->obj, &key, LEAF_LEVEL,
				&place)) == LP_PRESENT)
	{
		/* Unlocking old location */
		if (reg->body.node != NULL)
			obj40_unlock(&reg->obj, &reg->body);

		/* Locking new location */
		reg->body = place;
		obj40_lock(&reg->obj, &reg->body);
	}

	return res;
}

/*
  Resets file position. That is it searches first body item and sets file's
  offset to zero.
*/
static errno_t reg40_reset(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;
    
	aal_assert("umka-1963", reg != NULL);

	reg->offset = 0;

	if (reg40_size(entity) == 0)
		return 0;

	return -(reg40_next(reg) != LP_PRESENT);
}

/* Reads @n bytes to passed buffer @buff */
static int32_t reg40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	uint64_t size;
	uint32_t offset;
	item_entity_t *item;
	uint32_t read, chunk;
	
	reg40_t *reg = (reg40_t *)entity;

	aal_assert("umka-1183", buff != NULL);
	aal_assert("umka-1182", entity != NULL);

	size = reg40_size(entity);

	/* The file has not data at all */
	if (size == 0 || !reg->body.node)
		return 0;

	if (reg->offset >= size)
		return -1;
	
	if (n > size - reg->offset)
		n = size - reg->offset;

	/*
	  Reading data from the file. As we do not know item types, we just call
	  item's read method.
	*/
	for (read = 0; read < n; ) {
		item = &reg->body.item;
		
		if (item->pos.unit == ~0ul)
			item->pos.unit = 0;
			
		if ((chunk = n - read) == 0)
			return read;

		/* Getting item's key offset */
		offset = plugin_call(item->key.plugin->key_ops,
				     get_offset, &item->key);

		/* Calculating in-item local offset */
		offset = reg->offset - offset;

		/* Calling body item's "read" method */
		chunk = plugin_call(item->plugin->item_ops, read,
				    item, buff, offset, chunk);

		if (chunk == 0)
			return read;
		
		buff += chunk;
		read += chunk;
		reg->offset += chunk;

		/* Getting new body item by current file offset */
		if (reg40_next(reg) != LP_PRESENT)
			break;
			
	}

	return read;
}

/* Opening reg40 by statdata place passed in @place */
static object_entity_t *reg40_open(void *tree, place_t *place) {
	reg40_t *reg;
	key_entity_t *key;

	aal_assert("umka-1163", tree != NULL);
	aal_assert("umka-1164", place != NULL);
    
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	key = &place->item.key;

	/* Initializing file handle */
	if (obj40_init(&reg->obj, &reg40_plugin, key, core, tree))
		goto error_free_reg;

	/* Saving statdata place and looking the code it lies in */
	aal_memcpy(&reg->obj.statdata, place, sizeof(*place));
	obj40_lock(&reg->obj, &reg->obj.statdata);

	/* Position onto the first body item */
	if (reg40_reset((object_entity_t *)reg)) {
		aal_exception_error("Can't reset file 0x%llx.", 
				    obj40_objectid(&reg->obj));
		goto error_free_reg;
	}
    
	return (object_entity_t *)reg;

 error_free_reg:
	aal_free(reg);
	return NULL;
}

#ifndef ENABLE_ALONE

/* Creating the file described by pased @hint */
static object_entity_t *reg40_create(void *tree, object_entity_t *parent,
				     reiser4_object_hint_t *hint,
				     place_t *place) 
{
	reg40_t *reg;
	
	roid_t parent_locality;
	roid_t objectid, locality;

	reiser4_plugin_t *stat_plugin;
    
	reiser4_item_hint_t stat_hint;
	reiser4_statdata_hint_t stat;
    
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;
	
	aal_assert("umka-1169", tree != NULL);
	aal_assert("umka-1738", hint != NULL);
	aal_assert("umka-1880", place != NULL);

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	reg->offset = 0;

	/* Initializing file handle */
	if (obj40_init(&reg->obj, &reg40_plugin, &hint->object, core, tree))
		goto error_free_reg;
	
	locality = obj40_locality(&reg->obj);
    	objectid = obj40_objectid(&reg->obj);

	parent_locality = plugin_call(hint->object.plugin->key_ops, 
				      get_locality, &hint->parent);

	/* Getting statdata plugin */
	if (!(stat_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    hint->statdata)))
	{
		aal_exception_error("Can't find stat data item plugin by "
				    "its id 0x%x.", hint->statdata);
		goto error_free_reg;
	}
    
	/* Initializing the stat data hint */
	aal_memset(&stat_hint, 0, sizeof(stat_hint));

	stat_hint.plugin = stat_plugin;
	stat_hint.flags = HF_FORMATD;
	stat_hint.key.plugin = hint->object.plugin;
	
	plugin_call(hint->object.plugin->key_ops, assign, 
		    &stat_hint.key, &hint->object);
    
	/* Initializing stat data item hint. */
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID;
    
	lw_ext.nlink = 1;
	lw_ext.mode = S_IFREG | 0755;

	/* This should be modified by write function later */
	lw_ext.size = 0;
    
	unix_ext.uid = getuid();
	unix_ext.gid = getgid();
	unix_ext.atime = time(NULL);
	unix_ext.mtime = time(NULL);
	unix_ext.ctime = time(NULL);
	unix_ext.rdev = 0;
	unix_ext.bytes = 0;

	aal_memset(&stat.ext, 0, sizeof(stat.ext));
    
	stat.ext[SDEXT_LW_ID] = &lw_ext;
	stat.ext[SDEXT_UNIX_ID] = &unix_ext;

	stat_hint.type_specific = &stat;

	/* Insert statdata item into the tree */
	if (obj40_insert(&reg->obj, &stat_hint, LEAF_LEVEL, place))
		goto error_free_reg;

	aal_memcpy(&reg->obj.statdata, place, sizeof(*place));
	obj40_lock(&reg->obj, &reg->obj.statdata);
    
	if (parent) {
		plugin_call(parent->plugin->object_ops, link,
			    parent);
	}
	
	return (object_entity_t *)reg;

 error_free_reg:
	aal_free(reg);
	return NULL;
}

static errno_t reg40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	return -1;
}

static errno_t reg40_link(object_entity_t *entity) {
	aal_assert("umka-1912", entity != NULL);
	return obj40_link(&((reg40_t *)entity)->obj, 1);
}

static errno_t reg40_unlink(object_entity_t *entity) {
	reg40_t *reg;
	uint64_t size;
	
	aal_assert("umka-1911", entity != NULL);

	reg = (reg40_t *)entity;
	
	if (obj40_stat(&reg->obj))
		return -1;

	if (obj40_link(&reg->obj, -1))
		return -1;

	if (obj40_get_nlink(&reg->obj) > 0)
		return 0;
	
	/* Removing file when nlink became zero */
	if (reg40_reset(entity))
		return -1;
	
	size = obj40_get_size(&reg->obj);

	aal_assert("umka-1913", size > 0);
	
	if (reg40_truncate(entity, size))
		return -1;

	if (obj40_stat(&reg->obj))
		return -1;

	return obj40_remove(&reg->obj, &reg->obj.key, 1);
}

/* 
  Returns plugin (tail or extent) for next write operation basing on passed size
  to be writen. This function will be using tail policy plugin for find out what
  next item should be writen.
*/
static reiser4_plugin_t *reg40_policy(reg40_t *reg, uint32_t size) {
	return core->factory_ops.ifind(ITEM_PLUGIN_TYPE, ITEM_TAIL40_ID);
}

/* Writes "n" bytes from "buff" to passed file. */
static int32_t reg40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	/* Sorry, not implemented yet! */
	return 0;
}

struct layout_hint {
	object_entity_t *entity;
	block_func_t func;
	void *data;
};

typedef struct layout_hint layout_hint_t;

static errno_t callback_item_data(void *object, uint64_t start,
				  uint64_t count, void *data)
{
	item_entity_t *item = (item_entity_t *)object;
	blk_t blk;
	errno_t res;
	
	layout_hint_t *hint = (layout_hint_t *)data;

	for (blk = start; blk < start + count; blk++) {
		if ((res = hint->func(hint->entity, blk, hint->data)))
			return res;
	}

	return 0;
}

/*
  Implements reg40 layout function. It traverses all blocks belong to the file
  and needed for calculating fragmentation, printing, etc.
*/
static errno_t reg40_layout(object_entity_t *entity,
			    block_func_t block_func,
			    void *data)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	key_entity_t key;
	layout_hint_t hint;
	
	aal_assert("umka-1471", entity != NULL);
	aal_assert("umka-1472", block_func != NULL);

	reg = (reg40_t *)entity;
	
	if ((size = reg40_size(entity)) == 0)
		return 0;

	hint.data = data;
	hint.entity = entity;
	hint.func = block_func;
		
	while (reg->offset < size) {
		item_entity_t *item = &reg->body.item;
		
		if (item->plugin->item_ops.layout) {
			res = plugin_call(item->plugin->item_ops, layout,
					  item, callback_item_data, &hint);
			
			if (res != 0)
				return res;
			
		} else {
			if ((res = callback_item_data(item, item->context.blk,
						      1, &hint)))
				return res;
		}
		
		plugin_call(item->plugin->item_ops, utmost_key, item, &key);
		
		reg->offset = plugin_call(key.plugin->key_ops,
					  get_offset, &key) + 1;
		
		if (reg40_next(reg) != LP_PRESENT)
			break;
	}
	
	return 0;
}

/*
  Implements reg40 metadata function. It traverses items belong to the file and
  needed for calculating fragmentation, printing, etc.
*/
static errno_t reg40_metadata(object_entity_t *entity,
			      place_func_t func,
			      void *data)
{
	errno_t res;
	key_entity_t key;
	uint64_t size, offset;
	
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-1716", entity != NULL);
	aal_assert("umka-1717", func != NULL);

	if ((res = func(entity, &reg->obj.statdata, data)))
		return res;

	if ((size = reg40_size(entity)) == 0)
		return 0;
	
	while (reg->offset < size) {
		item_entity_t *item = &reg->body.item;
			
		if ((res = func(entity, &reg->body, data)))
			return res;

		plugin_call(item->plugin->item_ops, utmost_key,
			    item, &key);

		reg->offset = plugin_call(key.plugin->key_ops,
					  get_offset, &key) + 1;

		reg40_next(reg);
	}
	
	return 0;
}

#endif

static void reg40_close(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;
		
	aal_assert("umka-1170", entity != NULL);

	/* Unlocking statdata and body */
	if (reg->obj.statdata.node != NULL)
		obj40_unlock(&reg->obj, &reg->obj.statdata);

	if (reg->body.node != NULL)
		obj40_unlock(&reg->obj, &reg->body);
	
	aal_free(entity);
}

static uint64_t reg40_offset(object_entity_t *entity) {
	aal_assert("umka-1159", entity != NULL);
	return ((reg40_t *)entity)->offset;
}

static errno_t reg40_seek(object_entity_t *entity, 
			  uint64_t offset) 
{
	reg40_t *reg;
	
	aal_assert("umka-1968", entity != NULL);

	reg = (reg40_t *)entity;
	reg->offset = offset;

	return 0;
}

static reiser4_plugin_t reg40_plugin = {
	.object_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = OBJECT_FILE40_ID,
			.group = FILE_OBJECT,
			.type = OBJECT_PLUGIN_TYPE,
			.label = "reg40",
			.desc = "Regular file for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_ALONE
		.create	      = reg40_create,
		.write	      = reg40_write,
		.truncate     = reg40_truncate,
		.layout       = reg40_layout,
		.metadata     = reg40_metadata,
		.link         = reg40_link,
		.unlink       = reg40_unlink,
		
		.add_entry    = NULL,
		.rem_entry    = NULL,
#endif
		.valid	      = NULL,
		.lookup	      = NULL,
		.follow       = NULL,
		.read_entry   = NULL,
		
		.open	      = reg40_open,
		.close	      = reg40_close,
		.reset	      = reg40_reset,
		.offset	      = reg40_offset,
		.seek	      = reg40_seek,
		.size         = reg40_size,
		.read	      = reg40_read
	}
};

static reiser4_plugin_t *reg40_start(reiser4_core_t *c) {
	core = c;
	return &reg40_plugin;
}

plugin_register(reg40_start, NULL);
