/*
    dir40.c -- reiser4 default directory object plugin.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Yury Umanets.
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

#include "dir40.h"

extern reiser4_plugin_t dir40_plugin;

static reiser4_core_t *core = NULL;

static roid_t dir40_objectid(dir40_t *dir) {
    aal_assert("umka-839", dir != NULL, return 0);
    
    return plugin_call(return 0, dir->key.plugin->key_ops, 
	get_objectid, dir->key.body);
}

static roid_t dir40_locality(dir40_t *dir) {
    aal_assert("umka-839", dir != NULL, return 0);
    
    return plugin_call(return 0, dir->key.plugin->key_ops, 
	get_locality, dir->key.body);
}

static errno_t dir40_reset(reiser4_entity_t *entity) {
    reiser4_key_t key;
    dir40_t *dir = (dir40_t *)entity;
    
    aal_assert("umka-864", dir != NULL, return -1);
    
    /* Preparing key of the first entry in directory */
    key.plugin = dir->key.plugin;
    plugin_call(return -1, key.plugin->key_ops, build_direntry, key.body, 
	dir->hash, dir40_locality(dir), dir40_objectid(dir), ".");
	    
    if (core->tree_ops.lookup(dir->tree, &key, LEAF_LEVEL, &dir->place) != 1) {
	aal_exception_error("Can't find direntry of object 0x%llx.", 
	    dir40_objectid(dir));
	return -1;
    }

    if (core->item_ops.open(&dir->body, &dir->place)) {
	aal_exception_error("Can't open the body of direcrtory 0x%llx.",
	    dir40_objectid(dir));
	return -1;
    }
    
    dir->offset = 0;
    dir->place.pos.unit = 0;

    return 0;
}

/* Trying to guess hash in use by stat  dfata extention */
static reiser4_plugin_t *dir40_guess(dir40_t *dir) {
    /* 
	FIXME-UMKA: This functions should inspect stat data extention first. And
	only in the case there are not convenient plugin extention (hash plugin),
	it should use some default hash plugin id.
    */
    return core->factory_ops.ifind(HASH_PLUGIN_TYPE, HASH_R5_ID);
}

/* This function grabs the stat data of directory */
static errno_t dir40_realize(dir40_t *dir) {
    aal_assert("umka-857", dir != NULL, return -1);	

    plugin_call(return -1, dir->key.plugin->key_ops, build_generic, 
	dir->key.body, KEY_STATDATA_TYPE, dir40_locality(dir), 
	dir40_objectid(dir), 0);
    
    /* Positioning to the dir stat data */
    if (core->tree_ops.lookup(dir->tree, &dir->key, LEAF_LEVEL, 
	&dir->place) != 1) 
    {
	aal_exception_error("Can't find stat data of directory 0x%llx.", 
	    dir40_objectid(dir));
	return -1;
    }
    
    if (core->item_ops.open(&dir->statdata, &dir->place)) {
	aal_exception_error("Can't open the stadata of directory 0x%llx.",
	    dir40_objectid(dir));
	return -1;
    }
    
    if (!(dir->hash = dir40_guess(dir))) {
	aal_exception_error("Can't guess hash plugin for directory %llx.", 
	    dir40_objectid(dir));
	return -1;
    }

    return 0;
}

