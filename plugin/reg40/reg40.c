/*
  reg40.c -- reiser4 default regular file plugin.
  Copyright (C) 1996-2002 Hans Reiser.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_COMPACT
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <unistd.h>
#  include <time.h>
#endif

#include "reg40.h"

extern reiser4_plugin_t reg40_plugin;

static reiser4_core_t *core = NULL;

static roid_t reg40_objectid(reg40_t *reg) {
    aal_assert("umka-839", reg != NULL, return 0);
    
    return plugin_call(return 0, reg->key.plugin->key_ops, 
					   get_objectid, reg->key.body);
}

static roid_t reg40_locality(reg40_t *reg) {
    aal_assert("umka-839", reg != NULL, return 0);
    
    return plugin_call(return 0, reg->key.plugin->key_ops, 
					   get_locality, reg->key.body);
}

/* Gets size field from the stat data */
static errno_t reg40_get_size(reiser4_item_t *item, uint64_t *size) {
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	if (plugin_call(return -1, item->plugin->item_ops,
					open, item, &hint))
		return -1;

	*size = lw_hint.size;
	return 0;
}

/* Gets mode field from the stat data */
static errno_t reg40_get_mode(reiser4_item_t *item, uint16_t *mode) {
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	if (plugin_call(return -1, item->plugin->item_ops,
					open, item, &hint))
		return -1;

	*mode = lw_hint.mode;
	return 0;
}

#ifndef ENABLE_COMPACT

/* Updates size field in the stat data */
static errno_t reg40_set_size(reiser4_item_t *item, uint64_t size) {
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;

	if (plugin_call(return -1, item->plugin->item_ops,
					open, item, &hint))
		return -1;

	lw_hint.size = size;

	return plugin_call(return -1, item->plugin->item_ops,
					   init, item, &hint);
}

#endif

static errno_t reg40_reset(reiser4_entity_t *entity) {
    reiser4_key_t key;
    reg40_t *reg = (reg40_t *)entity;
    
    aal_assert("umka-1161", reg != NULL, return -1);
    
    /* There si no any data in file */
    if (!reg->body.plugin)
		return 0;
    
    key.plugin = reg->key.plugin;
    plugin_call(return -1, key.plugin->key_ops, build_generic, key.body, 
				KEY_FILEBODY_TYPE, reg40_locality(reg), reg40_objectid(reg), 0);
    
    if (core->tree_ops.lookup(reg->tree, &key, LEAF_LEVEL, &reg->place) != 1) {
		aal_exception_error("Can't find the body of file 0x%llx.", 
							reg40_objectid(reg));
		return -1;
    }

    if (core->item_ops.open(&reg->body, reg->place.entity, &reg->place.pos)) {
		aal_exception_error("Can't open the body of file 0x%llx.",
							reg40_objectid(reg));
		return -1;
    }
    
    reg->offset = 0;
    reg->place.pos.unit = 0;

    return 0;
}

/* This function grabs the stat data of the reg file */
static errno_t reg40_realize(reg40_t *reg) {
    rpid_t pid;
    
    aal_assert("umka-1162", reg != NULL, return -1);	

    plugin_call(return -1, reg->key.plugin->key_ops, build_generic, 
				reg->key.body, KEY_STATDATA_TYPE, reg40_locality(reg), 
				reg40_objectid(reg), 0);
    
    /* Positioning to the file stat data */
    if (core->tree_ops.lookup(reg->tree, &reg->key, LEAF_LEVEL, 
							  &reg->place) != 1) 
		{
			aal_exception_error("Can't find stat data of the file 0x%llx.", 
								reg40_objectid(reg));
			return -1;
		}
    
    if (core->item_ops.open(&reg->statdata, reg->place.entity, &reg->place.pos)) {
		aal_exception_error("Can't open the stadata of file 0x%llx.",
							reg40_objectid(reg));
		return -1;
    }
    
    return 0;
}

