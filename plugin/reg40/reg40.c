/*
    reg40.c -- reiser4 default regular file plugin.
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

static errno_t reg40_reset(reiser4_entity_t *entity) {
    rpid_t pid;
    reiser4_key_t key;
    
    reg40_t *reg = (reg40_t *)entity;
    
    aal_assert("umka-1161", reg != NULL, return -1);
    
    key.plugin = reg->key.plugin;
    plugin_call(return -1, key.plugin->key_ops, build_generic, 
	key.body, KEY_FILEBODY_TYPE, reg40_locality(reg), 
	reg40_objectid(reg), 0);
    
    if (core->tree_ops.lookup(reg->tree, &key, &reg->place) != 1) {
	aal_exception_error("Can't find stream of regular file 0x%llx.", 
	    reg40_objectid(reg));

	return -1;
    }

    if ((pid = core->tree_ops.item_pid(reg->tree, &reg->place, 
	ITEM_PLUGIN_TYPE)) == INVALID_PLUGIN_ID)
    {
	aal_exception_error("Can't get regular file plugin id from the tree.");
	return -1;
    }
    
    if (!(reg->body.plugin = 
	core->factory_ops.plugin_ifind(ITEM_PLUGIN_TYPE, pid)))
    {
	aal_exception_error("Can't find item plugin by its id 0x%x.", pid);
	return -1;
    }
    
    if (core->tree_ops.item_body(reg->tree, 
	&reg->place, &reg->body.body, &reg->body.len))
    {
	aal_exception_error("Can't get body of file 0x%x.", 
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
    if (core->tree_ops.lookup(reg->tree, &reg->key, &reg->place) != 1) {

	aal_exception_error("Can't find stat data of the file with oid 0x%llx.", 
	    reg40_objectid(reg));
	
	return -1;
    }
    
    /* 
	Initializing stat data plugin after reg40_realize function find and 
	grab pointer to the statdata item.
    */
    if ((pid = core->tree_ops.item_pid(reg->tree, &reg->place, 
	ITEM_PLUGIN_TYPE)) == INVALID_PLUGIN_ID)
    {
	aal_exception_error("Can't get stat data plugin id of the file 0x%llx.",
	    reg40_objectid(reg));
	
	return -1;
    }
    
    if (!(reg->statdata.plugin = 
	core->factory_ops.plugin_ifind(ITEM_PLUGIN_TYPE, pid)))
    {
	aal_exception_error("Can't find stat data item plugin "
	    "by its id 0x%x.", pid);
	return -1;
    }
    
    {
	errno_t res = core->tree_ops.item_body(reg->tree, &reg->place, 
	    &reg->statdata.body, NULL);

	if (res) return res;
    }
    
    return 0;
}

static errno_t reg40_next(reiser4_entity_t *entity) {
    rpid_t pid;
    roid_t curr_objectid;
    roid_t next_objectid;

    reiser4_key_t key;
    reg40_t *reg = (reg40_t *)entity;

    reiser4_place_t *place = &reg->place;
    reiser4_place_t save_place = reg->place;
    reiser4_plugin_t *save_plugin = reg->body.plugin;
    
    /* Getting the right neighbour */
    if (core->tree_ops.item_right(reg->tree, place))
        goto error_set_context;
    
    /* Getting key of the first item in the right neightbour */
    if (core->tree_ops.item_key(reg->tree, place, &key)) {
        aal_exception_error("Can't get next item key by coord.");
	goto error_set_context;
    }
    
    pid = core->tree_ops.item_pid(reg->tree, place, ITEM_PLUGIN_TYPE);
	
    if (!(reg->body.plugin = 
	core->factory_ops.plugin_ifind(ITEM_PLUGIN_TYPE, pid)))
    {
	aal_exception_error("Can't find item plugin by "
	    "its id %x.", pid);
	
	goto error_set_context;
    }
    
    if (reg->body.plugin->h.type != ITEM_PLUGIN_TYPE ||
	(reg->body.plugin->item_ops.type != TAIL_ITEM_TYPE &&
	reg->body.plugin->item_ops.type != EXTENT_ITEM_TYPE))
    {
	/* Next item is nor tail neither extent */
	goto error_set_context;
    }
    
    /* 
        Getting locality of both keys in order to determine is they are 
        mergeable.
    */
    curr_objectid = plugin_call(goto error_set_context, 
	reg->key.plugin->key_ops, get_objectid, reg->key.body);
	
    next_objectid = plugin_call(goto error_set_context, 
	reg->key.plugin->key_ops, get_objectid, key.body);
	
    /* Determining is items are mergeable */
    if (curr_objectid == next_objectid) {
	    
	if (core->tree_ops.item_body(reg->tree, place, 
		&reg->body.body, &reg->body.len))
	    goto error_set_context;
	
	reg->place.pos.unit = 0;
	return 0;
    }
error_set_context:
    *place = save_place;
    reg->body.plugin = save_plugin;
    return -1;
}