static errno_t dir40_next(reiser4_entity_t *entity) {
    reiser4_key_t next_key;
    reiser4_item_t next_item;
    
    roid_t curr_locality;
    roid_t next_locality;

    dir40_t *dir = (dir40_t *)entity;
    reiser4_place_t *place = &dir->place;
    reiser4_place_t save_place = dir->place;

    /* Getting the right neighbour */
    if (core->tree_ops.right(dir->tree, place))
        goto error_set_context;
    
    if (core->item_ops.open(&next_item, place))
	goto error_set_context;
    
    if (next_item.plugin->h.id != dir->body.plugin->h.id)
        goto error_set_context;
	
    /* Getting key of the first item in the right neightbour */
    if (core->item_ops.key(&next_item, &next_key)) {
        aal_exception_error("Can't get next item key by coord.");
        goto error_set_context;
    }
    
    /* 
        Getting locality of both keys in order to determine is they are 
        mergeable.
    */
    curr_locality = plugin_call(goto error_set_context, 
	dir->key.plugin->key_ops, get_locality, dir->key.body);
	
    next_locality = plugin_call(goto error_set_context, 
	dir->key.plugin->key_ops, get_locality, next_key.body);
	
    /* Determining is items are mergeable */
    if (curr_locality == next_locality) {
	dir->body = next_item;
	return 0;
    }

error_set_context:
    *place = save_place;
    return -1;
}

/* Reads n entries to passed buffer buff */
static int32_t dir40_read(reiser4_entity_t *entity, 
    void *buff, uint32_t n)
{
    uint32_t i, count;
    reiser4_plugin_t *plugin;
    
    dir40_t *dir = (dir40_t *)entity;

    reiser4_entry_hint_t *entry = 
    	(reiser4_entry_hint_t *)buff;
    
    aal_assert("umka-844", dir != NULL, return -1);
    aal_assert("umka-845", entry != NULL, return -1);

    plugin = core->item_ops.plugin(&dir->body);
       
    /* Getting the number of entries */
    if (!(count = plugin_call(return -1, plugin->item_ops, count, &dir->body)))
	return -1;
    
    for (i = 0; i < n; i++) {
	if (dir->place.pos.unit >= count) {
	    
	    /* Now we are trying to get next direntry item */
	    if (dir40_next(entity))
		break;
	}
    
	/* Getting next entry from the current direntry item */
	if ((plugin_call(break, plugin->item_ops.specific.direntry, 
		entry, &dir->body, dir->place.pos.unit, entry)))
	    break;

	entry++;
	dir->offset++; 
	dir->place.pos.unit++; 
    }
    
    return i;
}

/* 
    Makes lookup in directory by name. Returns the key of the stat data item found
    entry points to.
*/
static int dir40_lookup(reiser4_entity_t *entity, 
    char *name, reiser4_key_t *key) 
{
    reiser4_key_t wanted;
    reiser4_plugin_t *plugin;
    dir40_t *dir = (dir40_t *)entity;
    
    aal_assert("umka-1117", entity != NULL, return -1);
    aal_assert("umka-1118", name != NULL, return -1);

    aal_assert("umka-1119", key != NULL, return -1);
    aal_assert("umka-1120", key->plugin != NULL, return -1);

    /* Forming the directory key */
    wanted.plugin = dir->key.plugin;
    plugin_call(return -1, wanted.plugin->key_ops, build_direntry, wanted.body,
	dir->hash, dir40_locality(dir), dir40_objectid(dir), name);
    
    plugin = core->item_ops.plugin(&dir->body);

    while (1) {
	reiser4_item_t item;
	    
	if (plugin_call(return -1, plugin->item_ops, lookup, 
	    &dir->body, &wanted, &dir->place.pos.unit) == 1) 
	{
	    roid_t locality;
	    reiser4_entry_hint_t entry;
	    
	    if ((plugin_call(return -1, plugin->item_ops.specific.direntry, 
		    entry, &dir->body, dir->place.pos.unit, &entry)))
		return -1;

	    locality = plugin_call(return -1, key->plugin->key_ops,
		get_locality, &entry.objid);
	    
	    plugin_call(return -1, key->plugin->key_ops, build_generic,
		key->body, KEY_STATDATA_TYPE, locality, entry.objid.objectid, 0);
	    
	    return 1;
	}
	
	/* Now we are trying to get next direntry item */
	if (dir40_next(entity))
	    return 0;
    }
    
    return 0;
}

