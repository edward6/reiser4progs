/*
  reg40.c -- reiser4 default regular file plugin.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "reg40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t reg40_plugin;

static errno_t reg40_truncate(object_entity_t *entity,
			      uint64_t n);


static uint64_t reg40_size(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;

	aal_assert("umka-2278", entity != NULL);
	
#ifndef ENABLE_STAND_ALONE
	/* Updating stat data place */
	if (obj40_stat(&reg->obj))
		return 0;
#endif

	return obj40_get_size(&reg->obj);
}

/* Updates body place in correspond to file offset */
static lookup_t reg40_next(reg40_t *reg) {
	lookup_t res;
	place_t place;
	key_entity_t key;

	aal_assert("umka-1161", reg != NULL);
	
	/* Building key to be searched by current offset */
	plugin_call(STAT_KEY(&reg->obj)->plugin->o.key_ops, build_generic,
		    &key, KEY_FILEBODY_TYPE, obj40_locality(&reg->obj), 
		    obj40_objectid(&reg->obj), reg->offset);

	/* Getting the next body item from the tree */
	if ((res = obj40_lookup(&reg->obj, &key, LEAF_LEVEL,
				&place)) == PRESENT)
	{
		obj40_relock(&reg->obj, &reg->body, &place);

		aal_memcpy(&reg->body, &place,
			   sizeof(reg->body));

		if (reg->body.item.pos.unit == ~0ul)
			reg->body.item.pos.unit = 0;
	}

	return res;
}

/*
  Resets file position. That is it searches first body item and sets file's
  offset to zero.
*/
static errno_t reg40_reset(object_entity_t *entity) {
	aal_assert("umka-1963", entity != NULL);

	((reg40_t *)entity)->offset = 0;
	return 0;
}

/* Reads @n bytes to passed buffer @buff */
static int32_t reg40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	errno_t res;
	reg40_t *reg;
	
#ifndef ENABLE_STAND_ALONE
	uint64_t size;
	uint64_t offset;
#else
	uint32_t size;
	uint32_t offset;
#endif
	
	item_entity_t *item;
	uint32_t read, chunk;

	aal_assert("umka-1183", buff != NULL);
	aal_assert("umka-1182", entity != NULL);

	reg = (reg40_t *)entity;
	size = reg40_size(entity);

	if (reg->offset > size)
		return -EINVAL;
	
	/* The file has not data or nothing can be read */
	if (reg->offset == size)
		return 0;

	if (n > size - reg->offset)
		n = size - reg->offset;

	for (read = 0; read < n; ) {

		if (reg40_next(reg) != PRESENT)
			break;

		item = &reg->body.item;
		
		chunk = n - read;

		/* Calculating in-item offset */
		offset = reg->offset - plugin_call(item->key.plugin->o.key_ops,
						   get_offset, &item->key);

		/* Calling body item's read() method */
		chunk = plugin_call(item->plugin->o.item_ops, read,
				    item, buff, offset, chunk);

		aal_assert("umka-2216", chunk > 0);
		
		reg->offset += chunk;
		buff += chunk; read += chunk;
	}

	return read;
}

#ifndef ENABLE_STAND_ALONE
/*
  Returns plugin (tail or extent) for next write operation basing on passed size
  to be writen. This function will be using tail policy plugin for find out what
  next item should be writen.
*/
static reiser4_plugin_t *reg40_bplug(reg40_t *reg,
				     uint32_t size)
{
	if (reg->body.node)
		return reg->body.item.plugin;
			
	return core->factory_ops.ifind(ITEM_PLUGIN_TYPE,
				       ITEM_TAIL40_ID);
}
#endif

/* Opening reg40 by statdata place passed in @place */
static object_entity_t *reg40_open(void *tree, place_t *place) {
	reg40_t *reg;

	aal_assert("umka-1163", tree != NULL);
	aal_assert("umka-1164", place != NULL);
    	
	if (place->item.plugin->h.group != STATDATA_ITEM)
		return NULL;

	if (obj40_pid(&place->item) != reg40_plugin.h.id)
		return NULL;

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plugin,
		   &place->item.key, core, tree);

	/* Initialziing statdata place */
	aal_memcpy(&reg->obj.statdata, place,
		   sizeof(*place));
	
	obj40_lock(&reg->obj, &reg->obj.statdata);

	/* Reseting file (setting offset to 0) */
	reg40_reset((object_entity_t *)reg);

