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
	ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1419", item != NULL);
	aal_assert("umka-1420", buff != NULL);

	ptr_hint = (ptr_hint_t *)buff;
	nodeptr = nodeptr40_body(item);
	
	ptr_hint->width = 1;
	ptr_hint->start = np40_get_ptr(nodeptr);
	
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
				region_func_t region_func,
				void *data)
{
	nodeptr40_t *nodeptr;
	
	aal_assert("umka-1749", item != NULL);
	aal_assert("umka-1750", func != NULL);
	aal_assert("vpf-718",   item->body != NULL);

	nodeptr = nodeptr40_body(item);
	return region_func(item, np40_get_ptr(nodeptr), 1, data);
}

#ifndef ENABLE_STAND_ALONE

static errno_t nodeptr40_copy(item_entity_t *dst_item,
			      uint32_t dst_pos,
			      item_entity_t *src_item,
			      uint32_t src_pos,
			      uint32_t count)
{
	aal_assert("umka-2073", dst_item != NULL);
	aal_assert("umka-2074", src_item != NULL);

	return -EINVAL;
}

/* Writes of the specified nodeptr into passed @item*/
static int32_t nodeptr40_write(item_entity_t *item, void *buff,
			       uint32_t pos, uint32_t count)
{
	nodeptr40_t *nodeptr;

	create_hint_t *hint;
	ptr_hint_t *ptr_hint;
		
	aal_assert("umka-1423", item != NULL);
	aal_assert("umka-1424", buff != NULL);

	nodeptr = nodeptr40_body(item);
	
	hint = (create_hint_t *)buff;
	ptr_hint = (ptr_hint_t *)hint->type_specific;
	
	np40_set_ptr(nodeptr, ptr_hint->start);
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
	create_hint_t *hint;
	
	aal_assert("vpf-068", buff != NULL);
    
	hint = (create_hint_t *)buff;
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

	aal_stream_format(stream, "NODEPTR PLUGIN=%s LEN=%u, KEY=",
			  item->plugin->h.label, item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print,
			&item->key, stream, options))
	{
		return -EINVAL;
	}
	
	aal_stream_format(stream, " UNITS=1\n");
	
	aal_stream_format(stream, "[ %llu ]\n", np40_get_ptr(nodeptr));
	
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
#ifndef ENABLE_STAND_ALONE
			.desc = "Node pointer item for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
#ifndef ENABLE_STAND_ALONE	    
		.init		= nodeptr40_init,
		.copy           = nodeptr40_copy,
		.write          = nodeptr40_write,
		.estimate	= nodeptr40_estimate,
		.print		= nodeptr40_print,
		.check		= nodeptr40_check,
		.layout         = nodeptr40_layout,
		.layout_check	= nodeptr40_layout_check,

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
