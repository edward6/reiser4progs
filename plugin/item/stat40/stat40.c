/*
  stat40.c -- reiser4 default stat data plugin.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "stat40.h"
#include <sys/stat.h>

#include <aux/aux.h>

static reiser4_core_t *core = NULL;

static inline stat40_t *stat40_body(item_entity_t *item) {
	return (stat40_t *)item->body;
}

/* Type for stat40 layout callback function */
typedef int (*stat40_perext_func_t) (uint8_t, uint16_t, reiser4_body_t *, void *);

/* The function which implements stat40 layout pass */
static errno_t stat40_layout(item_entity_t *item,
			     stat40_perext_func_t perext_func, void *data)
{
	uint8_t i;
	stat40_t *stat;
	uint16_t extmask;
	
	reiser4_body_t *extbody;
	reiser4_plugin_t *plugin;

	aal_assert("umka-1197", item != NULL, return -1);
    
	stat = stat40_body(item);
	extmask = st40_get_extmask(stat);
	extbody = (reiser4_body_t *)stat + sizeof(stat40_t);
    
	for (i = 0; i < sizeof(uint64_t)*8; i++) {
		int ret;

		if ((i + 1) % 16 == 0) {
			
			if (!((1 << i) & extmask))
				break;
			
			extmask = *((uint16_t *)extbody);

			/* Clear the last bit in last mask */
			if (((uint64_t)1 << i) & 0x2f) {
				if (!(extmask & 0x8000))
					extmask &= ~0x8000;
			}
			
			extbody = (void *)extbody + sizeof(d16_t);
			continue;
		}

		if (!((1 << i) & extmask))
			continue;
		
		if (!(ret = perext_func(i, extmask, extbody, data)))
			return ret;

		if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			return 0;
		}

		extbody = (void *)extbody + plugin_call(return 0, plugin->sdext_ops,
							length, extbody);
	}
    
	return 0;
}

static int callback_open(uint8_t ext, uint16_t extmask,
			 reiser4_body_t *extbody, void *data)
{
	reiser4_plugin_t *plugin;
	reiser4_statdata_hint_t *hint;

	hint = ((reiser4_item_hint_t *)data)->hint;

	/* Reading mask into hint */
	if ((ext + 1) % 16 == 0) {
		hint->extmask <<= 16;
		hint->extmask |= extmask;
	}
	
	if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, ext))) {
		aal_exception_warn("Can't find stat data extention plugin "
				   "by its id 0x%x.", ext);
		return -1;
	}

	if (hint->ext[ext]) {
		if (plugin_call(return -1, plugin->sdext_ops, open,
				extbody, hint->ext[ext]))
			return -1;
	}
	
	return 1;
}

static errno_t stat40_open(item_entity_t *item, 
			   reiser4_item_hint_t *hint)
{
	aal_assert("umka-1414", item != NULL, return -1);
	aal_assert("umka-1415", hint != NULL, return -1);

	return stat40_layout(item, callback_open, (void *)hint);
}

#ifndef ENABLE_COMPACT

static errno_t stat40_init(item_entity_t *item) {
	aal_assert("umka-1670", item != NULL, return -1);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

static errno_t stat40_estimate(item_entity_t *item, uint32_t pos, 
			       reiser4_item_hint_t *hint) 
{
	uint8_t i;
	reiser4_statdata_hint_t *stat_hint;
    
	aal_assert("vpf-074", hint != NULL, return -1);

	hint->len = sizeof(stat40_t);
	stat_hint = (reiser4_statdata_hint_t *)hint->hint;
    
	if (!stat_hint->extmask)
		return 0;
    
	/* Estimating the all stat data extentions */
	for (i = 0; i < sizeof(uint64_t) * 8; i++) {
		reiser4_plugin_t *plugin;
	
		if (!(((uint64_t)1 << i) & stat_hint->extmask))
			continue;
	
		if ((i + 1) % 16 == 0) {
			hint->len += sizeof(d16_t);
			continue;
		}
	
		if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			continue;
		}
	
		hint->len += plugin_call(return -1, plugin->sdext_ops, 
					 length, stat_hint->ext[i]);
	}
	
	return 0;
}

/* This method inserts the stat data extentions */
static errno_t stat40_insert(item_entity_t *item, uint32_t pos,
			     reiser4_item_hint_t *hint)
{
	uint8_t i;
	reiser4_body_t *extbody;
	reiser4_statdata_hint_t *stat_hint;
    
	aal_assert("vpf-076", item != NULL, return -1); 
	aal_assert("vpf-075", hint != NULL, return -1);

	/*
	  FIXME-UMKA: Should this function insert extentions like for exmple
	  direntry does?
	*/
	extbody = (reiser4_body_t *)stat40_body(item);
	stat_hint = (reiser4_statdata_hint_t *)hint->hint;
    
	if (!stat_hint->extmask)
		return 0;
    
	for (i = 0; i < sizeof(uint64_t) * 8; i++) {
		reiser4_plugin_t *plugin;
	
		if (!(((uint64_t)1 << i) & stat_hint->extmask))
			continue;
	    
		/* 
		   Stat data contains 16 bit mask of extentions used in it. The
		   first 15 bits of the mask denote the first 15 extentions in
		   the stat data.  And the bit number is the stat data extention
		   plugin id. If the last bit turned on, it means that one more
		   16 bit mask present and so on. So, we should add sizeof(mask)
		   to extention body pointer, in the case we are on bit denoted
		   for indicating if next extention in use or not.
		*/
		if (i == 0 || (i + 1) % 16 == 0) {
			uint16_t extmask;

			extmask = (stat_hint->extmask >> i) &
				0x000000000000ffff;
			
			st40_set_extmask((stat40_t *)extbody, extmask);
			extbody = (void *)extbody + sizeof(d16_t);
			
			if (i > 0) continue;
		}
	    
		if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			continue;
		}
	
		plugin_call(return -1, plugin->sdext_ops, init, extbody, 
			    stat_hint->ext[i]);
	
		/* 
		   Getting pointer to the next extention. It is evaluating as
		   previous pointer plus its size.
		*/
		extbody += plugin_call(return -1, plugin->sdext_ops,
				       length, extbody);
	}
    
	return 0;
}

