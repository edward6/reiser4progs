/*
  stat40.c -- reiser4 default stat data plugin.
  Copyright (C) 1996-2002 Hans Reiser.
*/

#include "stat40.h"
#include <sys/stat.h>

static reiser4_core_t *core = NULL;

static stat40_t *stat40_body(reiser4_item_t *item) {

    if (item == NULL) return NULL;
    
    return (stat40_t *)plugin_call(return NULL, 
								   item->node->plugin->node_ops,
								   item_body, item->node, item->pos);
}

/* Type for stat40 layout callback function */
typedef int (*stat40_perext_func_t) (uint8_t, uint16_t, reiser4_body_t *, void *);

/* The function which implements stat40 layout pass */
static errno_t stat40_layout(reiser4_item_t *item,
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
			extmask = *((uint16_t *)extbody);

			/* Clear the last bit in last mask */
			if (((uint64_t)1 << i) & 0x2f) {
				if (!(extmask & 0x8000))
					extmask &= ~0x8000;
			}
			
			extbody = (void *)extbody + sizeof(d16_t);
			continue;
		}

		if (!(ret = perext_func(i, extmask, extbody, data)))
			return ret;

		if (!((1 << i) & extmask))
			continue;
		
		if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
							   "by its id 0x%x.", i);
			return 0;
		}

		extbody = (void *)extbody + plugin_call(return 0,
												plugin->sdext_ops, length,);
    }
    
    return 0;
}

