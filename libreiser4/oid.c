/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   oid.c -- oid allocator common code. */

#ifndef ENABLE_MINIMAL
#include <reiser4/libreiser4.h>

bool_t reiser4_oid_isdirty(reiser4_oid_t *oid) {
	uint32_t state;
	
	aal_assert("umka-2660", oid != NULL);

	state = reiser4call(oid, get_state);
	return (state & (1 << ENTITY_DIRTY));
}

void reiser4_oid_mkdirty(reiser4_oid_t *oid) {
	uint32_t state;
	
	aal_assert("umka-2659", oid != NULL);

	state = reiser4call(oid, get_state);
	state |= (1 << ENTITY_DIRTY);
	reiser4call(oid, set_state, state);
}

void reiser4_oid_mkclean(reiser4_oid_t *oid) {
	uint32_t state;
	
	aal_assert("umka-2658", oid != NULL);

	state = reiser4call(oid, get_state);
	state &= ~(1 << ENTITY_DIRTY);
	reiser4call(oid, set_state, state);
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
	if (!(oid->ent = plugcall((reiser4_oid_plug_t *)plug, 
				  open, fs->format->ent))) 
	{
		aal_error("Can't open oid allocator %s.", plug->label);
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
	reiser4call(oid, close);
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
	if (!(oid->ent = plugcall((reiser4_oid_plug_t *)plug, 
				  create, fs->format->ent)))
	{
		aal_error("Can't create oid allocator %s.", plug->label);
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

	if (!oid->ent->plug->layout)
		return 0;

	return reiser4call(oid, layout, region_func, data);
}

/* Returns next object id from specified oid allocator */
oid_t reiser4_oid_lost_objectid(reiser4_oid_t *oid) {
	aal_assert("umka-1108", oid != NULL);
    
	return reiser4call(oid, lost_objectid);
}

/* Returns free object id from specified oid allocator */
oid_t reiser4_oid_allocate(reiser4_oid_t *oid) {
	aal_assert("umka-522", oid != NULL);
    
	return reiser4call(oid, allocate);
}

/* Releases passed objectid */
void reiser4_oid_release(
	reiser4_oid_t *oid,	/* oid allocator to be used */
	oid_t id)		/* object id to be released */
{
	aal_assert("umka-525", oid != NULL);
    
	reiser4call(oid, release, id);
}

/* Synchronizes specified oid allocator */
errno_t reiser4_oid_sync(reiser4_oid_t *oid) {
	aal_assert("umka-735", oid != NULL);
	
	if (!reiser4_oid_isdirty(oid))
		return 0;

	return reiser4call(oid, sync);
}

/* Returns number of used oids from passed oid allocator */
uint64_t reiser4_oid_get_used(reiser4_oid_t *oid) {
	aal_assert("umka-527", oid != NULL);
    
	return reiser4call(oid, get_used);
}

/* Returns number of used oids from passed oid allocator */
void reiser4_oid_set_used(reiser4_oid_t *oid, uint64_t used) {
	aal_assert("vpf-1798", oid != NULL);
    
	reiser4call(oid, set_used, used);
}


/* Returns number of free oids from passed oid allocator */
uint64_t reiser4_oid_free(reiser4_oid_t *oid) {
	aal_assert("umka-527", oid != NULL);
    
	return reiser4call(oid, free);
}

/* Returns the first not used oid from passed oid allocator */
uint64_t reiser4_oid_next(reiser4_oid_t *oid) {
	aal_assert("umka-527", oid != NULL);
    
	return reiser4call(oid, get_next);
}

/* Checks specified oid allocator for validness */
errno_t reiser4_oid_valid(reiser4_oid_t *oid) {
	aal_assert("umka-962", oid != NULL);
    
	return reiser4call(oid, valid);
}

/* Returns root parent objectid from specified oid allocator */
oid_t reiser4_oid_root_locality(reiser4_oid_t *oid) {
	aal_assert("umka-746", oid != NULL);
    
	return reiser4call(oid, root_locality);
}

/* Returns root objectid from specified oid allocator */
oid_t reiser4_oid_root_objectid(reiser4_oid_t *oid) {
	aal_assert("umka-747", oid != NULL);
    
	return reiser4call(oid, root_objectid, oid->ent);
}
#endif