#ifndef ENABLE_STAND_ALONE
	reg->bplug = reg40_bplug(reg, 0);
#endif

	return (object_entity_t *)reg;
}

#ifndef ENABLE_STAND_ALONE
/* Creating the file described by pased @hint */
static object_entity_t *reg40_create(void *tree, object_entity_t *parent,
				     object_hint_t *hint, place_t *place) 
{
	reg40_t *reg;
	oid_t objectid, locality;
	
	statdata_hint_t stat;
    	create_hint_t stat_hint;
    
	sdext_lw_hint_t lw_ext;
	sdext_unix_hint_t unix_ext;
	
	reiser4_plugin_t *stat_plugin;
	
	aal_assert("umka-1169", tree != NULL);
	aal_assert("umka-1738", hint != NULL);
	aal_assert("umka-1880", place != NULL);

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	reg->offset = 0;
	
	/* Preparing dir oid and locality */
	locality = plugin_call(hint->object.plugin->o.key_ops, get_locality, 
			       &hint->object);
	objectid = plugin_call(hint->object.plugin->o.key_ops, get_objectid, 
			       &hint->object);
	
	/* Key contains valid locality and objectid only, build start key. */
	plugin_call(hint->object.plugin->o.key_ops, build_generic, 
		    &hint->object, KEY_STATDATA_TYPE, locality, objectid, 0);
	
	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plugin, &hint->object, core, tree);
	
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

	plugin_call(hint->object.plugin->o.key_ops, assign, 
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

	switch (obj40_lookup(&reg->obj, &stat_hint.key,
			     LEAF_LEVEL, &reg->obj.statdata))
	{
	case FAILED:
	case PRESENT:
		goto error_free_reg;
	default:
		break;
	}
	
	/* Insert statdata item into the tree */
	if (obj40_insert(&reg->obj, &stat_hint,
			 LEAF_LEVEL, &reg->obj.statdata))
	{
		goto error_free_reg;
	}

	aal_memcpy(place, &reg->obj.statdata, sizeof(*place));
	obj40_lock(&reg->obj, &reg->obj.statdata);
    
	if (parent) {
		plugin_call(parent->plugin->o.object_ops, link,
			    parent);
	}

	reg->bplug = reg40_bplug(reg, 0);
	return (object_entity_t *)reg;

 error_free_reg:
	aal_free(reg);
	return NULL;
}

static errno_t reg40_link(object_entity_t *entity) {
	reg40_t *reg;
	
	aal_assert("umka-1912", entity != NULL);

	reg = (reg40_t *)entity;
	
	/* Updating stat data place */
	if (obj40_stat(&reg->obj))
		return -EINVAL;
	
	return obj40_link(&reg->obj, 1);
}

static errno_t reg40_unlink(object_entity_t *entity) {
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	
	aal_assert("umka-1911", entity != NULL);

	reg = (reg40_t *)entity;
	
	if ((res = obj40_stat(&reg->obj)))
		return res;

	if ((res = obj40_link(&reg->obj, -1)))
		return res;

	if (obj40_get_nlink(&reg->obj) > 0)
		return 0;
	
	/* Removing file when nlink became zero */
	if ((res = reg40_reset(entity)))
		return res;
	
	if ((res = reg40_truncate(entity, 0)))
		return res;

	if ((res = obj40_stat(&reg->obj)))
		return res;

	return obj40_remove(&reg->obj, STAT_KEY(&reg->obj), 1);
}

static uint32_t reg40_chunk(reg40_t *reg) {
	return core->tree_ops.maxspace(reg->obj.tree);
}

