/*
  sym40.c -- reiser4 symlink file plugin.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef ENABLE_SYMLINKS_SUPPORT

#ifndef ENABLE_STAND_ALONE
#  include <time.h>
#  include <unistd.h>
#endif

#include "sym40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t sym40_plugin;

/* Opens symlink and returns initialized instance to the caller */
static object_entity_t *sym40_open(void *tree, place_t *place) {
	sym40_t *sym;

	aal_assert("umka-1163", tree != NULL);
	aal_assert("umka-1164", place != NULL);
    
	if (obj40_pid(&place->item) != sym40_plugin.h.id)
		return NULL;
	
	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return NULL;

	/* Initalizing file handle */
	obj40_init(&sym->obj, &sym40_plugin,
		   &place->item.key, core, tree);

	/* Initialziing statdata place */
	aal_memcpy(&sym->obj.statdata, place,
		   sizeof(*place));
	
	obj40_lock(&sym->obj, &sym->obj.statdata);
	
	return (object_entity_t *)sym;
}

#ifndef ENABLE_STAND_ALONE
/* Reads @n bytes to passed buffer @buff */
static int32_t sym40_read(object_entity_t *entity, 
			  void *buff, uint32_t n)
{
	create_hint_t hint;
	item_entity_t *item;
	statdata_hint_t stat;

	sym40_t *sym = (sym40_t *)entity;

	aal_assert("umka-1570", entity != NULL);
	aal_assert("umka-1571", buff != NULL);

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));

	hint.type_specific = &stat;
	stat.ext[SDEXT_SYMLINK_ID] = buff;

	item = &sym->obj.statdata.item;

	if (!item->plugin->o.item_ops->read)
		return -EINVAL;

	if (plugin_call(item->plugin->o.item_ops,
			read, item, &hint, 0, 1) != 1)
	{
		return -EINVAL;
	}

	return aal_strlen(buff);
}

/* Creates symlink and returns initialized instance to the caller */
static object_entity_t *sym40_create(void *tree, object_entity_t *parent,
				     object_hint_t *hint, place_t *place)
{
	sym40_t *sym;
    
	statdata_hint_t stat;
	create_hint_t stat_hint;
    
	sdext_lw_hint_t lw_ext;
	sdext_unix_hint_t unix_ext;
	
	reiser4_plugin_t *stat_plugin;
	
	aal_assert("umka-1741", tree != NULL);
	aal_assert("umka-1740", hint != NULL);

	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return NULL;

	/* Inizializes file handle */
	obj40_init(&sym->obj, &sym40_plugin, &hint->object, core, tree);
	
	/* Getting statdata plugin */
	if (!(stat_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
						    hint->statdata)))
	{
		aal_exception_error("Can't find stat data item plugin by "
				    "its id 0x%x.", hint->statdata);
		goto error_free_sym;
	}
    
	/* Initializing the stat data hint */
	aal_memset(&stat_hint, 0, sizeof(stat_hint));

	stat_hint.plugin = stat_plugin;
	stat_hint.flags = HF_FORMATD;
	stat_hint.key.plugin = hint->object.plugin;
	
	plugin_call(hint->object.plugin->o.key_ops, assign,
		    &stat_hint.key, &hint->object);
    
	/*
	  Initializing stat data item hint. Here we set up the extentions mask
	  to unix extention, light weight and symlink ones.
	*/
	stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID |
		1 << SDEXT_SYMLINK_ID;

	/* Lightweigh extention hint setup */
	lw_ext.mode = S_IFLNK | 0755;
	lw_ext.nlink = 1;
	lw_ext.size = 0;

	/* Unix extention hint setup */
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
	stat.ext[SDEXT_SYMLINK_ID] = hint->body.sym;

	stat_hint.type_specific = &stat;

	/* Inserting stat data into the tree */
	if (obj40_insert(&sym->obj, &stat_hint,
			 LEAF_LEVEL, &sym->obj.statdata))
	{
		goto error_free_sym;
	}

	/* Saving statdata place and locking the node it lies in */
	aal_memcpy(place, &sym->obj.statdata, sizeof(*place));

	obj40_lock(&sym->obj, &sym->obj.statdata);
		
	if (parent) {
		plugin_call(parent->plugin->o.object_ops, link,
			    parent);
	}
	
	return (object_entity_t *)sym;

 error_free_sym:
	aal_free(sym);
	return NULL;
}

