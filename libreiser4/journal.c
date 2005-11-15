/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   journal.c -- reiser4 filesystem common journal code. */

#ifndef ENABLE_MINIMAL
#include <reiser4/libreiser4.h>

bool_t reiser4_journal_isdirty(reiser4_journal_t *journal) {
	uint32_t state;
	
	aal_assert("umka-2652", journal != NULL);

	state = reiser4call(journal, get_state);
	return (state & (1 << ENTITY_DIRTY));
}

void reiser4_journal_mkdirty(reiser4_journal_t *journal) {
	uint32_t state;
	
	aal_assert("umka-2653", journal != NULL);

	state = reiser4call(journal, get_state);
	state |= (1 << ENTITY_DIRTY);
	reiser4call(journal, set_state, state);
}

void reiser4_journal_mkclean(reiser4_journal_t *journal) {
	uint32_t state;
	
	aal_assert("umka-2654", journal != NULL);

	state = reiser4call(journal, get_state);
	state &= ~(1 << ENTITY_DIRTY);
	reiser4call(journal, set_state, state);
}

/* This function opens journal on specified device and returns instance of
   opened journal. */
reiser4_journal_t *reiser4_journal_open(
	reiser4_fs_t *fs,	        /* fs journal will be opened on */
	aal_device_t *device)	        /* device journal will be opened on */
{
	rid_t pid;
	blk_t start;
	count_t blocks;
	uint32_t blksize;
	reiser4_plug_t *plug;
	reiser4_journal_t *journal;
	
	aal_assert("umka-095", fs != NULL);
	aal_assert("umka-1695", fs->format != NULL);
	
	/* Allocating memory for journal instance and initialize its fields. */
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	journal->fs = fs;
	journal->device = device;
	journal->fs->journal = journal;

	if ((pid = reiser4_format_journal_pid(fs->format)) == INVAL_PID) {
		aal_error("Invalid journal plugin id has been found.");
		goto error_free_journal;
	}
 
	/* Getting plugin by its id from plugin factory */
	if (!(plug = reiser4_factory_ifind(JOURNAL_PLUG_TYPE, pid))) {
		aal_error("Can't find journal plugin by its "
			  "id 0x%x.", pid);
		goto error_free_journal;
	}

	start = reiser4_format_start(fs->format);
	blocks = reiser4_format_get_len(fs->format);

	blksize = reiser4_master_get_blksize(fs->master);
	
	/* Initializing journal entity by means of calling "open" method from
	   found journal plugin. */
	if (!(journal->ent = plugcall((reiser4_journal_plug_t *)plug, 
				      open, journal->device, blksize,
				      fs->format->ent, fs->oid->ent,
				      start, blocks))) 
	{
		aal_error("Can't open journal %s on %s.",
			  plug->label, device->name);
		goto error_free_journal;
	}
	
	return journal;

 error_free_journal:
	aal_free(journal);
	return NULL;
}

errno_t reiser4_journal_layout(reiser4_journal_t *journal, 
			       region_func_t region_func,
			       void *data)
{
	aal_assert("umka-1078", journal != NULL);
	aal_assert("umka-1079", region_func != NULL);

	return reiser4call(journal, layout, region_func, data);
}

static errno_t cb_action_mark(blk_t start, count_t width, void *data) {
	reiser4_fs_t *fs = (reiser4_fs_t *)data;
	errno_t res;

	if ((res = reiser4_format_dec_free(fs->format, width)))
		return res;
		
	return reiser4_alloc_occupy(fs->alloc, start, width);
}

/* Marks journal area as used */
errno_t reiser4_journal_mark(reiser4_journal_t *journal) {
	aal_assert("umka-1855", journal != NULL);
	aal_assert("umka-1856", journal->fs != NULL);
	aal_assert("umka-1856", journal->fs->alloc != NULL);
	
	return reiser4_journal_layout(journal, cb_action_mark,
				      journal->fs);
}

/* Creates journal on specified jopurnal. Returns initialized instance */
reiser4_journal_t *reiser4_journal_create(
	reiser4_fs_t *fs,	        /* fs journal will be opened on */
	aal_device_t *device)	        /* device journal will be created on */
{
	rid_t pid;
	blk_t start;
	count_t blocks;
	uint32_t blksize;
	reiser4_plug_t *plug;
	reiser4_journal_t *journal;

	aal_assert("umka-1697", fs != NULL);
	aal_assert("umka-1696", fs->format != NULL);
	
	/* Allocating memory and finding plugin */
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;

	journal->fs = fs;
	journal->device = device;

	/* Getting journal plugin to be used. */
	if ((pid = reiser4_format_journal_pid(fs->format)) == INVAL_PID) {
		aal_error("Invalid journal plugin id has been found.");
		goto error_free_journal;
	}
    
	if (!(plug = reiser4_factory_ifind(JOURNAL_PLUG_TYPE, pid)))  {
		aal_error("Can't find journal plugin by its id 0x%x.", pid);
		goto error_free_journal;
	}
    
	start = reiser4_format_start(fs->format);
	blocks = reiser4_format_get_len(fs->format);

	blksize = reiser4_master_get_blksize(fs->master);
	
	/* Creating journal entity. */
	if (!(journal->ent = plugcall((reiser4_journal_plug_t *)plug,
				      create, journal->device, blksize,
				      fs->format->ent, fs->oid->ent,
				      start, blocks)))
	{
		aal_error("Can't create journal %s on %s.",
			  plug->label, journal->device->name);
		goto error_free_journal;
	}
	
	if (reiser4_journal_mark(journal)) {
		aal_error("Can't mark journal blocks used in "
			  "block allocator.");
		goto error_free_entity;
	}
	
	return journal;

 error_free_entity:
	reiser4call(journal, close);
	
 error_free_journal:
	aal_free(journal);
	return NULL;
}

/* Replays specified @journal and returns error code as a result. As super block
   may fit into one of replayed transactions, it should be reopened after replay
   is finished. */
errno_t reiser4_journal_replay(
	reiser4_journal_t *journal)	/* journal to be replayed */
{
	aal_assert("umka-727", journal != NULL);
    
	/* Calling plugin for actual replaying */
	return reiser4call(journal, replay);
}

/* Saves journal structures on journal device */
errno_t reiser4_journal_sync(
	reiser4_journal_t *journal)	/* journal to be saved */
{
	aal_assert("umka-100", journal != NULL);

	if (!reiser4_journal_isdirty(journal))
		return 0;
	
	return reiser4call(journal, sync);
}

/* Checks journal structure for validness */
errno_t reiser4_journal_valid(
	reiser4_journal_t *journal)  /* journal to be checked */
{
	aal_assert("umka-830", journal != NULL);

	return reiser4call(journal, valid);
}

/* Closes journal by means of freeing all assosiated memory */
void reiser4_journal_close(
	reiser4_journal_t *journal)	/* jouranl to be closed */
{
	aal_assert("umka-102", journal != NULL);
	
	reiser4_journal_sync(journal);
	reiser4call(journal, close);
	aal_free(journal);
}
#endif