/* Writes passed data to the file */
static int32_t reg40_put(object_entity_t *entity, 
			 void *buff, uint32_t n)
{
	errno_t res;
	reg40_t *reg;
	
	uint64_t size;
	uint32_t atime;

	uint32_t written;
	uint32_t maxspace;

	item_entity_t *item;
	sdext_unix_hint_t unix_hint;

	reg = (reg40_t *)entity;
	maxspace = reg40_chunk(reg);
	
	for (written = 0; written < n; ) {
		place_t place;
		lookup_t lookup;
		create_hint_t hint;

		/* Preparing @hint->key */
		hint.key.plugin = STAT_KEY(&reg->obj)->plugin;
		
		plugin_call(hint.key.plugin->o.key_ops, build_generic, &hint.key,
			    KEY_FILEBODY_TYPE, obj40_locality(&reg->obj),
			    obj40_objectid(&reg->obj), reg->offset);

		/* Setting up @hint */
		hint.count = n - written;
		
		if (hint.count > maxspace)
			hint.count = maxspace;
		
		hint.flags = HF_FORMATD;
		hint.plugin = reg->bplug;
		hint.type_specific = buff;
		
		/* Lookup place data will be inserted at */
		lookup = obj40_lookup(&reg->obj, &hint.key,
				      LEAF_LEVEL, &place);

		/* Loookup is failed */
		if (lookup == FAILED) {
			aal_exception_error("Lookup is failed while "
					    "writing file.");
			return -EINVAL;
		}

		/*
		  Checking if we need write chunk by chunk n odrer to rewrite
		  tail correctly in the case we write tail, that overlaps two
		  neighbour tails by key.
		*/
		if (lookup == PRESENT) {
			uint64_t offset;
			key_entity_t maxreal_key;

			plugin_call(place.item.plugin->o.item_ops,
				    maxreal_key, &place.item, &maxreal_key);

			offset = plugin_call(hint.key.plugin->o.key_ops,
					     get_offset, &maxreal_key);

			/* Rewritting only tails' last part */
			if (reg->offset + hint.count > offset + 1)
				hint.count = (offset + 1) - reg->offset;
		}

		/* Inserting data to the tree */
		if ((res = obj40_insert(&reg->obj, &hint,
					LEAF_LEVEL, &place)))
		{
			return res;
		}

		obj40_relock(&reg->obj, &reg->body, &place);
		aal_memcpy(&reg->body, &place, sizeof(reg->body));

		buff += hint.count;
		written += hint.count;
		reg->offset += hint.count;
	}
	
	/* Updating stat data place */
	if ((res = obj40_stat(&reg->obj)))
		return res;
	
	/* Updating stat data fields */
	size = obj40_get_size(&reg->obj);

	/*
	  Updating size if new file offset is further than size. This means,
	  that file realy got some data additionaly, not only got rewtittem
	  something.
	*/
	if (reg->offset > size) {
		if ((res = obj40_set_size(&reg->obj, reg->offset)))
			return res;
	}
	
	item = &reg->obj.statdata.item;
	
	if ((res = obj40_read_unix(item, &unix_hint)))
		return res;
	
	atime = time(NULL);
	
	unix_hint.atime = atime;
	unix_hint.mtime = atime;

	if (reg->offset > unix_hint.bytes)
		unix_hint.bytes = reg->offset;

	if ((res = obj40_write_unix(item, &unix_hint)))
		return res;
	
	return written;
}

/* Takes care about holes */
static errno_t reg40_holes(object_entity_t *entity) {
	errno_t res;
	reg40_t *reg;
	uint32_t size;

	reg = (reg40_t *)entity;

	/* Updating stat data place */
	if ((res = obj40_stat(&reg->obj)))
		return res;
	
	size = obj40_get_size(&reg->obj);

	/* Writing holes */
	if (reg->offset > size) {
		void *holes;
		int32_t hole;
		int32_t written;

		hole = reg->offset - size;
		reg->offset -= hole;

		for (written = 0; written < hole; ) {
			uint32_t chunk;

			chunk = hole - written;

			if (chunk > reg40_chunk(reg))
				chunk = reg40_chunk(reg);
			
			if (!(holes = aal_calloc(chunk, 0)))
				return -ENOMEM;

			if (reg40_put(entity, holes, chunk) < 0) {
				aal_free(holes);
				return -EIO;
			}

			aal_free(holes);
			written += chunk;
		}
	}

	return 0;
}