static errno_t reg40_next(reiser4_entity_t *entity) {
    roid_t curr_objectid;
    roid_t next_objectid;

    reiser4_key_t next_key;
    reiser4_item_t next_item;
    
    reg40_t *reg = (reg40_t *)entity;
    reiser4_place_t *place = &reg->place;
    reiser4_place_t save_place = reg->place;

    /* Getting the right neighbour */
    if (core->tree_ops.right(reg->tree, place))
        goto error_set_context;
    
    /* Opening the next item */
    if (core->item_ops.open(&next_item, place->entity, &place->pos))
		goto error_set_context;
    
    if ((reg->body.plugin->h.group != TAIL_ITEM &&
		 reg->body.plugin->h.group != EXTENT_ITEM))
		{
			/* Next item is nor tail neither extent */
			goto error_set_context;
		}
    
    /* Getting key of the first item in the right neightbour */
    if (core->item_ops.key(&next_item, &next_key)) {
        aal_exception_error("Can't get next item key by coord.");
        goto error_set_context;
    }
    
    /* 
	   Getting objectid of both keys in order to determine are items
	   mergeable.
    */
    curr_objectid = plugin_call(goto error_set_context, 
								reg->key.plugin->key_ops, get_objectid, reg->key.body);
	
    next_objectid = plugin_call(goto error_set_context, 
								reg->key.plugin->key_ops, get_objectid, next_key.body);
	
    /* Determining are items mergeable */
    if (curr_objectid == next_objectid) {
		reg->body = next_item;
		return 0;
    }

  error_set_context:
    *place = save_place;
    return -1;
}

/* Reads n entries to passed buffer buff */
static int32_t reg40_read(reiser4_entity_t *entity, 
						  void *buff, uint32_t n)
{
    uint32_t read;
    reg40_t *reg = (reg40_t *)entity;

    aal_assert("umka-1182", entity != NULL, return 0);
    aal_assert("umka-1183", buff != NULL, return 0);

    /* There si no any data in file */
    if (!reg->body.plugin)
		return 0;
    
    for (read = 0; read < n; ) {
		char *body;
		uint32_t chunk, len;
	
		if (!(len = core->item_ops.len(&reg->body)))
			break;
	
		if (reg->place.pos.unit >= len) {
	    
			/* Getting the next file body item */
			if (reg40_next(entity))
				break;
		}
	
		chunk = (len - reg->place.pos.unit) > n - read ?
			n - read : (len - reg->place.pos.unit);

		if (!chunk) break;
	
		if (reg->body.plugin->h.group == TAIL_ITEM) {
			if (!(body = core->item_ops.body(&reg->body)))
				break;
		
			/* Getting the data from the tail item */
			aal_memcpy(buff + read, body + reg->place.pos.unit, chunk);
		} else {
	    
			/* Getting the data from the extent item */
			aal_exception_error("Sorry, extents are not supported yet!");
			break;
		}
	
		reg->place.pos.unit += chunk;
		read += chunk;
    }
    
    return read;
}

static reiser4_entity_t *reg40_open(const void *tree, 
									reiser4_key_t *object) 
{
    reg40_t *reg;

    aal_assert("umka-1163", tree != NULL, return NULL);
    aal_assert("umka-1164", object != NULL, return NULL);
    aal_assert("umka-1165", object->plugin != NULL, return NULL);
    
    if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;
    
    reg->tree = tree;
    reg->plugin = &reg40_plugin;
    
    reg->key.plugin = object->plugin;
    plugin_call(goto error_free_reg, object->plugin->key_ops, assign, 
				reg->key.body, object->body);
    
    /* Grabbing the stat data item */
    if (reg40_realize(reg)) {
		aal_exception_error("Can't grab stat data of the file "
							"with oid 0x%llx.", reg40_objectid(reg));
		goto error_free_reg;
    }
    
    if (reg40_reset((reiser4_entity_t *)reg)) {
		aal_exception_error("Can't reset file 0x%llx.", 
							reg40_objectid(reg));
		goto error_free_reg;
    }
    
    return (reiser4_entity_t *)reg;

  error_free_reg:
    aal_free(reg);
    return NULL;
}

#ifndef ENABLE_COMPACT

static reiser4_entity_t *reg40_create(const void *tree, 
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
    
    reg->tree = tree;
    reg->plugin = &reg40_plugin;
    
    reg->key.plugin = object->plugin;
    plugin_call(goto error_free_reg, object->plugin->key_ops,
				assign, reg->key.body, object->body);
    
    locality = plugin_call(return NULL, object->plugin->key_ops, 
						   get_objectid, parent->body);
    
    objectid = plugin_call(return NULL, object->plugin->key_ops, 
						   get_objectid, object->body);
    
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
							"the thee.", objectid);
		goto error_free_reg;
    }
    
    /* Grabbing the stat data item */
    if (reg40_realize(reg)) {
		aal_exception_error("Can't grab stat data of file 0x%llx.", 
							reg40_objectid(reg));
		goto error_free_reg;
    }

    reg->offset = 0;
    reg->place.pos.unit = 0;
    
    return (reiser4_entity_t *)reg;

  error_free_reg:
    aal_free(reg);
    return NULL;
}

