/*
  reg40.c -- reiser4 default regular file plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT
#  include <time.h>
#  include <unistd.h>
#endif

#include "reg40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t reg40_plugin;

/*
  Resets file position. That is it searches first body item and sets file's
  offset to zero.
*/
static errno_t reg40_reset(object_entity_t *entity) {
	reg40_t *reg;
	uint64_t size;
	key_entity_t key;
    
	aal_assert("umka-1161", entity != NULL, return -1);

	reg = (reg40_t *)entity;
	
	if ((size = file40_get_size(&reg->file)) == 0)
		return 0;

	/* Building body key to be found */
	key.plugin = reg->file.key.plugin;
	
	plugin_call(key.plugin->key_ops, build_generic, &key,
		    KEY_FILEBODY_TYPE, file40_locality(&reg->file),
		    file40_objectid(&reg->file), 0);
    
	file40_unlock(&reg->file, &reg->body);

	/*
	  Perform lookup with instruction to stop on the leaf level. In the case
	  first item is extent, we will stop on twig level.
	*/
	if (file40_lookup(&reg->file, &key, LEAF_LEVEL, &reg->body) != PRESENT)	{
		/*
		  Cleaning body node. It is needed because functions below check
		  this in order to determine is file has a body or not.
		*/
		reg->body.node = NULL;
	}
	
	/*
	  Locking node the current body lies in, due to prevent the throwing it
	  out of tree cache.
	*/
	file40_lock(&reg->file, &reg->body);

	reg->offset = 0;

	return 0;
}

/* Updates body coord in correspond to file offset */
static errno_t reg40_next(reg40_t *reg) {
	errno_t res;
	key_entity_t key;
	reiser4_place_t place;

	/* Building key to be searched by current offset */
	key.plugin = reg->file.key.plugin;
	
	plugin_call(key.plugin->key_ops, build_generic, &key,
		    KEY_FILEBODY_TYPE, file40_locality(&reg->file), 
		    file40_objectid(&reg->file), reg->offset);

        /* Unlocking the old body */
	file40_unlock(&reg->file, &reg->body);

	place = reg->body;
	
	/* Getting the next body item from the tree */
	res = file40_lookup(&reg->file, &key, LEAF_LEVEL, &reg->body);

	if (res != PRESENT) {
		/*
		  Restoring previous body place. It is needed because we provide
		  the behavior which makes user sure that if next position is
		  not found, we should stay at the end of file.
		*/
		reg->body = place;
	}

	/* Locking new body or old one if lookup failed */
	file40_lock(&reg->file, &reg->body);
	
	return res;
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

	aal_assert("umka-1183", buff != NULL, return 0);
	aal_assert("umka-1182", entity != NULL, return 0);

	size = file40_get_size(&reg->file);

	/* The file has not data at all */
	if (size == 0 || !reg->body.node)
		return 0;

	if (n > size - reg->offset)
		n = size - reg->offset;

	/*
	  Reading data from the file. As we do not know item types, we just
	  call item's fetch method.
	*/
	for (read = 0; read < n; read += chunk) {
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

		/* Calling body item's "fetch" method */
		chunk = plugin_call(item->plugin->item_ops,
				    fetch, item, buff, offset, chunk);

		if (chunk == 0)
			return read;
		
		buff += chunk;
		reg->offset += chunk;

		/* Getting new body item by current file offset */
		reg40_next(reg);
	}

	return read;
}

/* Opening reg40 by statdata coord passed in @place */
static object_entity_t *reg40_open(void *tree, 
				   reiser4_place_t *place) 
{
	reg40_t *reg;
	key_entity_t *key;

	aal_assert("umka-1163", tree != NULL, return NULL);
	aal_assert("umka-1164", place != NULL, return NULL);
    
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	key = &place->item.key;

	/* Initializing file handle */
	if (file40_init(&reg->file, &reg40_plugin, key, core, tree))
		goto error_free_reg;

	/* saving statdata coord and looking the code it lies in */
	aal_memcpy(&reg->file.statdata, place, sizeof(*place));
	file40_lock(&reg->file, &reg->file.statdata);

	/* Position onto the first body item */
	if (reg40_reset((object_entity_t *)reg)) {
		aal_exception_error("Can't reset file 0x%llx.", 
				    file40_objectid(&reg->file));
		goto error_free_reg;
	}
    
	return (object_entity_t *)reg;

 error_free_reg:
	aal_free(reg);
	return NULL;
}

#ifndef ENABLE_COMPACT

