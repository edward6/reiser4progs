/*
  reg40.c -- reiser4 default regular file plugin.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/stat.h>

#ifndef ENABLE_COMPACT
#  include <time.h>
#  include <unistd.h>
#endif

#include "reg40.h"

extern reiser4_plugin_t reg40_plugin;

static reiser4_core_t *core = NULL;

static errno_t reg40_reset(object_entity_t *entity) {
	uint64_t size;
	reiser4_key_t key;
	reg40_t *reg = (reg40_t *)entity;
	reiser4_level_t level = {LEAF_LEVEL, TWIG_LEVEL};
    
	aal_assert("umka-1161", reg != NULL, return -1);
    
	if (file40_get_size(&reg->file.statdata, &size))
		return -1;

	if (size == 0)
		return 0;
	
	key.plugin = reg->file.key.plugin;
	plugin_call(return -1, key.plugin->key_ops, build_generic, key.body,
		    KEY_FILEBODY_TYPE, file40_locality(&reg->file),
		    file40_objectid(&reg->file), 0);
    
	file40_unlock(&reg->file, &reg->body);
	
	if (core->tree_ops.lookup(reg->file.tree, &key, &level, &reg->body) != 1)
		reg->body.node = NULL;
	
	/*
	  Locking node the current body lies in, due to prevent the throwing
	  it out of tree cache.
	*/
	file40_lock(&reg->file, &reg->body);

	reg->offset = 0;
	reg->local = 0;

	return 0;
}

static int reg40_next(object_entity_t *entity) {
	int res;
	reiser4_key_t key;
	reg40_t *reg = (reg40_t *)entity;
	reiser4_level_t level = {LEAF_LEVEL, TWIG_LEVEL};
	
	key.plugin = reg->file.key.plugin;
	plugin_call(return -1, key.plugin->key_ops, build_generic, 
		    key.body, KEY_FILEBODY_TYPE, file40_locality(&reg->file), 
		    file40_objectid(&reg->file), reg->offset);

        /* Unlocking the old body */
	file40_unlock(&reg->file, &reg->body);

	/* Getting the next body item from the tree */
	res = core->tree_ops.lookup(reg->file.tree, &key, &level, &reg->body);

	/* Locking new body or old one if lookup failed */
	file40_lock(&reg->file, &reg->body);
	
	return res;
}

/* Reads @n bytes to passed buffer @buff */
static int32_t reg40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	uint64_t size;
	uint32_t read;
	reg40_t *reg = (reg40_t *)entity;

	aal_assert("umka-1182", entity != NULL, return 0);
	aal_assert("umka-1183", buff != NULL, return 0);

	/* There is no any data in file */
	if (!reg->body.node)
		return 0;
    
	if (file40_get_size(&reg->file.statdata, &size))
		return -1;

	if (size == 0)
		return 0;
	
	for (read = 0; read < n; ) {
		item_entity_t *item = &reg->body.entity;

		if (item->plugin->h.sign.group == TAIL_ITEM) {
			uint32_t chunk;
			
			/* Check if we need next item */
			if (reg->local >= item->len) {
				if (reg->offset >= size || reg40_next(entity) != 1)
					break;
			}
		
			/* Calculating the chunk of data to be read. If it is
			 * zero, we go away. Else fetching of data from the item
			 * will be performed */
			chunk = (item->len - reg->local) > n - read ?
				n - read : (item->len - reg->local);
			
			if (!chunk) break;
	
			plugin_call(return -1, item->plugin->item_ops, fetch,
				    item, reg->local, buff + read, chunk);
			
			read += chunk;
			reg->offset += chunk;
			reg->local += chunk;
		} else {
			uint32_t count;
			uint32_t blocksize;
			
			aal_block_t *block;
			aal_device_t *device;
			
			reiser4_pos_t *pos = &reg->body.pos;

			if (pos->unit == ~0ul)
				pos->unit = 0;

			count = plugin_call(return -1, item->plugin->item_ops,
					    count, item);

			if (pos->unit >= count) {
				if (reg->offset >= size || reg40_next(entity) != 1)
					break;
			}
			
			device = item->con.device;
			blocksize = aal_device_get_bs(device);
			
			for (; pos->unit < count && read < n; ) {
				uint64_t blk;
				reiser4_ptr_hint_t ptr;
				
				if (plugin_call(return -1, item->plugin->item_ops,
						fetch, item, pos->unit, &ptr, 1))
					return -1;

				blk = ptr.ptr + (reg->local / blocksize);
				
				for (; blk < ptr.ptr + ptr.width && read < n; ) {
					uint32_t chunk, offset;

					if (!(block = aal_block_open(device, blk))) {
						aal_exception_error("Can't read block %llu. %s.",
								    blk, device->error);
					}

					offset = (reg->local % blocksize);
					chunk = blocksize - offset;
					chunk = (chunk <= n - read) ? chunk : n - read;
					
					aal_memcpy(buff + read, block->data + offset, chunk);
					aal_block_close(block);
					
					read += chunk;
					reg->local += chunk;
					reg->offset += chunk;

					if ((offset + chunk) % blocksize == 0)
						blk++;
				}

				if (blk >= ptr.ptr + ptr.width) {
					pos->unit++;
					reg->local = 0;
				}
			}
		}
	}

	return read;
}

