/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid40_repair.c -- reiser4 oid allocator plugin. */

#ifndef ENABLE_MINIMAL

#include "oid40.h"
#include "oid40_repair.h"

static uint32_t oid40_get_state(reiser4_oid_ent_t *entity) {
	aal_assert("umka-2088", entity != NULL);
	return PLUG_ENT(entity)->state;
}

/* Returnes next oid to be used */

static uint64_t oid40_get_next_oid(reiser4_oid_ent_t *entity) {
	aal_assert("umka-1109", entity != NULL);
	return PLUG_ENT(entity)->next;
}

/* Returns number of used oids */
static uint64_t oid40_get_used_oid(reiser4_oid_ent_t *entity) {
	aal_assert("umka-530", entity != NULL);
	return PLUG_ENT(entity)->used;
}

static void oid40_set_state(reiser4_oid_ent_t *entity,
			    uint32_t state)
{
	aal_assert("umka-2089", entity != NULL);
	PLUG_ENT(entity)->state = state;
}

/* Updates next oid to be used */

static void oid40_set_next_oid(reiser4_oid_ent_t *entity, uint64_t next) {
	aal_assert("vpf-1588", entity != NULL);
	PLUG_ENT(entity)->next = next;
	PLUG_ENT(entity)->state = (1 << ENTITY_DIRTY);
}

/* Updates number of used oids */
static void oid40_set_used_oid(reiser4_oid_ent_t *entity, uint64_t used) {
	aal_assert("umka-530", entity != NULL);
	PLUG_ENT(entity)->used = used;
	PLUG_ENT(entity)->state = (1 << ENTITY_DIRTY);
}


/* Open oid allocator on passed format instance. */
static reiser4_oid_ent_t *oid40_open(reiser4_format_ent_t *format) {
	oid40_t *oid;

	aal_assert("umka-2664", format != NULL);
	
	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;

	oid->state = 0;
	oid->format = format;
	oid->plug = &oid40_plug;

	/* Getting oid pluign work area from format plugin. */
	entcall(format, oid_area, &oid->start, &oid->len);
    
	oid->next = oid40_get_next(oid->start);
	oid->used = oid40_get_used(oid->start);
	
	return (reiser4_oid_ent_t *)oid;
}

static void oid40_close(reiser4_oid_ent_t *entity) {
	aal_assert("umka-510", entity != NULL);
	aal_free(entity);
}

/* Initializes oid allocator instance and returns it to caller. */
static reiser4_oid_ent_t *oid40_create(reiser4_format_ent_t *format) {
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
	entcall(format, oid_area, &oid->start, &oid->len);
	
	oid40_set_next(oid->start, oid->next);
	oid40_set_used(oid->start, oid->used);

	return (reiser4_oid_ent_t *)oid;
}

/* Updating next and used values in oid allocator dedicated area */
static errno_t oid40_sync(reiser4_oid_ent_t *entity) {
	reiser4_format_ent_t *format;
	uint32_t state;
	
	aal_assert("umka-1016", entity != NULL);

	oid40_set_next(PLUG_ENT(entity)->start, 
		       PLUG_ENT(entity)->next);
    
	oid40_set_used(PLUG_ENT(entity)->start, 
		       PLUG_ENT(entity)->used);

	/* Mark the format dirty. */
	format = PLUG_ENT(entity)->format;
	state = entcall(format, get_state);
	entcall(format, set_state, state | (1 << ENTITY_DIRTY));
	
	return 0;
}

/* Returns free oid and marks it as used */
static oid_t oid40_allocate(reiser4_oid_ent_t *entity) {
	aal_assert("umka-513", entity != NULL);

	PLUG_ENT(entity)->next++;
	PLUG_ENT(entity)->used++;

	PLUG_ENT(entity)->state |= (1 << ENTITY_DIRTY);
	return PLUG_ENT(entity)->next - 1;
}

/* Releases passed oid */
static void oid40_release(reiser4_oid_ent_t *entity, 
			  oid_t oid)
{
	aal_assert("umka-528", entity != NULL);

	PLUG_ENT(entity)->used--;
	PLUG_ENT(entity)->state |= (1 << ENTITY_DIRTY);
}

/* Returns number of free oids */
static oid_t oid40_free(reiser4_oid_ent_t *entity) {
	aal_assert("umka-961", entity != NULL);
	return MAX_UINT64 - PLUG_ENT(entity)->next;
}

/* Checks oid allocator for validness */
static errno_t oid40_valid(reiser4_oid_ent_t *entity) {
	aal_assert("umka-966", entity != NULL);

	/* Next oid should not be less than the root locality */
	if (PLUG_ENT(entity)->next < OID40_ROOT_LOCALITY)
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

reiser4_oid_plug_t oid40_plug = {
	.p = {
		.id    = {OID_REISER40_ID, 0, OID_PLUG_TYPE},
		.label = "oid40",
		.desc  = "Inode number allocator plugin.",
	},
	
	.open		= oid40_open,
	.close		= oid40_close,
	.create		= oid40_create,
	.valid		= oid40_valid,
	.allocate	= oid40_allocate,
	.release	= oid40_release,
	.sync		= oid40_sync,
	.print		= oid40_print,
	.free		= oid40_free,
	.layout         = NULL,

	.set_state      = oid40_set_state,
	.set_next	= oid40_set_next_oid,
	.set_used	= oid40_set_used_oid,
	.get_state      = oid40_get_state,
	.get_next	= oid40_get_next_oid,
	.get_used	= oid40_get_used_oid,
	
	.root_locality	= oid40_root_locality,
	.root_objectid	= oid40_root_objectid,
	.lost_objectid	= oid40_lost_objectid,
	.slink_locality = oid40_slink_locality
};

#endif
