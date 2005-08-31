/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40.c -- reiser4 symlink file plugin. */

#include "sym40.h"
#include "sym40_repair.h"

#ifdef ENABLE_SYMLINKS

reiser4_core_t *sym40_core = NULL;

/* Opens symlink and returns initialized instance to caller. */
object_entity_t *sym40_open(object_info_t *info) {
	sym40_t *sym;

	aal_assert("umka-1163", info != NULL);
	aal_assert("umka-1164", info->tree != NULL);
 	
	if (info->start.plug->id.group != STAT_ITEM)
		return NULL;
   
	if (info->opset.plug[OPSET_OBJ] != &sym40_plug)
		return NULL;
	
	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return NULL;

	/* Initalizing file handle */
	obj40_init(&sym->obj, info, sym40_core);
	
	/* Initializing statdata place */
	aal_memcpy(STAT_PLACE(&sym->obj), &info->start,
		   sizeof(info->start));
	
	return (object_entity_t *)sym;
}

/* Reads whole symlink data to passed @buff. */
static int64_t sym40_read(object_entity_t *entity, 
			  void *buff, uint64_t n)
{
	errno_t res;
	sym40_t *sym;

	aal_assert("umka-1571", buff != NULL);
	aal_assert("umka-1570", entity != NULL);

	sym = (sym40_t *)entity;

	/* Update stat data coord. */
	if ((res = obj40_update(&sym->obj)))
		return res;

	/* Reading symlink extension data. */
	if ((res = obj40_read_ext(&sym->obj, SDEXT_SYMLINK_ID, buff)))
		return res;

	return aal_strlen(buff);
}

/* Loads symlink stat data to passed @hint */
static errno_t sym40_stat(object_entity_t *entity, stat_hint_t *hint) {
	sym40_t *sym;
	
	aal_assert("umka-2557", entity != NULL);

	sym = (sym40_t *)entity;
	return obj40_load_stat(&sym->obj, hint);
}

#ifndef ENABLE_MINIMAL
/* Creates symlink and returns initialized instance to the caller */
static object_entity_t *sym40_create(object_hint_t *hint) {
	sym40_t *sym;
	uint32_t len;
	
	aal_assert("umka-1740", hint != NULL);
	aal_assert("vpf-1094",  hint->info.tree != NULL);

	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return NULL;
	
	/* Inizializes symlink file handle. */
	obj40_init(&sym->obj, &hint->info, sym40_core);

	len = aal_strlen(hint->name);

	/* Create symlink sta data item. */
	if (obj40_create_stat(&sym->obj, len, 0, 0, 0, S_IFLNK, hint->name))
		goto error_free_sym;

	return (object_entity_t *)sym;

 error_free_sym:
	aal_free(sym);
	return NULL;
}

/* Clober symlink, that is clobber its stat data. */
static errno_t sym40_clobber(object_entity_t *entity) {
	aal_assert("umka-2300", entity != NULL);
	return obj40_clobber(&((sym40_t *)entity)->obj);
}

/* Return number of hard links. */
static bool_t sym40_linked(object_entity_t *entity) {
	sym40_t *sym;
	
	aal_assert("umka-2295", entity != NULL);

	sym = (sym40_t *)entity;
	return obj40_links(&sym->obj) != 0;
}

/* Add one hard link. */
static errno_t sym40_link(object_entity_t *entity) {
	sym40_t *sym;
	
	aal_assert("umka-1915", entity != NULL);

	sym = (sym40_t *)entity;
	return obj40_link(&sym->obj);
}

/* Remove one hard link. */
static errno_t sym40_unlink(object_entity_t *entity) {
	sym40_t *sym;
	
	aal_assert("umka-1914", entity != NULL);

	sym = (sym40_t *)entity;
	return obj40_unlink(&sym->obj);
}

/* Calls function @place_func for each symlink item (statdata only) */
static errno_t sym40_metadata(object_entity_t *entity,
			      place_func_t place_func,
			      void *data)
{
	sym40_t *sym;

	aal_assert("umka-1718", entity != NULL);
	aal_assert("umka-1719", place_func != NULL);

	sym = (sym40_t *)entity;
	return obj40_metadata(&sym->obj, place_func, data);
}

/* Updates symlink stat data from passed @hint */
static errno_t sym40_update(object_entity_t *entity, stat_hint_t *hint) {
	sym40_t *sym;
	
	aal_assert("umka-2559", entity != NULL);
	aal_assert("umka-2560", hint != NULL);

	sym = (sym40_t *)entity;
	return obj40_save_stat(&sym->obj, hint);
}
#endif

/* This function reads symlink, parses it by means of using aux_parse_path()
   with applying corresponding callback fucntions for searching stat data and
   searchig all entries. It returns stat data key of the object symlink points
   to. */
static errno_t sym40_follow(object_entity_t *entity,
			    reiser4_key_t *from,
			    reiser4_key_t *key)
{
	sym40_t *sym = (sym40_t *)entity;
	uint32_t size;
	errno_t res;
	char *path;
	
	aal_assert("umka-1775", key != NULL);
	aal_assert("umka-2245", from != NULL);
	aal_assert("umka-1774", entity != NULL);

	/* Maximal symlink size is MAX_ITEM_LEN. Take the block size to 
	   simplify it. */
	size = place_blksize(STAT_PLACE(&sym->obj));
	if (!(path = aal_calloc(size, 0)))
		return -ENOMEM;
	
	/* Read symlink data to @path */
	if ((res = sym40_read(entity, path, size) < 0))
		goto error;

	/* Calling symlink parse function and resolution function. */
	if ((res = sym->obj.core->object_ops.resolve(sym->obj.info.tree,
						     path, from, key)))
		goto error;

	aal_free(path);
	return 0;
	
 error:
	aal_free(path);
	return res;
}

/* Releases passed @entity */
static void sym40_close(object_entity_t *entity) {
	aal_assert("umka-1170", entity != NULL);

	aal_free(entity);
}

/* Symlinks operations. */
static reiser4_object_ops_t sym40_ops = {
#ifndef ENABLE_MINIMAL
	.create	        = sym40_create,
	.metadata       = sym40_metadata,
	.link           = sym40_link,
	.unlink         = sym40_unlink,
	.linked         = sym40_linked,
	.update         = sym40_update,
	.clobber        = sym40_clobber,
	.recognize	= sym40_recognize,
	.check_struct   = sym40_check_struct,

	.layout         = NULL,
	.seek	        = NULL,
	.write	        = NULL,
	.convert        = NULL,
	.truncate       = NULL,
	.rem_entry      = NULL,
	.add_entry      = NULL,
	.build_entry    = NULL,
	.attach         = NULL,
	.detach         = NULL,
	
	.fake		= NULL,
	.check_attach 	= NULL,
#endif
	.lookup	        = NULL,
	.reset	        = NULL,
	.offset	        = NULL,
	.readdir        = NULL,
	.telldir        = NULL,
	.seekdir        = NULL,

	.stat           = sym40_stat,
	.read	        = sym40_read,
	.open	        = sym40_open,
	.close	        = sym40_close,
	.follow         = sym40_follow
};

/* Symlink plugin itself. */
reiser4_plug_t sym40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_SYM40_ID, SYM_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "sym40",
	.desc  = "Symlink file plugin.",
#endif
	.o = {
		.object_ops = &sym40_ops
	}
};

static reiser4_plug_t *sym40_start(reiser4_core_t *c) {
	sym40_core = c;
	return &sym40_plug;
}

plug_register(sym40, sym40_start, NULL);
#endif
