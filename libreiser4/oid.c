/*
  oid.c -- oid allocator common code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
bool_t reiser4_oid_isdirty(reiser4_oid_t *oid) {
	aal_assert("umka-2103", oid != NULL);

	return plugin_call(oid->entity->plugin->o.oid_ops,
			   isdirty, oid->entity);
}

void reiser4_oid_mkdirty(reiser4_oid_t *oid) {
	aal_assert("umka-2104", oid != NULL);

	plugin_call(oid->entity->plugin->o.oid_ops,
		    mkdirty, oid->entity);
}

void reiser4_oid_mkclean(reiser4_oid_t *oid) {
	aal_assert("umka-2105", oid != NULL);

	plugin_call(oid->entity->plugin->o.oid_ops,
		    mkclean, oid->entity);
}

/* Opens object allocator using start and end pointers */
reiser4_oid_t *reiser4_oid_open(
	reiser4_fs_t *fs)	    /* fs oid will be opened on */
{
	rid_t pid;
	reiser4_oid_t *oid;
	reiser4_plugin_t *plugin;

	void *oid_start;
	uint32_t oid_len;

	aal_assert("umka-1698", fs != NULL);
	aal_assert("umka-519", fs->format != NULL);

	/* Allocating memory needed for instance */
	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;

	oid->fs = fs;
	oid->fs->oid = oid;
	
	if ((pid = reiser4_format_oid_pid(fs->format)) == INVAL_PID) {
		aal_exception_error("Invalid oid allocator plugin id "
				    "has been detected.");
		goto error_free_oid;
	}
    
	/* Getting oid allocator plugin */
	if (!(plugin = libreiser4_factory_ifind(OID_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find oid allocator plugin by "
				    "its id 0x%x.", pid);
		goto error_free_oid;
	}
    
	plugin_call(fs->format->entity->plugin->o.format_ops, 
		    oid, fs->format->entity, &oid_start, &oid_len);
    
	/* Initializing oid allocator entity */
	if (!(oid->entity = plugin_call(plugin->o.oid_ops, open,
					oid_start, oid_len))) 
	{
		aal_exception_error("Can't open oid allocator %s.",
				    plugin->h.label);
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
	
	plugin_call(oid->entity->plugin->o.oid_ops, 
		    close, oid->entity);
    
	aal_free(oid);
}

/* Creates oid allocator in specified area */
reiser4_oid_t *reiser4_oid_create(
	reiser4_fs_t *fs)	/* fs oid allocator will be oned on */
{
	rid_t pid;
	reiser4_oid_t *oid;
	reiser4_plugin_t *plugin;

	void *oid_start;
	uint32_t oid_len;
	
	aal_assert("umka-729", fs != NULL);
	aal_assert("umka-1699", fs->format != NULL);

	/* Initializing instance */
	if (!(oid = aal_calloc(sizeof(*oid), 0)))
		return NULL;
   
	oid->fs = fs;
	oid->fs->oid = oid;
	
	if ((pid = reiser4_format_oid_pid(fs->format)) == INVAL_PID) {
		aal_exception_error("Invalid oid allocator plugin id "
				    "has been detected.");
		goto error_free_oid;
	}
    
	/* Getting plugin from plugin id */
	if (!(plugin = libreiser4_factory_ifind(OID_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find oid allocator plugin "
				    "by its id 0x%x.", pid);
		goto error_free_oid;
	}
    
	plugin_call(fs->format->entity->plugin->o.format_ops, 
		    oid, fs->format->entity, &oid_start, &oid_len);
    
	/* Initializing entity */
	if (!(oid->entity = plugin_call(plugin->o.oid_ops, create,
					oid_start, oid_len)))
	{
		aal_exception_error("Can't create oid allocator %s.", 
				    plugin->h.label);
		goto error_free_oid;
	}

	return oid;
    
 error_free_oid:
	aal_free(oid);
	return NULL;
}

/* Oid allocator enumerator */
errno_t reiser4_oid_layout(reiser4_oid_t *oid,
			   block_func_t block_func,
			   void *data)
{
	aal_assert("umka-2198", oid != NULL);

	if (!oid->entity->plugin->o.oid_ops->layout)
		return 0;

	return oid->entity->plugin->o.oid_ops->layout(oid->entity,
						    block_func, data);
}

/* Returns next object id from specified oid allocator */
oid_t reiser4_oid_next(reiser4_oid_t *oid) {
	aal_assert("umka-1108", oid != NULL);
    
	return plugin_call(oid->entity->plugin->o.oid_ops, 
			   next, oid->entity);
}

/* Returns free object id from specified oid allocator */
oid_t reiser4_oid_allocate(reiser4_oid_t *oid) {
	aal_assert("umka-522", oid != NULL);
    
	return plugin_call(oid->entity->plugin->o.oid_ops, 
			   allocate, oid->entity);
}

/* Releases passed objectid */
void reiser4_oid_release(
	reiser4_oid_t *oid,	/* oid allocator to be used */
	oid_t id)		/* object id to be released */
{
	aal_assert("umka-525", oid != NULL);
    
	plugin_call(oid->entity->plugin->o.oid_ops, 
		    release, oid->entity, id);
}

/* Synchronizes specified oid allocator */
errno_t reiser4_oid_sync(reiser4_oid_t *oid) {
	aal_assert("umka-735", oid != NULL);

	return plugin_call(oid->entity->plugin->o.oid_ops, 
			   sync, oid->entity);
}

errno_t reiser4_oid_print(reiser4_oid_t *oid, aal_stream_t *stream) {
	aal_assert("umka-1562", oid != NULL);
	aal_assert("umka-1563", stream != NULL);

	return plugin_call(oid->entity->plugin->o.oid_ops,
			   print, oid->entity, stream, 0);
}

/* Returns number of used oids from passed oid allocator */
uint64_t reiser4_oid_used(reiser4_oid_t *oid) {
	aal_assert("umka-527", oid != NULL);
    
	return plugin_call(oid->entity->plugin->o.oid_ops, 
			   used, oid->entity);
}

/* Returns number of free oids from passed oid allocator */
uint64_t reiser4_oid_free(reiser4_oid_t *oid) {
	aal_assert("umka-527", oid != NULL);
    
	return plugin_call(oid->entity->plugin->o.oid_ops, 
			   free, oid->entity);
}

/* Checks specified oid allocator for validness */
errno_t reiser4_oid_valid(reiser4_oid_t *oid) {
	aal_assert("umka-962", oid != NULL);
    
	return plugin_call(oid->entity->plugin->o.oid_ops, 
			   valid, oid->entity);
}

/* Returns root parent locality from specified oid allocator */
oid_t reiser4_oid_hyper_locality(reiser4_oid_t *oid) {
	aal_assert("umka-745", oid != NULL);
    
	return plugin_call(oid->entity->plugin->o.oid_ops, 
			   hyper_locality,);
}

/* Returns root parent objectid from specified oid allocator */
oid_t reiser4_oid_root_locality(reiser4_oid_t *oid) {
	aal_assert("umka-746", oid != NULL);
    
	return plugin_call(oid->entity->plugin->o.oid_ops, 
			   root_locality,);
}

/* Returns root objectid from specified oid allocator */
oid_t reiser4_oid_root_objectid(reiser4_oid_t *oid) {
	aal_assert("umka-747", oid != NULL);
    
	return plugin_call(oid->entity->plugin->o.oid_ops, 
			   root_objectid,);
}
#endif