static errno_t sym40_link(object_entity_t *entity) {
	aal_assert("umka-1915", entity != NULL);
	return obj40_link(&((sym40_t *)entity)->obj, 1);
}

static errno_t sym40_unlink(object_entity_t *entity) {
	errno_t res;
	sym40_t *sym;
	
	aal_assert("umka-1914", entity != NULL);

	sym = (sym40_t *)entity;
	
	if ((res = obj40_stat(&sym->obj)))
		return res;

	if ((res = obj40_link(&sym->obj, -1)))
		return res;

	if (obj40_get_nlink(&sym->obj) > 0)
		return 0;
	
	/* Removing file when nlink became zero */
	return obj40_remove(&sym->obj, STAT_KEY(&sym->obj), 1);
}

/* Calls function @func for each symlink item (statdata only) */
static errno_t sym40_metadata(object_entity_t *entity,
			      place_func_t func,
			      void *data)
{
	sym40_t *sym;

	aal_assert("umka-1719", func != NULL);
	aal_assert("umka-1718", entity != NULL);

	sym = (sym40_t *)entity;
	return func(entity, &sym->obj.statdata, data);
}

/* Calls function @func for each block symlink items lie in */
static errno_t sym40_layout(object_entity_t *entity,
			    block_func_t block_func,
			    void *data)
{
	blk_t blk;
	sym40_t *sym;

	aal_assert("umka-1721", block_func != NULL);
	aal_assert("umka-1720", entity != NULL);

	sym = (sym40_t *)entity;
	blk = sym->obj.statdata.item.context.blk;
		
	return block_func(entity, blk, data);
}
#endif

/*
  This function reads symlink and parses it by means of using aux_parse_path
  with applying corresponding callback fucntions for searching stat data and
  searchig entry. It returns stat data key of the object symlink points to.
*/
static errno_t sym40_follow(object_entity_t *entity,
			    key_entity_t *from,
			    key_entity_t *key)
{
	errno_t res;
	sym40_t *sym;
	char path[1024];
	
	aal_assert("umka-1775", key != NULL);
	aal_assert("umka-1774", entity != NULL);
	aal_assert("umka-2245", current != NULL);

	sym = (sym40_t *)entity;
	
	if ((res = obj40_get_sym(&sym->obj, path)))
		return res;

	return sym->obj.core->object_ops.resolve(sym->obj.tree,
						 &sym->obj.statdata,
						 path, from, key);
}

/* Releases passed @entity */
static void sym40_close(object_entity_t *entity) {
	sym40_t *sym = (sym40_t *)entity;
		
	aal_assert("umka-1170", entity != NULL);

	obj40_relock(&sym->obj, &sym->obj.statdata, NULL);
	aal_free(entity);
}

static reiser4_object_ops_t sym40_ops = {
#ifndef ENABLE_STAND_ALONE
	.create	      = sym40_create,
	.layout       = sym40_layout,
	.metadata     = sym40_metadata,
	.link         = sym40_link,
	.unlink       = sym40_unlink,
		
	.seek	      = NULL,
	.write	      = NULL,
	.truncate     = NULL,
	.rem_entry    = NULL,
	.add_entry    = NULL,
#endif
	.lookup	      = NULL,
	.reset	      = NULL,
	.offset	      = NULL,
	.size         = NULL,
	.readdir      = NULL,
	.telldir      = NULL,
	.seekdir      = NULL,

#ifndef ENABLE_SYMLINKS_SUPPORT
	.read	      = sym40_read,
#else
	.read	      = NULL,
#endif
	
	.open	      = sym40_open,
	.close	      = sym40_close,
	.follow       = sym40_follow
};

static reiser4_plugin_t sym40_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = OBJECT_SYMLINK40_ID,
		.group = SYMLINK_OBJECT,
		.type = OBJECT_PLUGIN_TYPE,
#ifndef ENABLE_STAND_ALONE
		.label = "sym40",
		.desc = "Symlink for reiser4, ver. " VERSION
#endif
	},
	.o = {
		.object_ops = &sym40_ops
	}
};

static reiser4_plugin_t *sym40_start(reiser4_core_t *c) {
	core = c;
	return &sym40_plugin;
}

plugin_register(sym40, sym40_start, NULL);
#endif
