/*
  journal.c -- reiser4 filesystem common journal code.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE

#include <reiser4/reiser4.h>

/* 
   This function opens journal on specified device and returns instance of
   opened journal.
*/
reiser4_journal_t *reiser4_journal_open(
	reiser4_fs_t *fs,	        /* fs journal will be opened on */
	aal_device_t *device)	        /* device journal will be opened on */
{
	rid_t pid;
	blk_t start;
	count_t len;
	
	reiser4_plugin_t *plugin;
	reiser4_journal_t *journal;
	
	aal_assert("umka-095", fs != NULL);
	aal_assert("umka-1695", fs->format != NULL);
	
	/* Allocating memory for jouranl instance */
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	if ((pid = reiser4_format_journal_pid(fs->format)) == INVAL_PID) {
		aal_exception_error("Invalid journal plugin id has been found.");
		goto error_free_journal;
	}
 
	/* Getting plugin by its id from plugin factory */
	if (!(plugin = libreiser4_factory_ifind(JOURNAL_PLUGIN_TYPE, pid))) {
		aal_exception_error("Can't find journal plugin by its id 0x%x.", pid);
		goto error_free_journal;
	}
    
	journal->fs = fs;
	journal->fs->journal = journal;
	
	journal->device = device;

	start = reiser4_format_start(fs->format);
	len = reiser4_format_get_len(fs->format);
	
	/* 
	   Initializing journal entity by means of calling "open" method from
	   found journal plugin.
	*/
	if (!(journal->entity = plugin_call(plugin->journal_ops, open,
					    fs->format->entity, device,
					    start, len))) 
	{
		aal_exception_error("Can't open journal %s on %s.",
				    plugin->h.label, fs->device->name);
		goto error_free_journal;
	}
	
	return journal;

 error_free_journal:
	aal_free(journal);
	return NULL;
}

errno_t reiser4_journal_layout(reiser4_journal_t *journal, 
			       block_func_t func,
			       void *data)
{
	aal_assert("umka-1078", journal != NULL);
	aal_assert("umka-1079", func != NULL);

	return plugin_call(journal->entity->plugin->journal_ops,
			   layout, journal->entity, func, data);
}

static errno_t callback_action_mark(
	object_entity_t *entity,	/* device for operating on */ 
	blk_t blk,			/* block number to be marked */
	void *data)			/* pointer to block allocator */
{
	reiser4_alloc_t *alloc = (reiser4_alloc_t *)data;
	return reiser4_alloc_occupy_region(alloc, blk, 1);
}

/* Marks format area as used */
errno_t reiser4_journal_mark(reiser4_journal_t *journal) {
	aal_assert("umka-1855", journal != NULL);
	aal_assert("umka-1856", journal->fs != NULL);
	aal_assert("umka-1856", journal->fs->alloc != NULL);
	
	return reiser4_journal_layout(journal, callback_action_mark,
				      journal->fs->alloc);
}

/* Creates journal on specified jopurnal. Returns initialized instance */
reiser4_journal_t *reiser4_journal_create(
	reiser4_fs_t *fs,	        /* fs journal will be opened on */
	aal_device_t *device,	        /* device journal will be created on */
	void *hint)		        /* journal params (opaque pointer) */
{
	rid_t pid;
	blk_t start;
	count_t len;
	
	reiser4_plugin_t *plugin;
	reiser4_journal_t *journal;

	aal_assert("umka-1697", fs != NULL);
	aal_assert("umka-1696", fs->format != NULL);
	
	/* Allocating memory and finding plugin */
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	if ((pid = reiser4_format_journal_pid(fs->format)) == INVAL_PID) {
		aal_exception_error("Invalid journal plugin id has been found.");
		goto error_free_journal;
	}
    
	if (!(plugin = libreiser4_factory_ifind(JOURNAL_PLUGIN_TYPE, pid)))  {
		aal_exception_error("Can't find journal plugin by its id 0x%x.", pid);
		goto error_free_journal;
	}
    
	journal->fs = fs;
	journal->fs->journal = journal;
	
	journal->device = device;
	
	start = reiser4_format_start(fs->format);
	len = reiser4_format_get_len(fs->format);
	
	/* Initializing journal entity */
	if (!(journal->entity = plugin_call(plugin->journal_ops, create,
					    fs->format->entity, device, start,
					    len, hint))) 
	{
		aal_exception_error("Can't create journal %s on %s.",
				    plugin->h.label, 
				    aal_device_name(device));
		goto error_free_journal;
	}
	
	if (reiser4_journal_mark(journal))
		goto error_free_entity;
			
	return journal;

 error_free_entity:
	plugin_call(journal->entity->plugin->journal_ops, 
		    close, journal->entity);
 error_free_journal:
	aal_free(journal);
	return NULL;
}

/* Replays specified journal. Returns error code */
errno_t reiser4_journal_replay(
	reiser4_journal_t *journal)	/* journal to be replayed */
{
	aal_assert("umka-727", journal != NULL);
    
	if (aal_device_readonly(journal->device)) {
		aal_exception_warn("Transactions can't be replayed on "
				   "read only opened filesystem.");
		return -1;
	}
	
	/* Calling plugin for actual replaying */
	return plugin_call(journal->entity->plugin->journal_ops, 
			   replay, journal->entity);
}

/* Saves journal strucres on jouranl's device */
errno_t reiser4_journal_sync(
	reiser4_journal_t *journal)	/* journal to be saved */
{
	aal_assert("umka-100", journal != NULL);

	return plugin_call(journal->entity->plugin->journal_ops, 
			   sync, journal->entity);
}

/* Checks jouranl structure for validness */
errno_t reiser4_journal_valid(
	reiser4_journal_t *journal)  /* jouranl to eb checked */
{
	aal_assert("umka-830", journal != NULL);

	return plugin_call(journal->entity->plugin->journal_ops, 
			   valid, journal->entity);
}

errno_t reiser4_journal_print(reiser4_journal_t *journal, aal_stream_t *stream) {
	aal_assert("umka-1564", journal != NULL);
	aal_assert("umka-1565", stream != NULL);

	return plugin_call(journal->entity->plugin->journal_ops,
			   print, journal->entity, stream, 0);
}

/* Closes journal by means of freeing all assosiated memory */
void reiser4_journal_close(
	reiser4_journal_t *journal)	/* jouranl to be closed */
{
	aal_assert("umka-102", journal != NULL);

	journal->fs->journal = NULL;
	
	plugin_call(journal->entity->plugin->journal_ops, 
		    close, journal->entity);
    
	aal_free(journal);
}

#endif