static int callback_open(uint8_t ext, uint16_t extmask,
						 reiser4_body_t *extbody, void *data)
{
	reiser4_plugin_t *plugin;
	reiser4_statdata_hint_t *hint;

	if (!((1 << ext) & extmask))
		return 1;

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

static errno_t stat40_open(reiser4_item_t *item, 
						   reiser4_item_hint_t *hint)
{
	aal_assert("umka-1414", item != NULL, return -1);
	aal_assert("umka-1415", hint != NULL, return -1);

	return stat40_layout(item, callback_open, (void *)hint);
}

#ifndef ENABLE_COMPACT

static errno_t stat40_init(reiser4_item_t *item, 
						   reiser4_item_hint_t *hint)
{
    uint8_t i;
    reiser4_body_t *extbody;
    reiser4_statdata_hint_t *stat_hint;
    
    aal_assert("vpf-076", item != NULL, return -1); 
    aal_assert("vpf-075", hint != NULL, return -1);
    
    extbody = (reiser4_body_t *)stat40_body(item);
    stat_hint = (reiser4_statdata_hint_t *)hint->hint;
    
    if (!stat_hint->extmask)
		return 0;
    
    for (i = 0; i < sizeof(uint64_t)*8; i++) {
		reiser4_plugin_t *plugin;
	
		if (!(((uint64_t)1 << i) & stat_hint->extmask))
			continue;
	    
		/* 
		   Stat data contains 16 bit mask of extentions used in it. The first 
		   15 bits of the mask denote the first 15 extentions in the stat data.
		   And the bit number is the stat data extention plugin id. If the last 
		   bit turned on, it means that one more 16 bit mask present and so on. 
		   So, we should add sizeof(mask) to extention body pointer, in the case
		   we are on bit denoted for indicating if next extention in use or not.
		*/
		if (i == 0 || (i + 1) % 16 == 0) {
			uint16_t extmask;

			extmask = (stat_hint->extmask >> i) & 0x000000000000ffff;
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
		   Getting pointer to the next extention. It is evaluating as previous 
		   pointer plus its size.
		*/
		extbody += plugin_call(return -1, plugin->sdext_ops, length,);
    }
    
    return 0;
}

static errno_t stat40_estimate(reiser4_item_t *item, uint32_t pos, 
							   reiser4_item_hint_t *hint) 
{
    uint8_t i;
    reiser4_statdata_hint_t *stat_hint;
    
    aal_assert("vpf-074", hint != NULL, return -1);
    aal_assert("umka-1196", item != NULL, return -1);

    hint->len = sizeof(stat40_t);
    stat_hint = (reiser4_statdata_hint_t *)hint->hint;
    
    if (!stat_hint->extmask)
		return 0;
    
    /* Estimating the all stat data extentions */
    for (i = 0; i < sizeof(uint64_t)*8; i++) {
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
								 length,);
    }
	
    return 0;
}

/* This method inserts the stat data extentions */
static errno_t stat40_insert(reiser4_item_t *item, 
							 uint32_t pos, reiser4_item_hint_t *hint)
{
    return -1;
}

/* This method deletes the stat data extentions */
static uint16_t stat40_remove(reiser4_item_t *item, 
							  uint32_t pos)
{
    return -1;
}

extern errno_t stat40_check(reiser4_item_t *, uint16_t);

#endif

/* Here we probably should check all stat data extention masks */
static errno_t stat40_valid(reiser4_item_t *item) {
    aal_assert("umka-1007", item != NULL, return -1);
    return 0;
}

/* Callbakc for counting the number of stat data extentions in use */
static int callback_count(uint8_t ext, uint16_t extmask,
					  reiser4_body_t *extbody, void *data)
{
	uint32_t *count = (uint32_t *)data;
	*count += ((1 << ext) & extmask);
	return 1;
}

/* This function returns stat data extention count */
static uint32_t stat40_count(reiser4_item_t *item) {
	uint32_t count = 0;

	if (stat40_layout(item, callback_count, &count) < 0)
		return 0;
	
    return count;
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

static reiser4_body_t *stat40_sdext_body(reiser4_item_t *item, 
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
	
	hint->present = (((1 << ext) & extmask));
	return (!hint->present && ext < hint->ext);
}

static int stat40_sdext_present(reiser4_item_t *item, 
								uint8_t bit)
{
	struct present_hint hint = {0, bit};

	if (!stat40_layout(item, callback_body, &hint) < 0)
		return 0;

	return hint.present;
}

/*static errno_t stat40_sdext_open(reiser4_item_t *item, 
								 uint8_t bit, stat40_sdext_t *sdext)
{
    aal_assert("umka-1193", item != NULL, return -1);
    aal_assert("umka-1194", sdext != NULL, return -1);

    if (!stat40_sdext_present(item, bit)) {
		aal_exception_error("Stat data extention 0x%x "
							"is not present.", bit);
		return -1;
    }
    
    if (!(sdext->plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, bit))) {
		aal_exception_error("Can't find stat data extention plugin "
							"by its id %x.", bit);
		return -1;
    }
    
    return -((sdext->body = stat40_sdext_body(item, bit)) == NULL);
}*/

static errno_t stat40_print(reiser4_item_t *item,
						   char *buff, uint32_t n,
						   uint16_t options)
{
    stat40_t *stat;
    uint16_t extmask;
	
	aal_assert("umka-1407", item != NULL, return -1);
	aal_assert("umka-1408", buff != NULL, return -1);
    
    aal_assert("umka-1293", item != NULL, return -1);
    
    stat = stat40_body(item);
    extmask = st40_get_extmask(stat);

	return 0;
}

static errno_t stat40_max_poss_key(reiser4_item_t *item,
								   reiser4_key_t *key) 
{
    aal_assert("umka-1207", item != NULL, return -1);
    aal_assert("umka-1208", key != NULL, return -1);

    return plugin_call(return 0, item->node->plugin->node_ops,
					   get_key, item->node, item->pos, key);
}

static reiser4_plugin_t stat40_plugin = {
    .item_ops = {
		.h = {
			.handle = NULL,
			.id = ITEM_STATDATA40_ID,
			.group = STATDATA_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "stat40",
			.desc = "Stat data for reiserfs 4.0, ver. " VERSION,
		},
		.open       = stat40_open,
		
#ifndef ENABLE_COMPACT
        .init		= stat40_init,
        .estimate	= stat40_estimate,
        .insert		= stat40_insert,
        .remove		= stat40_remove,
        .check		= stat40_check,
#else
        .init		= NULL,
        .estimate	= NULL,
        .insert		= NULL,
        .remove		= NULL,
        .check		= NULL,
#endif
        .lookup		= NULL,
		.shift      = NULL,
	    
        .count		= stat40_count,
        .valid		= stat40_valid,
        .print		= stat40_print,
        
		.max_poss_key	= stat40_max_poss_key,
        .max_real_key   = stat40_max_poss_key,
	
		.specific = {}
    }
};

static reiser4_plugin_t *stat40_start(reiser4_core_t *c) {
    core = c;
    return &stat40_plugin;
}

plugin_register(stat40_start);