static object_entity_t *reg40_open(const void *tree, 
				   reiser4_key_t *object) 
{
	reg40_t *reg;

	aal_assert("umka-1163", tree != NULL, return NULL);
	aal_assert("umka-1164", object != NULL, return NULL);
	aal_assert("umka-1165", object->plugin != NULL, return NULL);
    
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;

	if (file40_init(&reg->file, object, &reg40_plugin, tree, core))
		goto error_free_reg;
	
	if (file40_realize(&reg->file)) {
		aal_exception_error("Can't grab stat data of the file "
				    "0x%llx.", file40_objectid(&reg->file));
		goto error_free_reg;
	}
    
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

static object_entity_t *reg40_create(const void *tree, 
				     reiser4_key_t *parent,
				     reiser4_key_t *object, 
				     reiser4_file_hint_t *hint) 
{
	reg40_t *reg;
	reiser4_plugin_t *stat_plugin;
    
	reiser4_item_hint_t stat_hint;
	reiser4_statdata_hint_t stat;
    
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;
    
	roid_t objectid;
	roid_t locality;
	roid_t parent_locality;

	aal_assert("umka-1166", parent != NULL, return NULL);
	aal_assert("umka-1167", object != NULL, return NULL);
	aal_assert("umka-1168", object->plugin != NULL, return NULL);
	aal_assert("umka-1169", tree != NULL, return NULL);

	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;
    
	if (file40_init(&reg->file, object, &reg40_plugin, tree, core))
		goto error_free_reg;
	
	locality = file40_locality(&reg->file);
    	objectid = file40_objectid(&reg->file);

	parent_locality = plugin_call(return NULL, object->plugin->key_ops, 
				      get_locality, parent->body);
    
	if (!(stat_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    hint->statdata_pid)))
	{
		aal_exception_error("Can't find stat data item plugin by "
				    "its id 0x%x.", hint->statdata_pid);
		goto error_free_reg;
	}
    
	/* Initializing the stat data hint */
	aal_memset(&stat_hint, 0, sizeof(stat_hint));
	stat_hint.plugin = stat_plugin;
    
	stat_hint.key.plugin = object->plugin;
	plugin_call(goto error_free_reg, object->plugin->key_ops, assign, 
		    stat_hint.key.body, object->body);
    
	/* Initializing stat data item hint. */
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID;
    
	lw_ext.mode = S_IFREG | 0755;
	lw_ext.nlink = 2;

	/* This should be modifyed by write */
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
    
	/* Calling balancing code in order to insert statdata item into the tree */
	if (core->tree_ops.insert(tree, &stat_hint, LEAF_LEVEL, NULL)) {
		aal_exception_error("Can't insert stat data item of object 0x%llx into "
				    "the tree.", objectid);
		goto error_free_reg;
	}
    
	/* Grabbing the stat data item */
	if (file40_realize(&reg->file)) {
		aal_exception_error("Can't grab stat data of file 0x%llx.", 
				    file40_objectid(&reg->file));
		goto error_free_reg;
	}

	reg->offset = 0;
	reg->local = 0;
    
	return (object_entity_t *)reg;

 error_free_reg:
	aal_free(reg);
	return NULL;
}