static reiser4_entity_t *dir40_open(const void *tree, 
    reiser4_key_t *object) 
{
    dir40_t *dir;

    aal_assert("umka-836", tree != NULL, return NULL);
    aal_assert("umka-837", object != NULL, return NULL);
    aal_assert("umka-838", object->plugin != NULL, return NULL);
    
    if (!(dir = aal_calloc(sizeof(*dir), 0)))
	return NULL;
    
    dir->tree = tree;
    dir->plugin = &dir40_plugin;
    
    dir->key.plugin = object->plugin;
    plugin_call(goto error_free_dir, object->plugin->key_ops,
	assign, dir->key.body, object->body);
    
    /* Grabbing stat data */
    if (dir40_realize(dir)) {
	aal_exception_error("Can't grab stat data of  directory with oid 0x%llx.", 
	    dir40_objectid(dir));
	goto error_free_dir;
    }
    
    /* Positioning to the first directory unit */
    if (dir40_reset((reiser4_entity_t *)dir)) {
	aal_exception_error("Can't rewind directory with oid 0x%llx.", 
	    dir40_objectid(dir));
	goto error_free_dir;
    }
    
    return (reiser4_entity_t *)dir;

error_free_dir:
    aal_free(dir);
    return NULL;
}

#ifndef ENABLE_COMPACT

