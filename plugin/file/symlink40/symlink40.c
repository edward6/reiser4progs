/*
  symlink40.c -- reiser4 symlink file plugin.

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

#include "symlink40.h"

extern reiser4_plugin_t symlink40_plugin;

static reiser4_core_t *core = NULL;

/* Reads @n bytes to passed buffer @buff */
static int32_t symlink40_read(object_entity_t *entity, 
			      void *buff, uint32_t n)
{
	item_entity_t *item;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;

	symlink40_t *symlink = (symlink40_t *)entity;

	aal_assert("umka-1570", entity != NULL, return 0);
	aal_assert("umka-1571", buff != NULL, return 0);

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	hint.hint = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = buff;

	item = &symlink->file.statdata.entity;
	
	if (plugin_call(return -1, item->plugin->item_ops, open, item, &hint))
		return -1;

	return aal_strlen(buff);
}

static object_entity_t *symlink40_open(const void *tree, 
				       reiser4_place_t *place) 
{
	reiser4_key_t *pkey;
	symlink40_t *symlink;

	aal_assert("umka-1163", tree != NULL, return NULL);
	aal_assert("umka-1164", place != NULL, return NULL);
    
	if (!(symlink = aal_calloc(sizeof(*symlink), 0)))
		return NULL;

	pkey = &place->entity.key;
		
	if (file40_init(&symlink->file, pkey, &symlink40_plugin, tree, core))
		goto error_free_symlink;
	
	aal_memcpy(&symlink->file.statdata, place, sizeof(*place));
	symlink->file.core->tree_ops.lock(tree, &symlink->file.statdata);
    
	return (object_entity_t *)symlink;

 error_free_symlink:
	aal_free(symlink);
	return NULL;
}

#ifndef ENABLE_COMPACT

static object_entity_t *symlink40_create(const void *tree, 
					 reiser4_file_hint_t *hint) 
{
	int lookup;
	roid_t objectid;
	roid_t locality;
	roid_t parent_locality;

	symlink40_t *symlink;
	reiser4_place_t place;
	reiser4_plugin_t *stat_plugin;
    
	reiser4_statdata_hint_t stat;
	reiser4_item_hint_t stat_hint;
    
	reiser4_sdext_lw_hint_t lw_ext;
	reiser4_sdext_unix_hint_t unix_ext;
    
	reiser4_level_t stop = {LEAF_LEVEL, LEAF_LEVEL};
	
	aal_assert("umka-1741", tree != NULL, return NULL);
	aal_assert("umka-1740", hint != NULL, return NULL);

	if (!(symlink = aal_calloc(sizeof(*symlink), 0)))
		return NULL;
    
	if (file40_init(&symlink->file, &hint->object, &symlink40_plugin, tree, core))
		goto error_free_symlink;
	
	locality = file40_locality(&symlink->file);
	objectid = file40_objectid(&symlink->file);

	parent_locality = plugin_call(return NULL, hint->object.plugin->key_ops, 
				      get_locality, hint->parent.body);
    
	if (!(stat_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    hint->statdata)))
	{
		aal_exception_error("Can't find stat data item plugin by "
				    "its id 0x%x.", hint->statdata);
		goto error_free_symlink;
	}
    
	/* Initializing the stat data hint */
	aal_memset(&stat_hint, 0, sizeof(stat_hint));
	stat_hint.plugin = stat_plugin;
    
	stat_hint.key.plugin = hint->object.plugin;
	
	plugin_call(goto error_free_symlink, hint->object.plugin->key_ops, assign, 
		    stat_hint.key.body, hint->object.body);
    
	/* Initializing stat data item hint. */
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID |
		1 << SDEXT_SYMLINK_ID;
    
	lw_ext.mode = S_IFLNK | 0755;
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
	stat.ext[SDEXT_SYMLINK_ID] = hint->body.symlink;

	stat_hint.hint = &stat;

	if ((lookup = core->tree_ops.lookup(tree, &hint->object, &stop, &place)) == FAILED)
		goto error_free_symlink;

	if (lookup == PRESENT) {
		aal_exception_error("Stat data key of file 0x%llx already exists in "
				    "the tree.", objectid);
		goto error_free_symlink;
	}
	
	/* Calling balancing code in order to insert statdata item into the tree */
	if (core->tree_ops.insert(tree, &place, &stat_hint)) {
		aal_exception_error("Can't insert stat data item of file 0x%llx into "
				    "the tree.", objectid);
		goto error_free_symlink;
	}
    
	/* Grabbing the stat data item */
	if (file40_realize(&symlink->file)) {
		aal_exception_error("Can't grab stat data of file 0x%llx.", 
				    file40_objectid(&symlink->file));
		goto error_free_symlink;
	}
    
	return (object_entity_t *)symlink;

 error_free_symlink:
	aal_free(symlink);
	return NULL;
}

/* Writes "n" bytes from "buff" to passed file. */
static int32_t symlink40_write(object_entity_t *entity, 
			   void *buff, uint32_t n) 
{
	/* Sorry, not implemented yet! */
	return 0;
}

static errno_t symlink40_metadata(object_entity_t *entity,
			      metadata_func_t metadata_func,
			      void *data)
{
	symlink40_t *symlink;

	aal_assert("umka-1718", entity != NULL, return -1);
	aal_assert("umka-1719", metadata_func != NULL, return -1);

	symlink = (symlink40_t *)entity;
	return metadata_func(entity, &symlink->file.statdata, data);
}

static errno_t symlink40_layout(object_entity_t *entity,
				action_func_t action_func,
				void *data)
{
	blk_t blk;
	symlink40_t *symlink;

	aal_assert("umka-1720", entity != NULL, return -1);
	aal_assert("umka-1721", action_func != NULL, return -1);

	symlink = (symlink40_t *)entity;
	blk = symlink->file.statdata.entity.con.blk;
		
	return action_func(entity, blk, data);
}

#endif

static void symlink40_close(object_entity_t *entity) {
	symlink40_t *symlink = (symlink40_t *)entity;
		
	aal_assert("umka-1170", entity != NULL, return);

	/* Unlocking statdata and body */
	file40_unlock(&symlink->file, &symlink->file.statdata);
	
	aal_free(entity);
}

/* Detecting the object plugin by extentions or mode */
static int symlink40_confirm(reiser4_place_t *place) {
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
				    symlink40_plugin.h.label);
		return 0;
	}
    
	return S_ISLNK(mode);
}

static reiser4_plugin_t symlink40_plugin = {
	.file_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = FILE_SYMLINK40_ID,
			.group = SYMLINK_FILE,
			.type = FILE_PLUGIN_TYPE,
			.label = "symlink40",
			.desc = "Symlink for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_COMPACT
		.create	    = symlink40_create,
		.write	    = symlink40_write,
		.layout     = symlink40_layout,
		.metadata   = symlink40_metadata,
#else
		.create	    = NULL,
		.write	    = NULL,
		.layout     = NULL,
		.metadata   = NULL,
#endif
		.truncate   = NULL,
		.valid	    = NULL,
		.lookup	    = NULL,
		.reset	    = NULL,
		.offset	    = NULL,
		.seek	    = NULL,
		
		.open	    = symlink40_open,
		.confirm    = symlink40_confirm,
		.close	    = symlink40_close,
		.read	    = symlink40_read
	}
};

static reiser4_plugin_t *symlink40_start(reiser4_core_t *c) {
	core = c;
	return &symlink40_plugin;
}

plugin_register(symlink40_start, NULL);