static errno_t reg40_truncate(object_entity_t *entity, uint64_t n) {
	/* Sorry, not implemented yet! */
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

static errno_t reg40_layout(object_entity_t *entity, file_action_func_t func,
			    void *data)
{
	errno_t res;
	uint64_t size;
	reg40_t *reg = (reg40_t *)entity;
	
	aal_assert("umka-1471", entity != NULL, return -1);
	aal_assert("umka-1472", func != NULL, return -1);

	if (!reg->body.node)
		return 0;

	if (file40_get_size(&reg->file.statdata, &size))
		return -1;
		
	while (1) {
		if (reg->body.entity.plugin->h.sign.group == TAIL_ITEM) {
			blk_t blk = reg->body.entity.con.blk;

			if ((res = func(entity, blk, data)))
				return res;

			reg->offset += reg->body.entity.len;
		} else {
			uint32_t count;
			uint32_t blocksize;
			reiser4_ptr_hint_t ptr;
			reiser4_pos_t pos = reg->body.pos;

			count = plugin_call(return -1, reg->body.entity.plugin->item_ops,
					    count, &reg->body.entity);

			blocksize = reg->body.entity.con.device->blocksize;

			if (pos.unit == ~0ul)
				pos.unit = 0;
			
			for (; pos.unit < count; pos.unit++) {
				uint64_t blk;
				
				if (plugin_call(return -1, reg->body.entity.plugin->item_ops,
						fetch, &reg->body.entity, pos.unit, &ptr, 1))
					return -1;

				for (blk = ptr.ptr; blk < ptr.ptr + ptr.width; blk++) {
					if ((res = func(entity, blk, data)))
						return res;
				}
			
				reg->offset += ptr.width * blocksize;
			}
		}
		
		if (reg->offset >= size || reg40_next(entity) != 1)
			break;
			
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

/* Detecting the object plugin by extentions or mode */
static int reg40_confirm(reiser4_place_t *place) {
	uint16_t mode;
    
	aal_assert("umka-1292", place != NULL, return 0);

	/* 
	   FIXME-UMKA: Here we should inspect all extentions and try to find out
	   if non-standard file plugin is in use.
	*/

	/* 
	   Guessing plugin type and plugin id by mode field from the stat data
	   item. Here we return default plugins for every file type.
	*/
	if (file40_get_mode(place, &mode)) {
		aal_exception_error("Can't get mode from stat data while probing %s.",
				    reg40_plugin.h.label);
		return 0;
	}
    
	return S_ISREG(mode);
}

static errno_t reg40_seek(object_entity_t *entity, 
			  uint64_t offset) 
{
	return -1;
}

static reiser4_plugin_t reg40_plugin = {
	.file_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = FILE_REGULAR40_ID,
				.group = REGULAR_FILE,
				.type = FILE_PLUGIN_TYPE
			},
			.label = "reg40",
			.desc = "Regular file for reiserfs 4.0, ver. " VERSION,
		},
#ifndef ENABLE_COMPACT
		.create	    = reg40_create,
		.write	    = reg40_write,
		.truncate   = reg40_truncate,
		.layout     = reg40_layout,
#else
		.create	    = NULL,
		.write	    = NULL,
		.truncate   = NULL,
		.layout     = NULL,
#endif
		.valid	    = NULL,
		.lookup	    = NULL,
		
		.open	    = reg40_open,
		.confirm    = reg40_confirm,
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