static reiser4_entity_t *dir40_create(const void *tree, 
    reiser4_key_t *parent, reiser4_key_t *object, 
    reiser4_file_hint_t *hint) 
{
    dir40_t *dir;
    
    reiser4_statdata_hint_t stat;
    reiser4_direntry_hint_t body;
    reiser4_item_hint_t stat_hint;
    reiser4_item_hint_t body_hint;
   
    reiser4_sdext_lw_hint_t lw_ext;
    reiser4_sdext_unix_hint_t unix_ext;
    
    roid_t parent_locality;
    roid_t objectid, locality;

    reiser4_plugin_t *stat_plugin;
    reiser4_plugin_t *body_plugin;
    
    aal_assert("umka-835", tree != NULL, return NULL);
    aal_assert("umka-743", parent != NULL, return NULL);
    aal_assert("umka-744", object != NULL, return NULL);
    aal_assert("umka-881", object->plugin != NULL, return NULL);

    if (!(dir = aal_calloc(sizeof(*dir), 0)))
	return NULL;
    
    dir->tree = tree;
    dir->plugin = &dir40_plugin;
    
    dir->key.plugin = object->plugin;
    plugin_call(goto error_free_dir, object->plugin->key_ops, assign, 
	dir->key.body, object->body);
    
    if (!(dir->hash = core->factory_ops.ifind(HASH_PLUGIN_TYPE, 
	hint->body.dir.hash_pid)))
    {
	aal_exception_error("Can't find hash plugin by its id 0x%x.", 
	    hint->body.dir.hash_pid);
	goto error_free_dir;
    }
    
    locality = plugin_call(return NULL, 
	object->plugin->key_ops, get_objectid, parent->body);
    
    objectid = plugin_call(return NULL, 
	object->plugin->key_ops, get_objectid, object->body);
    
    parent_locality = plugin_call(return NULL, 
	object->plugin->key_ops, get_locality, parent->body);
    
    if (!(stat_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
	hint->statdata_pid)))
    {
	aal_exception_error("Can't find stat data item plugin by its id 0x%x.", 
	    hint->statdata_pid);
	
	goto error_free_dir;
    }
   
    {
	rpid_t body_pid = hint->body.dir.direntry_pid;

	if (!(body_plugin = core->factory_ops.ifind(ITEM_PLUGIN_TYPE, 
	    body_pid)))
	{
	    aal_exception_error("Can't find direntry item plugin by its id 0x%x.", 
		body_pid);
	    goto error_free_dir;
	}
    }
    
    /* 
	Initializing direntry item hint. This should be done earlier than 
	initializing of the stat data item hint, because we will need size 
	of direntry item durring stat data initialization.
    */
    aal_memset(&body_hint, 0, sizeof(body_hint));

    body.count = 2;
    body_hint.plugin = body_plugin;
    body_hint.key.plugin = object->plugin; 
   
    plugin_call(goto error_free_dir, object->plugin->key_ops, build_direntry, 
	body_hint.key.body, dir->hash, locality, objectid, ".");

    if (!(body.entry = aal_calloc(body.count*sizeof(*body.entry), 0)))
        goto error_free_dir;
    
    /* Preparing dot entry */
    body.entry[0].name = ".";
    
    plugin_call(goto error_free_dir, object->plugin->key_ops, build_objid, 
	&body.entry[0].objid, KEY_STATDATA_TYPE, locality, objectid);
	
    plugin_call(goto error_free_dir, object->plugin->key_ops, build_entryid, 
	&body.entry[0].entryid, dir->hash, body.entry[0].name);
    
    /* Preparing dot-dot entry */
    body.entry[1].name = "..";
    
    plugin_call(goto error_free_dir, object->plugin->key_ops, build_objid, 
	&body.entry[1].objid, KEY_STATDATA_TYPE, parent_locality, locality);
	
    plugin_call(goto error_free_dir, object->plugin->key_ops, build_entryid, 
	&body.entry[1].entryid, dir->hash, body.entry[1].name);
    
    body_hint.hint = &body;

    /* Initializing stat data hint */
    aal_memset(&stat_hint, 0, sizeof(stat_hint));
    
    stat_hint.plugin = stat_plugin;
    stat_hint.key.plugin = object->plugin;
    
    plugin_call(goto error_free_dir, object->plugin->key_ops,
	assign, stat_hint.key.body, object->body);
    
    /* Initializing stat data item hint. */
    stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID;
    
    lw_ext.mode = S_IFDIR | 0755;
    lw_ext.nlink = 2;
    lw_ext.size = 2;
    
    unix_ext.uid = getuid();
    unix_ext.gid = getgid();
    unix_ext.atime = time(NULL);
    unix_ext.mtime = time(NULL);
    unix_ext.ctime = time(NULL);
    unix_ext.rdev = 0;

    if (plugin_call(goto error_free_dir, body_plugin->item_ops, estimate, 
	NULL, ~0ul, &body_hint))
    {
	aal_exception_error("Can't estimate directory item.");
	goto error_free_dir;
    }
    
    unix_ext.bytes = body_hint.len;
    
    aal_memset(&stat.ext, 0, sizeof(stat.ext));
    
    stat.ext.count = 2;
    stat.ext.hint[0] = &lw_ext;
    stat.ext.hint[1] = &unix_ext;

    stat_hint.hint = &stat;
    
    /* Calling balancing code in order to insert statdata item into the tree */
    if (core->tree_ops.insert(tree, &stat_hint, LEAF_LEVEL, NULL)) {
	aal_exception_error("Can't insert stat data item of object 0x%llx "
	    "into the thee.", objectid);
	goto error_free_dir;
    }
    
    /* Inserting the direntry item into the tree */
    if (core->tree_ops.insert(tree, &body_hint, LEAF_LEVEL, NULL)) {
	aal_exception_error("Can't insert direntry item of object 0x%llx "
	    "into the thee.", objectid);
	goto error_free_dir;
    }
    
    if (dir40_realize(dir)) {
	aal_exception_error("Can't open stat data item of directory 0x%llx.",
	    dir40_objectid(dir));
	goto error_free_dir;
    }
    
    if (dir40_reset((reiser4_entity_t *)dir)) {
	aal_exception_error("Can't open body of directory 0x%llx.",
	    dir40_objectid(dir));
	goto error_free_dir;
    }
    
    aal_free(body.entry);
    
    return (reiser4_entity_t *)dir;

error_free_dir:
    aal_free(dir);
error:
    return NULL;
}

static errno_t dir40_truncate(reiser4_entity_t *entity, 
    uint64_t n) 
{
    /* Sorry, not implemented yet! */
    return -1;
}

