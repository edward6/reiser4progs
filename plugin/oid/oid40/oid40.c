/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid40_repair.c -- reiser4 oid allocator plugin. */

#ifndef ENABLE_STAND_ALONE

#include "oid40.h"
#include "oid40_repair.h"

extern reiser4_plug_t oid40_plug;

static uint32_t oid40_get_state(generic_entity_t *entity) {
	aal_assert("umka-2088", entity != NULL);
	return ((oid40_t *)entity)->state;
}

static void oid40_set_state(generic_entity_t *entity,
			    uint32_t state)
{
	aal_assert("umka-2089", entity != NULL);
	((oid40_t *)entity)->state = state;
}

/* Open oid allocator on passed format instance. */
static generic_entity_t *oid40_open(generic_entity_t *format) {
	oid40_t *oid;

	aal_assert("umka-2664", format != NULL);
	
	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;

	oid->state = 0;
	oid->format = format;
	oid->plug = &oid40_plug;

	/* Getting oid pluign work area from format plugin. */
	plug_call(format->plug->o.format_ops, oid_area,
		  format, &oid->start, &oid->len);
    
	oid->next = oid40_get_next(oid->start);
	oid->used = oid40_get_used(oid->start);
	
	return (generic_entity_t *)oid;
}

static void oid40_close(generic_entity_t *entity) {
	aal_assert("umka-510", entity != NULL);
	aal_free(entity);
}

/* Initializes oid allocator instance and returns it to caller. */
static generic_entity_t *oid40_create(generic_entity_t *format) {
	oid40_t *oid;

	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;

	oid->state = (1 << ENTITY_DIRTY);

	/* Setting up next by OID40_RESERVED. It is needed because all oid less
	   then OID40_RESERVED will be used for reiser4 insetrnal purposes. */
	oid->used = 0;
	oid->format = format;
	oid->plug = &oid40_plug;
	oid->next = OID40_RESERVED;

	/* Getting oid pluign work area from format plugin. */
	plug_call(format->plug->o.format_ops, oid_area,
		  format, &oid->start, &oid->len);
	
	oid40_set_next(oid->start, oid->next);
	oid40_set_used(oid->start, oid->used);

	return (generic_entity_t *)oid;
}

/* Updating next and used values in oid allocator dedicated area */
static errno_t oid40_sync(generic_entity_t *entity) {
	generic_entity_t *format;
	uint32_t state;
	
	aal_assert("umka-1016", entity != NULL);

	oid40_set_next(((oid40_t *)entity)->start, 
		       ((oid40_t *)entity)->next);
    
	oid40_set_used(((oid40_t *)entity)->start, 
		       ((oid40_t *)entity)->used);

	/* Mark the format dirty. */
	format = ((oid40_t *)entity)->format;
	
	state = plug_call(format->plug->o.format_ops,
			  get_state, format);
	
	plug_call(format->plug->o.format_ops, set_state, 
		  format, state | (1 << ENTITY_DIRTY));
	
	return 0;
}

/* Returns next oid to be used */
static oid_t oid40_next(generic_entity_t *entity) {
	aal_assert("umka-1109", entity != NULL);
	return ((oid40_t *)entity)->next;
}

/* Returns free oid and marks it as used */
static oid_t oid40_allocate(generic_entity_t *entity) {
	aal_assert("umka-513", entity != NULL);

	((oid40_t *)entity)->next++;
	((oid40_t *)entity)->used++;

	((oid40_t *)entity)->state |= (1 << ENTITY_DIRTY);
	return ((oid40_t *)entity)->next - 1;
}

/* Releases passed oid */
static void oid40_release(generic_entity_t *entity, 
			  oid_t oid)
{
	aal_assert("umka-528", entity != NULL);

	((oid40_t *)entity)->used--;
	((oid40_t *)entity)->state |= (1 << ENTITY_DIRTY);
}

/* Returns number of free oids */
static oid_t oid40_free(generic_entity_t *entity) {
	aal_assert("umka-961", entity != NULL);
	return MAX_UINT64 - ((oid40_t *)entity)->next;
}

/* Returns number of used oids */
static oid_t oid40_used(generic_entity_t *entity) {
	aal_assert("umka-530", entity != NULL);
	return ((oid40_t *)entity)->used;
}

/* Checks oid allocator for validness */
static errno_t oid40_valid(generic_entity_t *entity) {
	aal_assert("umka-966", entity != NULL);

	/* Next oid should not be less than the root locality */
	if (((oid40_t *)entity)->next < OID40_ROOT_LOCALITY)
		return -EINVAL;

	return 0;
}

/* Returns root locality */
static oid_t oid40_root_locality() {
	return OID40_ROOT_LOCALITY;
}

/* Returns root oid */
static oid_t oid40_root_objectid() {
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
	.print		= oid40_print,
	.used		= oid40_used,
	.free		= oid40_free,
	.layout         = NULL,

	.set_state      = oid40_set_state,
	.get_state      = oid40_get_state,
	.root_locality	= oid40_root_locality,
	.root_objectid	= oid40_root_objectid,
	.lost_objectid	= oid40_lost_objectid,
	.slink_locality  = oid40_slink_locality
};

static reiser4_plug_t oid40_plug = {
	.cl    = class_init,
	.id    = {OID_REISER40_ID, 0, OID_PLUG_TYPE},
	.label = "oid40",
	.desc  = "Inode allocator for reiser4, ver. " VERSION,
	.o = {
		.oid_ops = &oid40_ops
	}
};

static reiser4_plug_t *oid40_start(reiser4_core_t *c) {
	return &oid40_plug;
}

plug_register(oid40, oid40_start, NULL);
#endif
