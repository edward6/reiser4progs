/*
  oid40.c -- reiser4 default oid allocator plugin. It operates on passed memory
  area inside of the loaded superblock.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "oid40.h"

static reiser4_core_t *core = NULL;
extern reiser4_plugin_t oid40_plugin;

/*
  Initializies oid allocator instance and loads its data (namely next oid, used
  oids, etc).
*/
static object_entity_t *oid40_open(const void *start, 
				   uint32_t len) 
{
	oid40_t *oid;

	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;

	oid->start = start;
	oid->len = len;
    
	oid->next = oid40_get_next(start);
	oid->used = oid40_get_used(start);
	oid->plugin = &oid40_plugin;
    
	return (object_entity_t *)oid;
}

static void oid40_close(object_entity_t *entity) {
	aal_assert("umka-510", entity != NULL);
	aal_free(entity);
}

#ifndef ENABLE_COMPACT

/* Initializes oid allocator instance and return it to the caller */
static object_entity_t *oid40_create(const void *start, 
				     uint32_t len) 
{
	oid40_t *oid;

	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;

	oid->start = start;
	oid->len = len;

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
static roid_t oid40_next(object_entity_t *entity) {
	aal_assert("umka-1109", entity != NULL);
	return ((oid40_t *)entity)->next;
}

/* Returns free oid and marks it as used */
static roid_t oid40_allocate(object_entity_t *entity) {
	aal_assert("umka-513", entity != NULL);

	((oid40_t *)entity)->next++;
	((oid40_t *)entity)->used++;
    
	return ((oid40_t *)entity)->next - 1;
}

/* Releases passed oid */
static void oid40_release(object_entity_t *entity, 
			  roid_t oid)
{
	aal_assert("umka-528", entity != NULL);
	((oid40_t *)entity)->used--;
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

	aal_stream_format(stream, "used oids:\t0x%llx\n",
			  ((oid40_t *)entity)->used);
	return 0;
}

#endif

/* Checks oid allocator for validness */
static errno_t oid40_valid(object_entity_t *entity) {
	aal_assert("umka-966", entity != NULL);

	/* Next oid should not be lesser than the root parent locality */
	if (((oid40_t *)entity)->next < OID40_HYPER_LOCALITY)
		return -1;

	return 0;
}

/* Returns number of free oids */
static roid_t oid40_free(object_entity_t *entity) {
	aal_assert("umka-961", entity != NULL);
	return ~0ull - ((oid40_t *)entity)->next;
}

/* Returns number of used oids */
static roid_t oid40_used(object_entity_t *entity) {
	aal_assert("umka-530", entity != NULL);
	return ((oid40_t *)entity)->used;
}

/* Returns the root parent locality */
static roid_t oid40_hyper_locality(void) {
	return OID40_HYPER_LOCALITY;
}

/* Returns root locality */
static roid_t oid40_root_locality(void) {
	return OID40_ROOT_LOCALITY;
}

/* Returns root oid */
static roid_t oid40_root_objectid(void) {
	return OID40_ROOT_OBJECTID;
}

/* Prepare oid40 plugin */
static reiser4_plugin_t oid40_plugin = {
	.oid_ops = {
		.h = {
			.handle = empty_handle,
			.id = OID_REISER40_ID,
			.group = 0,
			.type = OID_PLUGIN_TYPE,
			.label = "oid40",
			.desc = "Inode allocator for reiserfs 4.0, ver. " VERSION,
		},
		.open		= oid40_open,
		.close		= oid40_close,
		.valid		= oid40_valid,
		
#ifndef ENABLE_COMPACT	
		.create		= oid40_create,
		.next		= oid40_next,
		.allocate	= oid40_allocate,
		.release	= oid40_release,
		.sync		= oid40_sync,
		.print		= oid40_print,
#else
		.create		= NULL,
		.next		= NULL,
		.allocate	= NULL,
		.release	= NULL,
		.sync		= NULL,
		.print		= NULL,
#endif
		.used		= oid40_used,
		.free		= oid40_free,
	
		.root_locality	= oid40_root_locality,
		.root_objectid	= oid40_root_objectid,
		.hyper_locality	= oid40_hyper_locality
	}
};

static reiser4_plugin_t *oid40_start(reiser4_core_t *c) {
	core = c;
	return &oid40_plugin;
}

plugin_register(oid40_start, NULL);

