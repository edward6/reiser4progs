/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40.c -- reiser4 symlink file plugin. */

#ifdef ENABLE_SYMLINKS

#include "sym40.h"
#include "sym40_repair.h"

reiser4_core_t *sym40_core = NULL;

/* Opens symlink and returns initialized instance to caller. */
object_entity_t *sym40_open(object_info_t *info) {
	sym40_t *sym;

	aal_assert("umka-1163", info != NULL);
	aal_assert("umka-1164", info->tree != NULL);
 	
	if (info->start.plug->id.group != STATDATA_ITEM)
		return NULL;
   
	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return NULL;

	/* Initalizing file handle */
	obj40_init(&sym->obj, &sym40_plug, sym40_core, info);
	
	if (obj40_pid(&sym->obj, OBJECT_PLUG_TYPE, "symlink") != 
	    sym40_plug.id.id)
	{
		goto error_free_sym;
	}
	
	/* Initializing statdata place */
	aal_memcpy(STAT_PLACE(&sym->obj), &info->start,
		   sizeof(info->start));
	
	return (object_entity_t *)sym;
	
 error_free_sym:
	aal_free(sym);
	return NULL;
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
	if ((res = obj40_read_ext(STAT_PLACE(&sym->obj),
				  SDEXT_SYMLINK_ID, buff)))
	{
		return res;
	}

	return aal_strlen(buff);
}

/* Loads symlink stat data to passed @hint */
static errno_t sym40_stat(object_entity_t *entity,
			  statdata_hint_t *hint)
{
	sym40_t *sym;
	
	aal_assert("umka-2557", entity != NULL);
	aal_assert("umka-2558", hint != NULL);

	sym = (sym40_t *)entity;
	return obj40_load_stat(&sym->obj, hint);
}

#ifndef ENABLE_STAND_ALONE
/* Creates symlink and returns initialized instance to the caller */
static object_entity_t *sym40_create(object_info_t *info,
				     object_hint_t *hint)
{
	sym40_t *sym;
	uint32_t len;
	uint64_t mask;
	
	aal_assert("umka-1741", info != NULL);
	aal_assert("vpf-1094",  info->tree != NULL);
	aal_assert("umka-1740", hint != NULL);

	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return NULL;
	
	/* Inizializes symlink file handle. */
	obj40_init(&sym->obj, &sym40_plug, sym40_core, info);

	/* Initializing stat data extensions mask */
	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID |
		1 << SDEXT_SYMLINK_ID);

	len = aal_strlen(hint->body.sym);

	/* Create symlink sta data item. */
	if (obj40_create_stat(&sym->obj, hint->label.statdata, mask,
			      len, len, 0, 0, S_IFLNK, hint->body.sym))
	{
		goto error_free_sym;
	}

	/* Saving statdata place. */
	aal_memcpy(&info->start, STAT_PLACE(&sym->obj),
		   sizeof(info->start));

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
static uint32_t sym40_links(object_entity_t *entity) {
	sym40_t *sym;
	
	aal_assert("umka-2295", entity != NULL);

	sym = (sym40_t *)entity;
	return obj40_links(&sym->obj);
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
static errno_t sym40_update(object_entity_t *entity,
			    statdata_hint_t *hint)
{
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
			    key_entity_t *from,
			    key_entity_t *key)
{
	errno_t res;
	sym40_t *sym;
	char path[_SYMLINK_LEN];
	
	aal_assert("umka-1775", key != NULL);
	aal_assert("umka-2245", from != NULL);
	aal_assert("umka-1774", entity != NULL);

	sym = (sym40_t *)entity;

	/* Read symlink data to @path */
	if ((res = sym40_read(entity, path, sizeof(path)) < 0))
		return res;

	/* Calling symlink parse function and resolution function. */
	return sym->obj.core->object_ops.resolve(sym->obj.info.tree,
						 path, from, key);
}

/* Releases passed @entity */
static void sym40_close(object_entity_t *entity) {
	aal_assert("umka-1170", entity != NULL);

	aal_free(entity);
}

/* Symlinks operations. */
static reiser4_object_ops_t sym40_ops = {
#ifndef ENABLE_STAND_ALONE
	.create	        = sym40_create,
	.metadata       = sym40_metadata,
	.link           = sym40_link,
	.unlink         = sym40_unlink,
	.links          = sym40_links,
	.update         = sym40_update,
	.clobber        = sym40_clobber,
	.recognize	= sym40_recognize,
	.check_struct   = sym40_check_struct,

	.layout         = NULL,
	.form		= NULL,
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
	.cl    = CLASS_INIT,
	.id    = {OBJECT_SYM40_ID, SYM_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "sym40",
	.desc  = "Symlink plugin for reiser4, ver. " VERSION,
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
