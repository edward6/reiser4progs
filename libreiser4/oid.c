/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid.c -- oid allocator common code. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/libreiser4.h>

bool_t reiser4_oid_isdirty(reiser4_oid_t *oid) {
	uint32_t state;
	
	aal_assert("umka-2660", oid != NULL);

	state = plug_call(oid->entity->plug->o.oid_ops,
			  get_state, oid->entity);
	
	return (state & (1 << ENTITY_DIRTY));
}

void reiser4_oid_mkdirty(reiser4_oid_t *oid) {
	uint32_t state;
	
	aal_assert("umka-2659", oid != NULL);

	state = plug_call(oid->entity->plug->o.oid_ops,
			  get_state, oid->entity);

	state |= (1 << ENTITY_DIRTY);
	
	plug_call(oid->entity->plug->o.oid_ops,
		  set_state, oid->entity, state);
}

void reiser4_oid_mkclean(reiser4_oid_t *oid) {
	uint32_t state;
	
	aal_assert("umka-2658", oid != NULL);

	state = plug_call(oid->entity->plug->o.oid_ops,
			  get_state, oid->entity);

	state &= ~(1 << ENTITY_DIRTY);
	
	plug_call(oid->entity->plug->o.oid_ops,
		  set_state, oid->entity, state);
}

/* Opens object allocator using start and end pointers */
reiser4_oid_t *reiser4_oid_open(
	reiser4_fs_t *fs)	    /* fs oid will be opened on */
{
	rid_t pid;
	reiser4_oid_t *oid;
	reiser4_plug_t *plug;

	aal_assert("umka-1698", fs != NULL);
	aal_assert("umka-519", fs->format != NULL);

	/* Allocating memory needed for instance */
	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;

	oid->fs = fs;
	oid->fs->oid = oid;
	
	if ((pid = reiser4_format_oid_pid(fs->format)) == INVAL_PID) {
		aal_error("Invalid oid allocator plugin id "
			  "has been detected.");
		goto error_free_oid;
	}
    
	/* Getting oid allocator plugin */
	if (!(plug = reiser4_factory_ifind(OID_PLUG_TYPE, pid))) {
		aal_error("Can't find oid allocator plugin by "
			  "its id 0x%x.", pid);
		goto error_free_oid;
	}
    
	/* Initializing oid allocator entity. */
	if (!(oid->entity = plug_call(plug->o.oid_ops, open,
				      fs->format->entity))) 
	{
		aal_error("Can't open oid allocator %s.",
			  plug->label);
		goto error_free_oid;
	}

	return oid;
    
 error_free_oid:
	aal_free(oid);
	return NULL;
}

/* Closes oid allocator */
void reiser4_oid_close(
	reiser4_oid_t *oid)		/* oid allocator instance to be closed */
{
	aal_assert("umka-1507", oid != NULL);
	
	oid->fs->oid = NULL;
	
	plug_call(oid->entity->plug->o.oid_ops, 
		  close, oid->entity);
    
	aal_free(oid);
}

/* Creates oid allocator in specified area */
reiser4_oid_t *reiser4_oid_create(
	reiser4_fs_t *fs)	/* fs oid allocator will be oned on */
{
	rid_t pid;
	reiser4_oid_t *oid;
	reiser4_plug_t *plug;
	
	aal_assert("umka-729", fs != NULL);
	aal_assert("umka-1699", fs->format != NULL);

	/* Initializing instance */
	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;
   
	oid->fs = fs;
	oid->fs->oid = oid;
	
	if ((pid = reiser4_format_oid_pid(fs->format)) == INVAL_PID) {
		aal_error("Invalid oid allocator plugin id "
			  "has been detected.");
		goto error_free_oid;
	}
    
	/* Getting plugin from plugin id */
	if (!(plug = reiser4_factory_ifind(OID_PLUG_TYPE, pid))) {
		aal_error("Can't find oid allocator plugin "
			  "by its id 0x%x.", pid);
		goto error_free_oid;
	}
    
	/* Initializing oid allocator entity. */
	if (!(oid->entity = plug_call(plug->o.oid_ops, create,
				      fs->format->entity)))
	{
		aal_error("Can't create oid allocator %s.", 
			  plug->label);
		goto error_free_oid;
	}

	return oid;
    
 error_free_oid:
	aal_free(oid);
	return NULL;
}

/* Oid allocator enumerator */
errno_t reiser4_oid_layout(reiser4_oid_t *oid,
			   region_func_t region_func,
			   void *data)
{
	aal_assert("umka-2198", oid != NULL);

	if (!oid->entity->plug->o.oid_ops->layout)
		return 0;

	return plug_call(oid->entity->plug->o.oid_ops,
			 layout, oid->entity, region_func, data);
}

/* Returns next object id from specified oid allocator */
oid_t reiser4_oid_lost_objectid(reiser4_oid_t *oid) {
	aal_assert("umka-1108", oid != NULL);
    
	return plug_call(oid->entity->plug->o.oid_ops, 
			 lost_objectid, oid->entity);
}

/* Returns free object id from specified oid allocator */
oid_t reiser4_oid_allocate(reiser4_oid_t *oid) {
	aal_assert("umka-522", oid != NULL);
    
	return plug_call(oid->entity->plug->o.oid_ops, 
			 allocate, oid->entity);
}

/* Releases passed objectid */
void reiser4_oid_release(
	reiser4_oid_t *oid,	/* oid allocator to be used */
	oid_t id)		/* object id to be released */
{
	aal_assert("umka-525", oid != NULL);
    
	plug_call(oid->entity->plug->o.oid_ops, 
		  release, oid->entity, id);
}

/* Synchronizes specified oid allocator */
errno_t reiser4_oid_sync(reiser4_oid_t *oid) {
	aal_assert("umka-735", oid != NULL);
	
	if (!reiser4_oid_isdirty(oid))
		return 0;

	return plug_call(oid->entity->plug->o.oid_ops, 
			 sync, oid->entity);
}

/* Returns number of used oids from passed oid allocator */
uint64_t reiser4_oid_used(reiser4_oid_t *oid) {
	aal_assert("umka-527", oid != NULL);
    
	return plug_call(oid->entity->plug->o.oid_ops, 
			 get_used, oid->entity);
}

/* Returns number of free oids from passed oid allocator */
uint64_t reiser4_oid_free(reiser4_oid_t *oid) {
	aal_assert("umka-527", oid != NULL);
    
	return plug_call(oid->entity->plug->o.oid_ops, 
			 free, oid->entity);
}

/* Returns the first not used oid from passed oid allocator */
uint64_t reiser4_oid_next(reiser4_oid_t *oid) {
	aal_assert("umka-527", oid != NULL);
    
	return plug_call(oid->entity->plug->o.oid_ops, 
			 get_next, oid->entity);
}

/* Checks specified oid allocator for validness */
errno_t reiser4_oid_valid(reiser4_oid_t *oid) {
	aal_assert("umka-962", oid != NULL);
    
	return plug_call(oid->entity->plug->o.oid_ops, 
			 valid, oid->entity);
}

/* Returns root parent objectid from specified oid allocator */
oid_t reiser4_oid_root_locality(reiser4_oid_t *oid) {
	aal_assert("umka-746", oid != NULL);
    
	return plug_call(oid->entity->plug->o.oid_ops, 
			 root_locality, oid->entity);
}

/* Returns root objectid from specified oid allocator */
oid_t reiser4_oid_root_objectid(reiser4_oid_t *oid) {
	aal_assert("umka-747", oid != NULL);
    
	return plug_call(oid->entity->plug->o.oid_ops, 
			 root_objectid, oid->entity);
}
#endif