/* Reads n entries to passed buffer buff */
static int32_t reg40_read(reiser4_entity_t *entity, 
    char *buff, uint32_t n)
{
    uint32_t read;
    reg40_t *reg = (reg40_t *)entity;

    aal_assert("umka-1182", entity != NULL, return 0);
    aal_assert("umka-1183", buff != NULL, return 0);
    
    for (read = 0; read < n; ) {
	uint32_t chunk;
	
	if (reg->place.pos.unit >= reg->body.len) {

	    /* Getting the next file body item */
	    if (reg40_next(entity))
		break;
	}
	
	chunk = (reg->body.len - reg->place.pos.unit) > n - read ?
	    n - read : (reg->body.len - reg->place.pos.unit);

	if (!chunk) break;
	
	if (reg->body.plugin->item_ops.type == TAIL_ITEM_TYPE) {

	    /* Getting the data from the tail item */
	    aal_memcpy(buff + read, reg->body.body + 
		reg->place.pos.unit, chunk);

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
    plugin_call(goto error_free_reg, object->plugin->key_ops,
	assign, reg->key.body, object->body);
    
    /* Grabbing the stat data item */
    if (reg40_realize(reg)) {
	aal_exception_error("Can't grab stat data of the file "
	    "with oid 0x%llx.", reg40_objectid(reg));
	goto error_free_reg;
    }
    
    if (reg40_reset((reiser4_entity_t *)reg)) {
	aal_exception_error("Can't reset file 0x%llx.", reg40_objectid(reg));
	goto error_free_reg;
    }
    
    return (reiser4_entity_t *)reg;

error_free_reg:
    aal_free(reg);
    return NULL;
}

#ifndef ENABLE_COMPACT

static reiser4_entity_t *reg40_create(const void *tree, 
    reiser4_key_t *parent, reiser4_key_t *object, 
    reiser4_file_hint_t *hint) 
{
    reg40_t *reg;
    
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
    
    locality = plugin_call(return NULL, object->plugin->key_ops, 
	get_objectid, parent->body);
    
    objectid = plugin_call(return NULL, object->plugin->key_ops, 
	get_objectid, object->body);
    
    parent_locality = plugin_call(return NULL, object->plugin->key_ops, 
	get_locality, parent->body);
    
    if (!(reg->statdata.plugin = 
	core->factory_ops.plugin_ifind(ITEM_PLUGIN_TYPE, hint->statdata_pid)))
    {
	aal_exception_error("Can't find stat data item plugin by its id 0x%x.", 
	    hint->statdata_pid);
	
	goto error_free_reg;
    }
    
    /* Initializing the stat data hint */
    aal_memset(&stat_hint, 0, sizeof(stat_hint));
    
    stat_hint.plugin = reg->statdata.plugin;
    
    stat_hint.key.plugin = object->plugin;
    plugin_call(goto error_free_reg, object->plugin->key_ops,
	assign, stat_hint.key.body, object->body);
    
    /* Initializing stat data item hint. */
    stat.extmask = 1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID;
    
    lw_ext.mode = S_IFDIR | 0755;
    lw_ext.nlink = 2;
    lw_ext.size = 0;
    
    unix_ext.uid = getuid();
    unix_ext.gid = getgid();
    unix_ext.atime = time(NULL);
    unix_ext.mtime = time(NULL);
    unix_ext.ctime = time(NULL);
    unix_ext.rdev = 0;

    /* Taken space, should be changed by write */
    unix_ext.bytes = 0;

    aal_memset(&stat.extentions, 0, sizeof(stat.extentions));
    
    stat.extentions.count = 2;
    stat.extentions.hint[0] = &lw_ext;
    stat.extentions.hint[1] = &unix_ext;

    stat_hint.hint = &stat;
    
    /* Calling balancing code in order to insert statdata item into the tree */
    if (core->tree_ops.item_insert(tree, &stat_hint)) {
	aal_exception_error("Can't insert stat data item of object 0x%llx into "
	    "the thee.", objectid);
	goto error_free_reg;
    }
    
    reg->key.plugin = object->plugin;
    plugin_call(goto error_free_reg, object->plugin->key_ops,
	assign, reg->key.body, object->body);
    
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

/* Adds n entries from buff to passed entity */
static int32_t reg40_write(reiser4_entity_t *entity, 
    char *buff, uint32_t n) 
{
    reiser4_item_hint_t hint;
    reg40_t *reg = (reg40_t *)entity;

    aal_assert("umka-1184", entity != NULL, return -1);
    aal_assert("umka-1185", entity != NULL, return -1);
    
    /* 
	FIXME-UMKA: Here we should also check if n greater than max space in 
	node. If so, we should split buffer onto few parts and insert them 
	separately.
    */
    
    hint.len = n;
    hint.data = buff;
    hint.plugin = /*reg->body.plugin*/core->factory_ops.plugin_ifind(ITEM_PLUGIN_TYPE, 
	ITEM_TAIL40_ID);

    /* 
	FIXME-UMKA: Here tail policy plugin should decide what kind of item (tail 
	or extent) we have to insert. And we should build the key basing on that 
	desicion. 
    */
    hint.key.plugin = reg->key.plugin;
    plugin_call(return 0, hint.key.plugin->key_ops, 
	build_generic, hint.key.body, KEY_FILEBODY_TYPE, reg40_locality(reg), 
	reg40_objectid(reg), reg->offset);
    
    /* Inserting the entry to the tree */
    if (core->tree_ops.item_insert(reg->tree, &hint)) {
        aal_exception_error("Can't insert body item to the thee.");
	return 0;
    }
    
    reg->offset += n;
    return 0;
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

static errno_t reg40_seek(reiser4_entity_t *entity, 
    uint64_t offset) 
{
    reg40_t *reg = (reg40_t *)entity;
    
    aal_assert("umka-1171", entity != NULL, return 0);

    /* FIXME-UMKA: Not implemented yet! */

    reg->offset = offset;
    return -1;
}

static reiser4_plugin_t reg40_plugin = {
    .file_ops = {
	.h = {
	    .handle = NULL,
	    .id = FILE_REGULAR40_ID,
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

