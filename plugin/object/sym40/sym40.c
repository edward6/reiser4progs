/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40.c -- reiser4 symlink file plugin. */

#ifdef ENABLE_SYMLINKS
#include "sym40.h"

reiser4_core_t *score = NULL;
extern reiser4_plug_t sym40_plug;

/* Opens symlink and returns initialized instance to the caller */
object_entity_t *sym40_open(object_info_t *info) {
	sym40_t *sym;

	aal_assert("umka-1163", info != NULL);
	aal_assert("umka-1164", info->tree != NULL);
 	
	if (info->start.plug->id.group != STATDATA_ITEM)
		return NULL;
   
	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return NULL;

	/* Initalizing file handle */
	obj40_init(&sym->obj, &sym40_plug, score, info);
	
	if (obj40_pid(&sym->obj, OBJECT_PLUG_TYPE, "symlink") != 
	    sym40_plug.id.id)
	{
		goto error_free_sym;
	}
	
	/* Initialziing statdata place */
	aal_memcpy(STAT_PLACE(&sym->obj), &info->start,
		   sizeof(info->start));
	
	return (object_entity_t *)sym;
	
 error_free_sym:
	aal_free(sym);
	return NULL;
}

/* Reads @n bytes to passed buffer @buff */
static int64_t sym40_read(object_entity_t *entity, 
			  void *buff, uint64_t n)
{
	sym40_t *sym;
	errno_t res;

	aal_assert("umka-1571", buff != NULL);
	aal_assert("umka-1570", entity != NULL);

	sym = (sym40_t *)entity;

	if (obj40_update(&sym->obj))
		return -EINVAL;
	
	if ((res = obj40_read_ext(STAT_PLACE(&sym->obj),
				  SDEXT_SYMLINK_ID, buff)))
	{
		return res;
	}

	return aal_strlen(buff);
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
	
	/* Inizializes file handle */
	obj40_init(&sym->obj, &sym40_plug, score, info);

	mask = (1 << SDEXT_UNIX_ID | 1 << SDEXT_LW_ID |
		1 << SDEXT_SYMLINK_ID);

	len = aal_strlen(hint->body.sym);
	
	if (obj40_create_stat(&sym->obj, hint->statdata, mask,
			      len, len, 0, S_IFLNK, hint->body.sym))
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

static errno_t sym40_clobber(object_entity_t *entity) {
	sym40_t *sym;
	trans_hint_t hint;
	
	aal_assert("umka-2300", entity != NULL);

	sym = (sym40_t *)entity;
	
	if (obj40_update(&sym->obj))
		return -EINVAL;

	hint.count = 1;
	
	return obj40_remove(&sym->obj,
			    STAT_PLACE(&sym->obj), &hint);
}

static uint32_t sym40_links(object_entity_t *entity) {
	sym40_t *sym;
	
	aal_assert("umka-2295", entity != NULL);

	sym = (sym40_t *)entity;
	
	if (obj40_update(&sym->obj))
		return -EINVAL;

	return obj40_get_nlink(&sym->obj);
}

static errno_t sym40_link(object_entity_t *entity) {
	sym40_t *sym;
	
	aal_assert("umka-1915", entity != NULL);

	sym = (sym40_t *)entity;
	
	if (obj40_update(&sym->obj))
		return -EINVAL;
	
	return obj40_link(&sym->obj, 1);
}

static errno_t sym40_unlink(object_entity_t *entity) {
	sym40_t *sym;
	
	aal_assert("umka-1914", entity != NULL);

	sym = (sym40_t *)entity;
	
	if (obj40_update(&sym->obj))
		return -EINVAL;

	return obj40_link(&sym->obj, -1);
}

/* Calls function @func for each symlink item (statdata only) */
static errno_t sym40_metadata(object_entity_t *entity,
			      place_func_t place_func,
			      void *data)
{
	sym40_t *sym;

	aal_assert("umka-1718", entity != NULL);
	aal_assert("umka-1719", place_func != NULL);

	sym = (sym40_t *)entity;

	if (obj40_update(&sym->obj))
		return -EINVAL;

	return place_func(entity, STAT_PLACE(&sym->obj), data);
}

/* Calls function @func for each block symlink items lie in */
static errno_t sym40_layout(object_entity_t *entity,
			    region_func_t region_func,
			    void *data)
{
	blk_t blk;
	sym40_t *sym;

	aal_assert("umka-1720", entity != NULL);
	aal_assert("umka-1721", region_func != NULL);

	sym = (sym40_t *)entity;

	if (obj40_update(&sym->obj))
		return -EINVAL;
	
	blk = STAT_PLACE(&sym->obj)->block->nr;
	return region_func(entity, blk, 1, data);
}

extern object_entity_t *sym40_recognize(object_info_t *info);

extern errno_t sym40_check_struct(object_entity_t *object,
				  place_func_t place_func,
				  region_func_t region_func,
				  void *data, uint8_t mode);
#endif

/* This function reads symlink and parses it by means of using aux_parse_path
   with applying corresponding callback fucntions for searching stat data and
   searchig entry. It returns stat data key of the object symlink points to. */
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
	
	if ((res = sym40_read(entity, path, sizeof(path)) < 0))
		return res;

	return sym->obj.core->object_ops.resolve(sym->obj.info.tree,
						 STAT_PLACE(&sym->obj),
						 path, from, key);
}

/* Releases passed @entity */
static void sym40_close(object_entity_t *entity) {
	aal_assert("umka-1170", entity != NULL);

	aal_free(entity);
}

static reiser4_object_ops_t sym40_ops = {
#ifndef ENABLE_STAND_ALONE
	.create	        = sym40_create,
	.layout         = sym40_layout,
	.metadata       = sym40_metadata,
	.link           = sym40_link,
	.unlink         = sym40_unlink,
	.links          = sym40_links,
	.clobber        = sym40_clobber,
	.recognize	= sym40_recognize,
	.check_struct   = sym40_check_struct,

	.form		= NULL,
	.seek	        = NULL,
	.write	        = NULL,
	.convert        = NULL,
	.truncate       = NULL,
	.rem_entry      = NULL,
	.add_entry      = NULL,
	.attach         = NULL,
	.detach         = NULL,
	
	.fake		= NULL,
	.check_attach 	= NULL,
#endif
	.lookup	        = NULL,
	.reset	        = NULL,
	.offset	        = NULL,
	.size           = NULL,
	.readdir        = NULL,
	.telldir        = NULL,
	.seekdir        = NULL,

	.read	        = sym40_read,
	.open	        = sym40_open,
	.close	        = sym40_close,
	.follow         = sym40_follow
};

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
	score = c;
	return &sym40_plug;
}

plug_register(sym40, sym40_start, NULL);
#endif