static errno_t reg40_truncate(reiser4_entity_t *entity, 
							  uint64_t n) 
{
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

/* Writes "n" bytes from "buff" to passed file */
static int32_t reg40_write(reiser4_entity_t *entity, 
						   void *buff, uint32_t n) 
{
    int is_extent;
    int64_t size;
    
    reiser4_item_hint_t hint;
    uint32_t wrote, maxspace;
	uint32_t overwrote;
    
    reiser4_place_t place;
    reiser4_plugin_t *plugin;
    reg40_t *reg = (reg40_t *)entity;

    aal_assert("umka-1184", entity != NULL, return -1);
    aal_assert("umka-1185", buff != NULL, return -1);

    if (reg40_get_size(&reg->statdata, &size))
		return -1;
	
    overwrote = size - reg->offset;
    plugin = reg40_policy(reg, n);
    
    is_extent = (plugin->h.group == EXTENT_ITEM);
    
    maxspace = is_extent ? core->tree_ops.blockspace(reg->tree) : 
		core->tree_ops.nodespace(reg->tree);

    /* 
	   FIXME-UMKA: Here also should be tail conversion code in the future. It 
	   will find the last tail if exists and convert it to the extent.
    */
    for (wrote = 0; wrote < n; ) {
		uint8_t level;
		uint32_t chunk;

		hint.plugin = reg40_policy(reg, n - wrote);
		chunk = n - wrote > maxspace ? maxspace : n - wrote;

		hint.len = chunk;
		hint.data = buff + wrote;

		hint.key.plugin = reg->key.plugin;
		plugin_call(break, hint.key.plugin->key_ops, build_generic, 
					hint.key.body, KEY_FILEBODY_TYPE, reg40_locality(reg), 
					reg40_objectid(reg), reg->offset);
    
		/* Inserting the entry to the tree */
		level = LEAF_LEVEL + (hint.plugin->h.group == EXTENT_ITEM);
	
		if (core->tree_ops.insert(reg->tree, &hint, level, &place)) {
			aal_exception_error("Can't insert body item to the tree. "
								"Wrote %u bytes.", wrote);
			return 0;
		}
    
		wrote += chunk;
		reg->offset += chunk;
    }
    
    if (core->item_ops.open(&reg->body, place.entity, &place.pos))
		return 0;
    
    /* Updating stat data item, because it may be moved durring balancing */
    if (reg40_realize(reg))
		return 0;
    
	/* Updating stat data size field */
	reg40_set_size(&reg->statdata, size + (wrote - overwrote));
	    
    return wrote;
}

#endif

static void reg40_close(reiser4_entity_t *entity) {
    aal_assert("umka-1170", entity != NULL, return);
    aal_free(entity);
}

static uint64_t reg40_offset(reiser4_entity_t *entity) {
    aal_assert("umka-1159", entity != NULL, return 0);
    return ((reg40_t *)entity)->offset;
}

/* Detecting the object plugin by extentions or mode */
static int reg40_confirm(reiser4_item_t *item) {
    uint16_t mode;
    
    aal_assert("umka-1292", item != NULL, return 0);

    /* 
	   FIXME-UMKA: Here we should inspect all extentions and try to find out
	   if non-standard file plugin is in use.
    */

    /* 
	   Guessing plugin type and plugin id by mode field from the stat data 
	   item. Here we return default plugins for every file type.
    */
    if (reg40_get_mode(item, &mode)) {
		aal_exception_error("Can't get mode from stat data while probing %s.",
							reg40_plugin.h.label);
		return 0;
	}
    
    return S_ISREG(mode);
}

static errno_t reg40_seek(reiser4_entity_t *entity, 
						  uint64_t offset) 
{
    reg40_t *reg = (reg40_t *)entity;
    aal_assert("umka-1171", entity != NULL, return 0);

    /* FIXME-UMKA: Not implemented yet! */
    return -1;
}

static reiser4_plugin_t reg40_plugin = {
    .file_ops = {
		.h = {
			.handle = NULL,
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
#else
        .create	    = NULL,
        .write	    = NULL,
        .truncate   = NULL,
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

plugin_register(reg40_start);
