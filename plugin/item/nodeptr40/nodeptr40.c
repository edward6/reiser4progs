/*
  nodeptr40.c -- reiser4 default node pointer item plugin.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "nodeptr40.h"

static reiser4_core_t *core = NULL;

static nodeptr40_t *nodeptr40_body(item_entity_t *item) {
	return (nodeptr40_t *)item->body;
}

static uint32_t nodeptr40_count(item_entity_t *item) {
	return 1;
}

#ifndef ENABLE_COMPACT

static errno_t nodeptr40_init(item_entity_t *item, 
			      reiser4_item_hint_t *hint)
{
	nodeptr40_t *nodeptr;
    
	aal_assert("vpf-063", item != NULL, return -1); 
	aal_assert("vpf-064", hint != NULL, return -1);

	nodeptr = nodeptr40_body(item);
	np40_set_ptr(nodeptr,((reiser4_ptr_hint_t *)hint->hint)->ptr);
	    
	return 0;
}

static errno_t nodeptr40_estimate(item_entity_t *item, uint32_t pos,
				  reiser4_item_hint_t *hint) 
{
	aal_assert("vpf-068", hint != NULL, return -1);
    
	hint->len = sizeof(nodeptr40_t);
	return 0;
}

static errno_t nodeptr40_print(item_entity_t *item, aal_stream_t *stream,
			       uint16_t options) 
{
	nodeptr40_t *nodeptr;
	
	aal_assert("umka-544", item != NULL, return -1);
	aal_assert("umka-545", stream != NULL, return -1);
    
	nodeptr = nodeptr40_body(item);

	aal_stream_format(stream, "[ %llu ]", np40_get_ptr(nodeptr));
	return 0;
}

#endif

static errno_t nodeptr40_fetch(item_entity_t *item, uint32_t pos,
			       void *buff, uint32_t count)
{
	nodeptr40_t *nodeptr;
	reiser4_ptr_hint_t *hint = (reiser4_ptr_hint_t *)buff;
		
	aal_assert("umka-1419", item != NULL, return -1);
	aal_assert("umka-1420", buff != NULL, return -1);

	nodeptr = nodeptr40_body(item);
	hint->ptr = np40_get_ptr(nodeptr);
	hint->width = 1;
	
	return 0;
}

#ifndef ENABLE_COMPACT

static errno_t nodeptr40_update(item_entity_t *item, uint32_t pos,
				void *buff, uint32_t count)
{
	nodeptr40_t *nodeptr;
	reiser4_ptr_hint_t *hint = (reiser4_ptr_hint_t *)buff;
		
	aal_assert("umka-1423", item != NULL, return -1);
	aal_assert("umka-1424", buff != NULL, return -1);

	nodeptr = nodeptr40_body(item);
	np40_set_ptr(nodeptr, hint->ptr);
	
	return 0;
}

#endif

static reiser4_plugin_t nodeptr40_plugin = {
	.item_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.sign   = {
				.id = ITEM_NODEPTR40_ID,
				.group = NODEPTR_ITEM,
				.type = ITEM_PLUGIN_TYPE,
			},
			.label = "nodeptr40",
			.desc = "Node pointer item for reiserfs 4.0, ver. " VERSION,
		},
		.check = NULL,
#ifndef ENABLE_COMPACT	    
		.init		= nodeptr40_init,
		.update         = nodeptr40_update,
		.estimate	= nodeptr40_estimate,
		.print		= nodeptr40_print,
#else
		.init		= NULL,
		.update         = NULL,
		.estimate	= NULL,
		.print		= NULL,
#endif
		.lookup		= NULL,
		.valid		= NULL,
		.insert		= NULL,
		.remove		= NULL,
		.shift          = NULL,
		.open           = NULL,
		.mergeable      = NULL,

		.count		= nodeptr40_count,
		.fetch          = nodeptr40_fetch,
	
		.max_poss_key	= NULL,
		.max_real_key   = NULL,
	}
};

static reiser4_plugin_t *nodeptr40_start(reiser4_core_t *c) {
	core = c;
	return &nodeptr40_plugin;
}

plugin_register(nodeptr40_start, NULL);
