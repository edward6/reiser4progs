/*
  nodeptr40.c -- reiser4 default node pointer item plugin.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "nodeptr40.h"

static reiser4_core_t *core = NULL;

/*
  Returns the number of units in nodeptr. As nodeptr40 has not units and thus
  cannot be splitted by balancing, it has one unit.
*/
static uint32_t nodeptr40_units(item_entity_t *item) {
	return 1;
}

#ifndef ENABLE_COMPACT

/* Initializes the area nodeptr will lie in */
static errno_t nodeptr40_init(item_entity_t *item) {
	aal_assert("umka-1671", item != NULL, return -1);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

/* Inserts new nodeptr described by passed @buff */
static errno_t nodeptr40_insert(item_entity_t *item, void *buff,
				uint32_t pos, uint32_t count)
{
	nodeptr40_t *nodeptr;
	reiser4_item_hint_t *hint;
	reiser4_ptr_hint_t *ptr_hint;
    
	aal_assert("vpf-063", item != NULL, return -1); 
	aal_assert("vpf-064", buff != NULL, return -1);

	if (!(nodeptr = nodeptr40_body(item)))
		return -1;
	
	hint = (reiser4_item_hint_t *)buff;
	ptr_hint = (reiser4_ptr_hint_t *)hint->hint;
	
	np40_set_ptr(nodeptr, ptr_hint->ptr);
	    
	return 0;
}

/* Estimates how many bytes is needed for creating new nodeptr */
static errno_t nodeptr40_estimate(item_entity_t *item, void *buff,
				  uint32_t pos, uint32_t count) 
{
	reiser4_item_hint_t *hint;
	
	aal_assert("vpf-068", buff != NULL, return -1);
    
	hint = (reiser4_item_hint_t *)buff;
	hint->len = sizeof(nodeptr40_t);
	
	return 0;
}

/* Prints passed nodeptr into @stream */
static errno_t nodeptr40_print(item_entity_t *item,
			       aal_stream_t *stream,
			       uint16_t options) 
{
	nodeptr40_t *nodeptr;
	
	aal_assert("umka-544", item != NULL, return -1);
	aal_assert("umka-545", stream != NULL, return -1);
    
	nodeptr = nodeptr40_body(item);

	aal_stream_format(stream, "NODEPTR: len=%u, KEY: ", item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print, &item->key,
			stream, options))
		return -1;
	
	aal_stream_format(stream, " PLUGIN: 0x%x (%s)\n",
			  item->plugin->h.id, item->plugin->h.label);
	
	aal_stream_format(stream, "[ %llu ]", np40_get_ptr(nodeptr));
	
	return 0;
}

#endif

/* Reads nodeptr into passed buff */
static int32_t nodeptr40_fetch(item_entity_t *item, void *buff,
			       uint32_t pos, uint32_t count)
{
	nodeptr40_t *nodeptr;
	reiser4_ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1419", item != NULL, return -1);
	aal_assert("umka-1420", buff != NULL, return -1);

	nodeptr = nodeptr40_body(item);
	ptr_hint = (reiser4_ptr_hint_t *)buff;
	
	ptr_hint->width = 1;
	ptr_hint->ptr = np40_get_ptr(nodeptr);
	
	return 1;
}

#ifndef ENABLE_COMPACT

/*
  Layout implementation for nodeptr40. It calls @func for each block nodeptr
  points to.
*/
static errno_t nodeptr40_layout(item_entity_t *item,
				data_func_t func,
				void *data)
{
	errno_t res;
	uint64_t start;
	nodeptr40_t *nodeptr;
	
	aal_assert("umka-1749", item != NULL, return -1);
	aal_assert("vpf-718",   item->body != NULL, return -1);
	aal_assert("umka-1750", func != NULL, return -1);

	start = np40_get_ptr(nodeptr40_body(item));
	
	if ((res = func(item, start, start + 1, data)))
		return res;

	return 0;
}

/* Makes update of the specified nodeptr */
static int32_t nodeptr40_update(item_entity_t *item, void *buff,
				uint32_t pos, uint32_t count)
{
	nodeptr40_t *nodeptr;
	reiser4_ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1423", item != NULL, return -1);
	aal_assert("umka-1424", buff != NULL, return -1);

	nodeptr = nodeptr40_body(item);
	ptr_hint = (reiser4_ptr_hint_t *)buff;
	
	np40_set_ptr(nodeptr, ptr_hint->ptr);
	return 1;
}

#endif

static reiser4_plugin_t nodeptr40_plugin = {
	.item_ops = {
		.h = {
			.handle = empty_handle,
			.id = ITEM_NODEPTR40_ID,
			.group = NODEPTR_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "nodeptr40",
			.desc = "Node pointer item for reiserfs 4.0, ver. " VERSION,
		},
		.check = NULL,
		
#ifndef ENABLE_COMPACT	    
		.init		= nodeptr40_init,
		.insert		= nodeptr40_insert,
		.update         = nodeptr40_update,
		.estimate	= nodeptr40_estimate,
		.print		= nodeptr40_print,
#else
		.init		= NULL,
		.insert		= NULL,
		.update         = NULL,
		.estimate	= NULL,
		.print		= NULL,
#endif
		.units		= nodeptr40_units,
		.fetch          = nodeptr40_fetch,
		.layout         = nodeptr40_layout,
		
		.belongs        = NULL,
		.lookup		= NULL,
		.valid		= NULL,
		.remove		= NULL,
		.mergeable      = NULL,

		.shift          = NULL,
		.predict        = NULL,

		.get_key	= NULL,
		.set_key	= NULL,
		
		.maxposs_key	= NULL,
		.utmost_key     = NULL,
		.gap_key	= NULL
	}
};

static reiser4_plugin_t *nodeptr40_start(reiser4_core_t *c) {
	core = c;
	return &nodeptr40_plugin;
}

plugin_register(nodeptr40_start, NULL);