/* Creating the file described by pased @hint */
static object_entity_t *reg40_create(void *tree, 
				     reiser4_file_hint_t *hint) 
{
	reg40_t *reg;
	reiser4_place_t place;
	
	roid_t parent_locality;
	roid_t objectid, locality;

	reiser4_plugin_t *stat_plugin;
    
	reiser4_item_hint_t stat_hint;
	reiser4_statdata_hint_t stat;
    
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;
	
	aal_assert("umka-1169", tree != NULL, return NULL);
	aal_assert("umka-1738", hint != NULL, return NULL);

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	reg->offset = 0;

	/* Initializing file handle */
	if (file40_init(&reg->file, &reg40_plugin, &hint->object, core, tree))
		goto error_free_reg;
	
	locality = file40_locality(&reg->file);
    	objectid = file40_objectid(&reg->file);

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
    
	stat_hint.key.plugin = hint->object.plugin;
	
	plugin_call(hint->object.plugin->key_ops, assign, 
		    &stat_hint.key, &hint->object);
    
	/* Initializing stat data item hint. */
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID;
    
	lw_ext.mode = S_IFREG | 0755;
	lw_ext.nlink = 2;

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

	stat_hint.hint = &stat;

	/* Insert statdata item into the tree */
	if (file40_insert(&reg->file, &stat_hint, LEAF_LEVEL, &place))
		goto error_free_reg;

	aal_memcpy(&reg->file.statdata, &place, sizeof(place));
    
	return (object_entity_t *)reg;

 error_free_reg:
	aal_free(reg);
	return NULL;
}

static errno_t reg40_truncate(object_entity_t *entity, uint64_t n) {
	return -1;
}

/* 
   Returns plugin (tail or extent) for next write operation basing on passed
   size to be writen. This function will be using tail policy plugin for find
   out what next item should be writen.
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

static errno_t callback_item_data(item_entity_t *item,
				  blk_t blk, void *data)
{
	layout_hint_t *hint = (layout_hint_t *)data;
	return hint->func(hint->entity, blk, hint->data);
}

/*
  Implements reg40 layout function. It traverses all blocks belong to the file
  and needed for calculating fragmentation, printing, etc.
*/
static errno_t reg40_layout(object_entity_t *entity,
			    block_func_t func,
			    void *data)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	key_entity_t key;
	layout_hint_t hint;
	
	aal_assert("umka-1471", entity != NULL, return -1);
	aal_assert("umka-1472", func != NULL, return -1);

	reg = (reg40_t *)entity;
	
	if ((size = file40_get_size(&reg->file)) == 0)
		return 0;

	hint.func = func;
	hint.data = data;
	hint.entity = entity;
		
	while (reg->offset < size) {
		item_entity_t *item = &reg->body.item;
		
		if (item->plugin->item_ops.layout) {

			if ((res = item->plugin->item_ops.layout(item, 
								 callback_item_data, 
								 &hint)))
				return res;
		} else {
			if ((res = callback_item_data(item, item->con.blk, &hint)))
				return res;
		}
		
		plugin_call(item->plugin->item_ops, utmost_key, item, &key);
		
		reg->offset = plugin_call(key.plugin->key_ops,
					  get_offset, &key) + 1;
		
		reg40_next(reg);
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
	
	aal_assert("umka-1716", entity != NULL, return -1);
	aal_assert("umka-1717", func != NULL, return -1);

	if ((res = func(entity, &reg->file.statdata, data)))
		return res;

	if ((size = file40_get_size(&reg->file)) == 0)
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
		
	aal_assert("umka-1170", entity != NULL, return);

	/* Unlocking statdata and body */
	file40_unlock(&reg->file, &reg->file.statdata);
	file40_unlock(&reg->file, &reg->body);
	
	aal_free(entity);
}

static uint64_t reg40_offset(object_entity_t *entity) {
	aal_assert("umka-1159", entity != NULL, return 0);
	return ((reg40_t *)entity)->offset;
}

static errno_t reg40_seek(object_entity_t *entity, 
			  uint64_t offset) 
{
	return -1;
}

static reiser4_plugin_t reg40_plugin = {
	.file_ops = {
		.h = {
			.handle = empty_handle,
			.id = FILE_REGULAR40_ID,
			.group = REGULAR_FILE,
			.type = FILE_PLUGIN_TYPE,
			.label = "reg40",
			.desc = "Regular file for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_COMPACT
		.create	    = reg40_create,
		.write	    = reg40_write,
		.truncate   = reg40_truncate,
		.layout     = reg40_layout,
		.metadata   = reg40_metadata,
#else
		.create	    = NULL,
		.write	    = NULL,
		.truncate   = NULL,
		.layout     = NULL,
		.metadata   = NULL,
#endif
		.valid	    = NULL,
		.lookup	    = NULL,
		.follow     = NULL,
		
		.open	    = reg40_open,
		.close	    = reg40_close,
		.reset	    = reg40_reset,
		.offset	    = reg40_offset,
		.seek	    = reg40_seek,
		.read	    = reg40_read
	}
};

static reiser4_plugin_t *reg40_start(reiser4_core_t *c) {
	core = c;
	return &reg40_plugin;
}

plugin_register(reg40_start, NULL);