/* Writes "n" bytes from "buff" to passed file. */
static int32_t reg40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	int32_t res;
	
	aal_assert("umka-2280", buff != NULL);
	aal_assert("umka-2281", entity != NULL);

	if ((res = reg40_holes(entity)))
		return res;
	
	if ((res = reg40_put(entity, buff, n)) < 0)
		return res;
	
	return res;
}

static errno_t reg40_truncate(object_entity_t *entity,
			      uint64_t n)
{
	errno_t res;
	reg40_t *reg;
	uint64_t size;
	uint64_t offset;

	reg = (reg40_t *)entity;
	
	/* Updating stat data place */
	if ((res = obj40_stat(&reg->obj)))
		return res;
	
	/* Getting file size */
	size = obj40_get_size(&reg->obj);

	if (n == size)
		return 0;

	/* Saving current file offset */
	offset = reg->offset;
		
	/* Checking if truncate will increase file size */
	if (n > size) {
		reg->offset = n;
		
		if ((res = reg40_holes(entity)))
			goto error_restore_offset;
	} else {
	}

 error_restore_offset:
	reg->offset = offset;
	return res;
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
	blk_t blk;
	errno_t res;
	
	layout_hint_t *hint = (layout_hint_t *)data;
	item_entity_t *item = (item_entity_t *)object;

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
		item_entity_t *item;
		
		if (reg40_next(reg) != PRESENT)
			break;
		
		item = &reg->body.item;
		
		if (item->plugin->o.item_ops->layout) {
			res = plugin_call(item->plugin->o.item_ops, layout,
					  item, callback_item_data, &hint);
			
			if (res != 0)
				return res;
			
		} else {
			if ((res = callback_item_data(item, item->context.blk,
						      1, &hint)))
				return res;
		}
		
		plugin_call(item->plugin->o.item_ops, maxreal_key, item, &key);
		
		reg->offset = plugin_call(key.plugin->o.key_ops,
					  get_offset, &key) + 1;
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
		item_entity_t *item;
		
		if (reg40_next(reg) != PRESENT)
			break;
		
		item = &reg->body.item;
			
		if ((res = func(entity, &reg->body, data)))
			return res;

		plugin_call(item->plugin->o.item_ops, maxreal_key,
			    item, &key);

		reg->offset = plugin_call(key.plugin->o.key_ops,
					  get_offset, &key) + 1;
	}
	
	return 0;
}
#endif

static errno_t reg40_seek(object_entity_t *entity, 
			  uint64_t offset) 
{
	aal_assert("umka-1968", entity != NULL);

	((reg40_t *)entity)->offset = offset;
	return 0;
}

static void reg40_close(object_entity_t *entity) {
	reg40_t *reg = (reg40_t *)entity;
		
	aal_assert("umka-1170", entity != NULL);

	/* Unlocking statdata and body */
	obj40_relock(&reg->obj, &reg->obj.statdata, NULL);

	if (reg->body.node != NULL)
		obj40_relock(&reg->obj, &reg->body, NULL);
	
	aal_free(entity);
}

static uint64_t reg40_offset(object_entity_t *entity) {
	aal_assert("umka-1159", entity != NULL);
	return ((reg40_t *)entity)->offset;
}

static reiser4_object_ops_t reg40_ops = {
#ifndef ENABLE_STAND_ALONE
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
	.lookup	      = NULL,
	.follow       = NULL,
	.readdir      = NULL,
	.telldir      = NULL,
	.seekdir      = NULL,
		
	.open	      = reg40_open,
	.close	      = reg40_close,
	.reset	      = reg40_reset,
	.seek	      = reg40_seek,
	.offset	      = reg40_offset,
	.size         = reg40_size,
	.read	      = reg40_read,
	.check_struct = NULL
};

static reiser4_plugin_t reg40_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = OBJECT_FILE40_ID,
		.group = FILE_OBJECT,
		.type = OBJECT_PLUGIN_TYPE,
#ifndef ENABLE_STAND_ALONE
		.label = "reg40",
		.desc = "Regular file for reiser4, ver. " VERSION
#endif
	},
	.o = {
		.object_ops = &reg40_ops
	}
};

static reiser4_plugin_t *reg40_start(reiser4_core_t *c) {
	core = c;
	return &reg40_plugin;
}

plugin_register(reg40, reg40_start, NULL);
