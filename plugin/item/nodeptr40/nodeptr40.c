/*
  nodeptr40.c -- reiser4 default node pointer item plugin.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
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

/* Reads nodeptr into passed buff */
static int32_t nodeptr40_read(item_entity_t *item, void *buff,
			      uint32_t pos, uint32_t count)
{
	nodeptr40_t *nodeptr;
	reiser4_ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1419", item != NULL);
	aal_assert("umka-1420", buff != NULL);

	nodeptr = nodeptr40_body(item);
	ptr_hint = (reiser4_ptr_hint_t *)buff;
	
	ptr_hint->width = 1;
	ptr_hint->ptr = np40_get_ptr(nodeptr);
	
	return 1;
}

static int nodeptr40_branch(void) {
	return 1;
}

/*
  Layout implementation for nodeptr40. It calls @func for each block nodeptr
  points to.
*/
static errno_t nodeptr40_layout(item_entity_t *item,
				region_func_t func,
				void *data)
{
	errno_t res;
	nodeptr40_t *nodeptr;
	
	aal_assert("umka-1749", item != NULL);
	aal_assert("umka-1750", func != NULL);
	aal_assert("vpf-718",   item->body != NULL);

	if ((res = func(item, np40_get_ptr(nodeptr40_body(item)), 1, data)))
		return res;

	return 0;
}

#ifndef ENABLE_STAND_ALONE

/* Writes of the specified nodeptr into passed @item*/
static int32_t nodeptr40_write(item_entity_t *item, void *buff,
			       uint32_t pos, uint32_t count)
{
	nodeptr40_t *nodeptr;

	reiser4_item_hint_t *hint;
	reiser4_ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1423", item != NULL);
	aal_assert("umka-1424", buff != NULL);

	nodeptr = nodeptr40_body(item);
	
	hint = (reiser4_item_hint_t *)buff;
	ptr_hint = (reiser4_ptr_hint_t *)hint->type_specific;
	
	np40_set_ptr(nodeptr, ptr_hint->ptr);
	return 1;
}


/* Initializes the area nodeptr will lie in */
static errno_t nodeptr40_init(item_entity_t *item) {
	aal_assert("umka-1671", item != NULL);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

/* Estimates how many bytes is needed for creating new nodeptr */
static errno_t nodeptr40_estimate(item_entity_t *item, void *buff,
				  uint32_t pos, uint32_t count) 
{
	reiser4_item_hint_t *hint;
	
	aal_assert("vpf-068", buff != NULL);
    
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
	
	aal_assert("umka-544", item != NULL);
	aal_assert("umka-545", stream != NULL);
    
	nodeptr = nodeptr40_body(item);

	aal_stream_format(stream, "NODEPTR: len=%u, KEY: ",
			  item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print,
			&item->key, stream, options))
		return -EINVAL;
	
	aal_stream_format(stream, " PLUGIN: 0x%x (%s)\n",
			  item->plugin->h.id, item->plugin->h.label);
	
	aal_stream_format(stream, "[ %llu ]", np40_get_ptr(nodeptr));
	
	return 0;
}

extern errno_t nodeptr40_layout_check(item_entity_t *item,
				      region_func_t func, 
				      void *data, uint8_t mode);

extern errno_t nodeptr40_check(item_entity_t *item, uint8_t mode);

#endif

static reiser4_plugin_t nodeptr40_plugin = {
	.item_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = ITEM_NODEPTR40_ID,
			.group = NODEPTR_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "nodeptr40",
			.desc = "Node pointer item for reiser4, ver. " VERSION,
		},
#ifndef ENABLE_STAND_ALONE	    
		.init		= nodeptr40_init,
		.write          = nodeptr40_write,
		.estimate	= nodeptr40_estimate,
		.print		= nodeptr40_print,
		.check		= nodeptr40_check,
		.layout         = nodeptr40_layout,
		.layout_check	= nodeptr40_layout_check,

		.insert		= NULL,
		.remove		= NULL,
		.feel           = NULL,

		.shift          = NULL,
		.predict        = NULL,

		.set_key	= NULL,
		.gap_key	= NULL,
		.utmost_key     = NULL,
#endif
		.units		= nodeptr40_units,
		.read           = nodeptr40_read,
		.branch         = nodeptr40_branch,
		
		.data		= NULL,
		.lookup		= NULL,
		.valid		= NULL,
		.mergeable      = NULL,

		.maxposs_key	= NULL,
		.get_key	= NULL
	}
};

static reiser4_plugin_t *nodeptr40_start(reiser4_core_t *c) {
	core = c;
	return &nodeptr40_plugin;
}

plugin_register(nodeptr40_start, NULL);
