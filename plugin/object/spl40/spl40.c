/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   spl40.c -- reiser4 special files plugin. */

#ifdef ENABLE_SPECIAL
#include "spl40.h"
#include "spl40_repair.h"

reiser4_core_t *spl40_core = NULL;

/* Opens special file and returns initialized instance to the caller */
object_entity_t *spl40_open(object_info_t *info) {
	spl40_t *spl;

	aal_assert("umka-2529", info != NULL);
	aal_assert("umka-2530", info->tree != NULL);
 	
	if (info->start.plug->id.group != STAT_ITEM)
		return NULL;
  
	if (info->opset.plug[OPSET_OBJ] != &spl40_plug)
		return NULL;
	
	if (!(spl = aal_calloc(sizeof(*spl), 0)))
		return NULL;

	/* Initalizing file handle. */
	obj40_init(&spl->obj, info, spl40_core);
	
	/* Initializing statdata place. */
	aal_memcpy(STAT_PLACE(&spl->obj), &info->start,
		   sizeof(info->start));
	
	return (object_entity_t *)spl;
}

/* Loads special file stat data to passed @hint */
static errno_t spl40_stat(object_entity_t *entity, stat_hint_t *hint) {
	spl40_t *spl;
	
	aal_assert("umka-2551", entity != NULL);
	aal_assert("umka-2552", hint != NULL);

	spl = (spl40_t *)entity;
	return obj40_load_stat(&spl->obj, hint);
}

#ifndef ENABLE_STAND_ALONE
/* Creates special file and returns initialized instance to the caller. */
static object_entity_t *spl40_create(object_hint_t *hint) {
	spl40_t *spl;
	
	aal_assert("umka-2533", hint != NULL);
	aal_assert("umka-2532", hint->info.tree != NULL);

	if (!(spl = aal_calloc(sizeof(*spl), 0)))
		return NULL;
	
	/* Inizializes file handle */
	obj40_init(&spl->obj, &hint->info, spl40_core);

	if (obj40_create_stat(&spl->obj, 0, 0, hint->rdev, 0, hint->mode, NULL))
		goto error_free_spl;

	return (object_entity_t *)spl;

 error_free_spl:
	aal_free(spl);
	return NULL;
}

/* Removes stat data of the passed @entity. */
static errno_t spl40_clobber(object_entity_t *entity) {
	aal_assert("umka-2536", entity != NULL);
	return obj40_clobber(&((spl40_t *)entity)->obj);
}

/* Return number of hard links. */
static uint32_t spl40_links(object_entity_t *entity) {
	spl40_t *spl;
	
	aal_assert("umka-2537", entity != NULL);

	spl = (spl40_t *)entity;
	return obj40_links(&spl->obj);
}

/* Adds one more hardlink */
static errno_t spl40_link(object_entity_t *entity) {
	spl40_t *spl;
	
	aal_assert("umka-2538", entity != NULL);

	spl = (spl40_t *)entity;
	return obj40_link(&spl->obj);
}

/* Removes hardlink. */
static errno_t spl40_unlink(object_entity_t *entity) {
	spl40_t *spl;
	
	aal_assert("umka-2539", entity != NULL);

	spl = (spl40_t *)entity;
	return obj40_unlink(&spl->obj);
}

/* Calls function @func for each symlink item (statdata only) */
static errno_t spl40_metadata(object_entity_t *entity,
			      place_func_t place_func,
			      void *data)
{
	spl40_t *spl;

	aal_assert("umka-2540", entity != NULL);
	aal_assert("umka-2541", place_func != NULL);

	spl = (spl40_t *)entity;
	return obj40_metadata(&spl->obj, place_func, data);
}

/* Updates special file stat data from passed @hint */
static errno_t spl40_update(object_entity_t *entity, stat_hint_t *hint) {
	spl40_t *spl;
	
	aal_assert("umka-2555", entity != NULL);
	aal_assert("umka-2556", hint != NULL);

	spl = (spl40_t *)entity;
	return obj40_save_stat(&spl->obj, hint);
}
#endif

/* Releases passed @entity */
static void spl40_close(object_entity_t *entity) {
	aal_assert("umka-2544", entity != NULL);
	aal_free(entity);
}

static reiser4_object_ops_t spl40_ops = {
#ifndef ENABLE_STAND_ALONE
	.create	        = spl40_create,
	.metadata       = spl40_metadata,
	.link           = spl40_link,
	.unlink         = spl40_unlink,
	.links          = spl40_links,
	.clobber        = spl40_clobber,
	.update         = spl40_update,
	.check_struct	= spl40_check_struct,
	.recognize	= spl40_recognize,

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
	.read	        = NULL,
	.follow         = NULL,
	
	.stat           = spl40_stat,
	.open	        = spl40_open,
	.close	        = spl40_close
};

reiser4_plug_t spl40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_SPL40_ID, SPL_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "spl40",
	.desc  = "Special file plugin for reiser4, ver. " VERSION,
#endif
	.o = {
		.object_ops = &spl40_ops
	}
};

static reiser4_plug_t *spl40_start(reiser4_core_t *c) {
	spl40_core = c;
	return &spl40_plug;
}

plug_register(spl40, spl40_start, NULL);
#endif