/* Adds n entries from buff to passed entity */
static int32_t dir40_write(reiser4_entity_t *entity, 
    void *buff, uint32_t n) 
{
    uint64_t i;

    reiser4_item_hint_t hint;
    dir40_t *dir = (dir40_t *)entity;
    reiser4_direntry_hint_t body_hint;
    
    reiser4_entry_hint_t *entry = 
	(reiser4_entry_hint_t *)buff;
    
    aal_assert("umka-844", dir != NULL, return -1);
    aal_assert("umka-845", entry != NULL, return -1);
   
    aal_memset(&hint, 0, sizeof(hint));
    aal_memset(&body_hint, 0, sizeof(body_hint));

    body_hint.count = 1;

    /* 
	FIXME-UMKA: Here we have the funny situation. Direntry plugin can insert 
	more than one entry in turn, but they should be sorted before. Else we 
	will have broken direntry. So, we should either perform a loop here in 
	order to insert	all n entris, or we should sort entries whenever (in 
	direntry plugin or here).
    */
    if (!(body_hint.entry = aal_calloc(sizeof(*entry), 0)))
	return 0;
    
    hint.hint = &body_hint;
  
    for (i = 0; i < n; i++) {
	plugin_call(break, dir->key.plugin->key_ops, build_objid, &entry->objid, 
	    KEY_STATDATA_TYPE, entry->objid.locality, entry->objid.objectid);
	
	plugin_call(break, dir->key.plugin->key_ops, build_entryid, &entry->entryid, 
	    dir->hash, entry->name);
    
	aal_memcpy(&body_hint.entry[0], entry, sizeof(*entry));
    
	hint.key.plugin = dir->key.plugin;
	plugin_call(break, hint.key.plugin->key_ops, build_direntry, hint.key.body, 
	    dir->hash, dir40_locality(dir), dir40_objectid(dir), entry->name);
    
	hint.len = 0;
	hint.plugin = dir->body.plugin;
    
	/* Inserting the entry to the tree */
	if (core->tree_ops.insert(dir->tree, &hint, LEAF_LEVEL, NULL)) {
	    aal_exception_error("Can't add entry %s to the thee.", 
		entry->name);
	    break;
	}

	entry++;
    }
    
    aal_free(body_hint.entry);
    return i;
}

#endif

static void dir40_close(reiser4_entity_t *entity) {
    aal_assert("umka-750", entity != NULL, return);
    aal_free(entity);
}

static uint64_t dir40_offset(reiser4_entity_t *entity) {
    aal_assert("umka-874", entity != NULL, return 0);
    return ((dir40_t *)entity)->offset;
}

static errno_t dir40_seek(reiser4_entity_t *entity, 
    uint64_t offset) 
{
    dir40_t *dir = (dir40_t *)entity;
    
    aal_assert("umka-1130", entity != NULL, return 0);

    /* FIXME-UMKA: Not implemented yet! */

    dir->offset = offset;
    return -1;
}

static reiser4_plugin_t dir40_plugin = {
    .file_ops = {
	.h = {
	    .handle = NULL,
	    .id = DIR_DIR40_ID,
	    .type = DIR_PLUGIN_TYPE,
	    .label = "dir40",
	    .desc = "Compound directory for reiserfs 4.0, ver. " VERSION,
	},
#ifndef ENABLE_COMPACT
        .create	    = dir40_create,
        .write	    = dir40_write,
        .truncate   = dir40_truncate,
#else
        .create	    = NULL,
        .write	    = NULL,
        .truncate   = NULL,
#endif
        .valid	    = NULL,
        .open	    = dir40_open,
        .close	    = dir40_close,
        .reset	    = dir40_reset,
        .offset	    = dir40_offset,
        .seek	    = dir40_seek,
        .lookup	    = dir40_lookup,
	.read	    = dir40_read
    }
};

static reiser4_plugin_t *dir40_start(reiser4_core_t *c) {
    core = c;
    return &dir40_plugin;
}

plugin_register(dir40_start);

