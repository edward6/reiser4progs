/*
  oid40.c -- reiser4 default oid allocator plugin. It operates on passed memory
  area inside of the loaded superblock.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef ENABLE_STAND_ALONE
#include "oid40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t oid40_plugin;

static int oid40_isdirty(object_entity_t *entity) {
	aal_assert("umka-2088", entity != NULL);
	return ((oid40_t *)entity)->dirty;
}

static void oid40_mkdirty(object_entity_t *entity) {
	aal_assert("umka-2089", entity != NULL);
	((oid40_t *)entity)->dirty = 1;
}

static void oid40_mkclean(object_entity_t *entity) {
	aal_assert("umka-2090", entity != NULL);
	((oid40_t *)entity)->dirty = 0;
}

/*
  Initializies oid allocator instance and loads its data (namely next oid, used
  oids, etc).
*/
static object_entity_t *oid40_open(void *start, 
				   uint32_t len) 
{
	oid40_t *oid;

	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;

	oid->dirty = 0;

	oid->len = len;
	oid->start = start;
    
	oid->plugin = &oid40_plugin;
	oid->next = oid40_get_next(start);
	oid->used = oid40_get_used(start);
    
	return (object_entity_t *)oid;
}

static void oid40_close(object_entity_t *entity) {
	aal_assert("umka-510", entity != NULL);
	aal_free(entity);
}

/* Initializes oid allocator instance and return it to the caller */
static object_entity_t *oid40_create(void *start, 
				     uint32_t len) 
{
	oid40_t *oid;

	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;

	oid->dirty = 1;
	oid->len = len;
	oid->start = start;

	/*
	  Setting up next by OID40_RESERVED. It is needed because all oid less
	  then OID40_RESERVED will be used for reiser4 insetrnal purposes.
	*/
	oid->next = OID40_RESERVED;
	oid->used = 0;
    
	oid->plugin = &oid40_plugin;
	oid40_set_next(start, oid->next);
	oid40_set_used(start, oid->used);
    
	return (object_entity_t *)oid;
}

/* Updating next and used values in oid allocator dedicated area */
static errno_t oid40_sync(object_entity_t *entity) {
	aal_assert("umka-1016", entity != NULL);
    
	oid40_set_next(((oid40_t *)entity)->start, 
		       ((oid40_t *)entity)->next);
    
	oid40_set_used(((oid40_t *)entity)->start, 
		       ((oid40_t *)entity)->used);

	return 0;
}

/* Returns next oid to be used */
static oid_t oid40_next(object_entity_t *entity) {
	aal_assert("umka-1109", entity != NULL);
	return ((oid40_t *)entity)->next;
}

/* Returns free oid and marks it as used */
static oid_t oid40_allocate(object_entity_t *entity) {
	aal_assert("umka-513", entity != NULL);

	((oid40_t *)entity)->next++;
	((oid40_t *)entity)->used++;
	((oid40_t *)entity)->dirty = 1;
	
	return ((oid40_t *)entity)->next - 1;
}

/* Releases passed oid */
static void oid40_release(object_entity_t *entity, 
			  oid_t oid)
{
	aal_assert("umka-528", entity != NULL);

	((oid40_t *)entity)->used--;
	((oid40_t *)entity)->dirty = 1;
}

/* Prints oid allocator data into passed @stream */
static errno_t oid40_print(object_entity_t *entity,
			   aal_stream_t *stream,
			   uint16_t options)
{
	aal_assert("umka-1303", entity != NULL);
	aal_assert("umka-1304", stream != NULL);

	aal_stream_format(stream, "Oid allocator:\n");
	
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  entity->plugin->h.label);

	aal_stream_format(stream, "next oid:\t0x%llx\n",
			  ((oid40_t *)entity)->next);

	aal_stream_format(stream, "used oids:\t%llu\n",
			  ((oid40_t *)entity)->used);
	return 0;
}

/* Returns number of free oids */
static oid_t oid40_free(object_entity_t *entity) {
	aal_assert("umka-961", entity != NULL);
	return ~0ull - ((oid40_t *)entity)->next;
}

/* Returns number of used oids */
static oid_t oid40_used(object_entity_t *entity) {
	aal_assert("umka-530", entity != NULL);
	return ((oid40_t *)entity)->used;
}

/* Checks oid allocator for validness */
static errno_t oid40_valid(object_entity_t *entity) {
	aal_assert("umka-966", entity != NULL);

	/*
	  Next oid should not be lesser than the root parent locality (so called
	  hyper locality).
	*/
	if (((oid40_t *)entity)->next < OID40_HYPER_LOCALITY)
		return -EINVAL;

	return 0;
}

/* Returns the root parent locality */
static oid_t oid40_hyper_locality(void) {
	return OID40_HYPER_LOCALITY;
}

/* Returns root locality */
static oid_t oid40_root_locality(void) {
	return OID40_ROOT_LOCALITY;
}

/* Returns root oid */
static oid_t oid40_root_objectid(void) {
	return OID40_ROOT_OBJECTID;
}

reiser4_oid_ops_t oid40_ops = {
	.open		= oid40_open,
	.close		= oid40_close,
	.create		= oid40_create,
	.valid		= oid40_valid,
	.next		= oid40_next,
	.allocate	= oid40_allocate,
	.release	= oid40_release,
	.sync		= oid40_sync,
	.isdirty        = oid40_isdirty,
	.mkdirty        = oid40_mkdirty,
	.mkclean        = oid40_mkclean,
	.print		= oid40_print,
	.used		= oid40_used,
	.free		= oid40_free,
	.layout         = NULL,

	.root_locality	= oid40_root_locality,
	.root_objectid	= oid40_root_objectid,
	.hyper_locality	= oid40_hyper_locality
};

static reiser4_plugin_t oid40_plugin = {
	.h = {
		.class = CLASS_INIT,
		.id = OID_REISER40_ID,
		.group = 0,
		.type = OID_PLUGIN_TYPE,
		.label = "oid40",
		.desc = "Inode allocator for reiser4, ver. " VERSION
	},
	.o = {
		.oid_ops = &oid40_ops
	}
};

static reiser4_plugin_t *oid40_start(reiser4_core_t *c) {
	core = c;
	return &oid40_plugin;
}

plugin_register(oid40, oid40_start, NULL);
#endif
