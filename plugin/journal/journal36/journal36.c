/*
  journal36.c -- journal plugin for reiser3.6.x.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "journal36.h"

extern reiser4_plugin_t journal36_plugin;

static reiser4_core_t *core = NULL;

static errno_t journal36_header_check(journal36_header_t *header) {
	return 0;
}

static object_entity_t *journal36_open(object_entity_t *format) {
	journal36_t *journal;

	aal_assert("umka-406", format != NULL, return NULL);
    
	if (!(journal = aal_calloc(sizeof(*journal), 0)))
		return NULL;
    
	journal->plugin = &journal36_plugin;
    
	return (object_entity_t *)journal;
}

static errno_t journal36_sync(object_entity_t *entity) {
	aal_assert("umka-407", entity != NULL, return -1);
	return -1;
}

static void journal36_close(object_entity_t *entity) {
	aal_assert("umka-408", entity != NULL, return);
	aal_free(entity);
}

static errno_t journal36_replay(object_entity_t *entity) {
	return 0;
}

static reiser4_plugin_t journal36_plugin = {
	.journal_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = JOURNAL_REISER36_ID,
			.group = 0,
			.type = JOURNAL_PLUGIN_TYPE,
			.label = "journal36",
			.desc = "Default journal for reiserfs 3.6.x, ver. " VERSION,
		},
		.open	= journal36_open,
		.close	= journal36_close,
		.sync	= journal36_sync,
		.replay = journal36_replay,
		.print  = NULL,
		.create = NULL, 
		.valid	= NULL,
		.device = NULL,
		.check = NULL
	}
};

static reiser4_plugin_t *journal36_start(reiser4_core_t *c) {
	core = c;
	return &journal36_plugin;
}

plugin_register(journal36_start, NULL);