/* This method deletes the stat data extentions */
static uint16_t stat40_remove(item_entity_t *item, 
			      uint32_t pos)
{
	return -1;
}

extern errno_t stat40_check(item_entity_t *);

#endif

/* Here we probably should check all stat data extention masks */
static errno_t stat40_valid(item_entity_t *item) {
	aal_assert("umka-1007", item != NULL, return -1);
	return 0;
}

/* This function returns stat data extention count */
static uint32_t stat40_units(item_entity_t *item) {
	return 1;
}

/* Helper structrure for keeping track of stat data extention body */
struct body_hint {
	reiser4_body_t *body;
	uint8_t ext;
};

/* Callback function for finding stat data extention body by bit */
static int callback_body(uint8_t ext, uint16_t extmask,
			 reiser4_body_t *extbody, void *data)
{
	struct body_hint *hint = (struct body_hint *)data;

	hint->body = extbody;
	return (ext < hint->ext);
}

static reiser4_body_t *stat40_sdext_body(item_entity_t *item, 
					 uint8_t bit)
{
	struct body_hint hint = {NULL, bit};

	if (stat40_layout(item, callback_body, &hint) < 0)
		return NULL;
	
	return hint.body;
}

/* Helper structure for keeping track of presence of a stat data
 * extention */
struct present_hint {
	int present;
	uint8_t ext;
};

/* Callback for getting presence information for certain stat data
 * extention */
static int callback_present(uint8_t ext, uint16_t extmask,
			    reiser4_body_t *extbody, void *data)
{
	struct present_hint *hint = (struct present_hint *)data;
	
	hint->present = (ext == hint->ext);
	return !hint->present;
}

static int stat40_sdext_present(item_entity_t *item, 
				uint8_t bit)
{
	struct present_hint hint = {0, bit};

	if (!stat40_layout(item, callback_body, &hint) < 0)
		return 0;

	return hint.present;
}

#ifndef ENABLE_COMPACT

/* Callback for counting the number of stat data extentions in use */
static int callback_sdexts(uint8_t ext, uint16_t extmask,
			   reiser4_body_t *extbody, void *data)
{
        (*(uint32_t *)data)++;
        return 1;
}

/* This function returns stat data extention count */
static uint32_t stat40_sdexts(item_entity_t *item) {
        uint32_t count = 0;

        if (stat40_layout(item, callback_sdexts, &count) < 0)
                return 0;

        return count;
}

static int callback_print(uint8_t ext, uint16_t extmask,
			  reiser4_body_t *extbody, void *data)
{
	reiser4_plugin_t *plugin;
	aal_stream_t *stream = (aal_stream_t *)data;

	if (ext == 0 || (ext + 1) % 16 == 0)
		aal_stream_format(stream, "mask:\t\t0x%x\n", extmask);
				
	if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, ext))) {
		aal_exception_warn("Can't find stat data extention plugin "
				   "by its id 0x%x.", ext);
		return 1;
	}

	aal_stream_format(stream, "label:\t\t%s\n", plugin->h.label);
	aal_stream_format(stream, "plugin:\t\t%s\n", plugin->h.desc);
	
	plugin_call(return 1, plugin->sdext_ops, print, extbody,
		    stream, 0);
	
	return 1;
}

static errno_t stat40_print(item_entity_t *item, aal_stream_t *stream,
			    uint16_t options)
{
	aal_assert("umka-1407", item != NULL, return -1);
	aal_assert("umka-1408", stream != NULL, return -1);
    
	aal_stream_format(stream, "count:\t\t%u\n", stat40_sdexts(item));

	if (stat40_layout(item, callback_print, (void *)stream) < 0)
		return -1;

	return 0;
}

#endif

static reiser4_plugin_t stat40_plugin = {
	.item_ops = {
		.h = {
			.handle = { "", NULL, NULL, NULL },
			.id = ITEM_STATDATA40_ID,
			.group = STATDATA_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "stat40",
			.desc = "Stat data for reiserfs 4.0, ver. " VERSION,
		},
#ifndef ENABLE_COMPACT
		.init		= stat40_init,
		.estimate	= stat40_estimate,
		.insert		= stat40_insert,
		.remove		= stat40_remove,
		.check		= stat40_check,
		.print		= stat40_print,
#else
		.init		= NULL,
		.estimate	= NULL,
		.insert		= NULL,
		.remove		= NULL,
		.check		= NULL,
		.print		= NULL,
#endif
		.lookup		= NULL,
		.fetch          = NULL,
		.update         = NULL,
		.mergeable      = NULL,
	    
		.shift          = NULL,
		.predict        = NULL,
		
		.open           = stat40_open,
		.units		= stat40_units,
		.valid		= stat40_valid,
        
		.max_poss_key	= NULL,
		.max_real_key   = NULL,
		.unit_key	= NULL,
	}
};

static reiser4_plugin_t *stat40_start(reiser4_core_t *c) {
	core = c;
	return &stat40_plugin;
}

plugin_register(stat40_start, NULL);

