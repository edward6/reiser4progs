/*
    stat40.c -- reiser4 default stat data plugin.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include "stat40.h"

static reiser4_core_t *core = NULL;

static stat40_t *stat40_body(reiser4_item_t *item) {

    if (item == NULL) return NULL;
    
    return (stat40_t *)plugin_call(return NULL, 
	item->node->plugin->node_ops, item_body, item->node, item->pos);
}

#ifndef ENABLE_COMPACT

static errno_t stat40_init(reiser4_item_t *item, 
    reiser4_item_hint_t *hint)
{
    uint8_t i;
    stat40_t *stat;
    reiser4_body_t *extbody;
    reiser4_statdata_hint_t *stat_hint;
    
    aal_assert("vpf-076", item != NULL, return -1); 
    aal_assert("vpf-075", hint != NULL, return -1);
    
    stat = stat40_body(item);
    stat_hint = (reiser4_statdata_hint_t *)hint->hint;
    
    st40_set_extmask(stat, stat_hint->extmask);
 
    if (!stat_hint->extmask)
	return 0;
    
    extbody = ((void *)stat) + sizeof(stat40_t);
	
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
	if (((uint64_t)1 << i) & (uint64_t)(((uint64_t)1 << 0xf) | 
	    ((uint64_t)1 << 0x1f) | ((uint64_t)1 << 0x2f))) 
	{
	    extbody = (void *)extbody + sizeof(d16_t);
	    continue;
	}
	    
	if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
	    aal_exception_warn("Can't find stat data extention plugin "
	        "by its id 0x%x.", i);
	    continue;
	}
	
	plugin_call(return -1, plugin->sdext_ops, init, extbody, 
	    stat_hint->ext.hint[i]);
	
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
	
	if (((uint64_t)1 << i) & (((uint64_t)1 << 0xf) | 
	    ((uint64_t)1 << 0x1f) | ((uint64_t)1 << 0x2f))) 
	{
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

/* here w eprobably should check all stat data extention masks */
static errno_t stat40_valid(reiser4_item_t *item) {
    aal_assert("umka-1007", item != NULL, return -1);
    return 0;
}

/* This function returns stat data extention count */
static uint32_t stat40_count(reiser4_item_t *item) {
    stat40_t *stat;
    uint64_t extmask;
    uint8_t i, count = 0;

    aal_assert("umka-1197", item != NULL, return 0);
    
    stat = stat40_body(item);
    extmask = st40_get_extmask(stat);
    
    for (i = 0; i < sizeof(uint64_t)*8; i++) {
	    
	if (((uint64_t)1 << i) & (((uint64_t)1 << 0xf) | 
		((uint64_t)1 << 0x1f) | ((uint64_t)1 << 0x2f)))
	    continue;
	
	count += (((uint64_t)1 << i) & extmask);
    }
    
    return count;
}

static reiser4_body_t *stat40_extbody(reiser4_item_t *item, 
    uint8_t bit)
{
    uint8_t i;
    stat40_t *stat;
    uint64_t extmask;
    reiser4_body_t *extbody;
   
    aal_assert("umka-1191", item != NULL, return NULL);

    stat = stat40_body(item);
    extbody = ((void *)stat) + sizeof(stat40_t);
    
    extmask = st40_get_extmask(stat);
    
    for (i = 0; i < bit; i++) {
        reiser4_plugin_t *plugin;
	
        if (!(((uint64_t)1 << i) & extmask))
	   continue;
	    
	if (((uint64_t)1 << i) & (((uint64_t)1 << 0xf) | 
	    ((uint64_t)1 << 0x1f) | ((uint64_t)1 << 0x2f))) 
	{
	    extbody = (void *)extbody + sizeof(d16_t);
	    i++;
	}
		
	if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
	    aal_exception_warn("Can't find stat data extention plugin "
	        "by its id 0x%x.", i);
	    continue;
	}
	
	extbody = (void *)extbody + plugin_call(return NULL, 
	    plugin->sdext_ops, length,);
    }

    return extbody;
}

static errno_t stat40_open_sdext(reiser4_item_t *item, 
    uint8_t bit, stat40_sdext_t *sdext)
{
    stat40_t *stat;
    uint64_t extmask;
    
    aal_assert("umka-1193", item != NULL, return -1);
    aal_assert("umka-1194", sdext != NULL, return -1);

    stat = stat40_body(item);
    extmask = st40_get_extmask(stat);

    if (!(((uint64_t)1 << bit) & extmask)) {
	aal_exception_error("Stat data extention 0x%x "
	    "is not present.", bit);
	return -1;
    }
    
    if (!(sdext->plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, bit))) {
	aal_exception_error("Can't find stat data extention plugin "
	    "by its id %x.", bit);
	return -1;
    }
    
    return -((sdext->body = stat40_extbody(item, bit)) == NULL);
}

static uint16_t stat40_get_mode(reiser4_item_t *item) {
    stat40_sdext_t sdext;
    reiser4_sdext_lw_hint_t hint;
    
    aal_assert("umka-710", item != NULL, return 0);
    
    if (stat40_open_sdext(item, SDEXT_LW_ID, &sdext))
	return 0;
    
    if (plugin_call(return 0, sdext.plugin->sdext_ops, open, 
	sdext.body, &hint)) 
    {
	aal_exception_error("Can't open light weight stat data "
	    "extention.");
	
	return 0;
    }
    
    return hint.mode;
}

static uint32_t stat40_get_size(reiser4_item_t *item) {
    stat40_sdext_t sdext;
    reiser4_sdext_lw_hint_t hint;
    
    aal_assert("umka-1223", item != NULL, return 0);
    
    if (stat40_open_sdext(item, SDEXT_LW_ID, &sdext))
	return 0;
    
    if (plugin_call(return 0, sdext.plugin->sdext_ops, open, 
	sdext.body, &hint)) 
    {
	aal_exception_error("Can't open light weight stat data "
	    "extention.");
	
	return 0;
    }
    
    return hint.size;
}

#ifndef ENABLE_COMPACT

static errno_t stat40_set_mode(reiser4_item_t *item, 
    uint16_t mode)
{
    stat40_sdext_t sdext;
    reiser4_sdext_lw_hint_t hint;
    
    aal_assert("umka-1192", item != NULL, return 0);
    
    if (stat40_open_sdext(item, SDEXT_LW_ID, &sdext))
	return 0;
    
    if (plugin_call(return 0, sdext.plugin->sdext_ops, open, 
	sdext.body, &hint)) 
    {
	aal_exception_error("Can't open light weight stat data "
	    "extention.");
	
	return -1;
    }
    
    hint.mode = mode;
    
    if (plugin_call(return 0, sdext.plugin->sdext_ops, init, 
	sdext.body, &hint)) 
    {
	aal_exception_error("Can't update light weight stat data "
	    "extention.");
	return -1;
    }
    
    return 0;
}

static errno_t stat40_set_size(reiser4_item_t *item, 
    uint32_t size)
{
    stat40_sdext_t sdext;
    reiser4_sdext_lw_hint_t hint;
    
    aal_assert("umka-1224", item != NULL, return 0);
    
    if (stat40_open_sdext(item, SDEXT_LW_ID, &sdext))
	return 0;
    
    if (plugin_call(return 0, sdext.plugin->sdext_ops, open, 
	sdext.body, &hint)) 
    {
	aal_exception_error("Can't open light weight stat data "
	    "extention.");
	
	return -1;
    }
    
    hint.size = size;
    
    if (plugin_call(return 0, sdext.plugin->sdext_ops, init, 
	sdext.body, &hint)) 
    {
	aal_exception_error("Can't update light weight stat data "
	    "extention.");
	return -1;
    }
    
    return 0;
}

#endif

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
	    .type = ITEM_PLUGIN_TYPE,
	    .label = "stat40",
	    .desc = "Stat data for reiserfs 4.0, ver. " VERSION,
	},
	.type = STATDATA_ITEM_TYPE,
	
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
        .print		= NULL,
	    
        .max_poss_key	= stat40_max_poss_key,
        .count		= stat40_count,
        .valid		= stat40_valid,
        .max_real_key   = stat40_max_poss_key,
	
	.specific = {
	    .statdata = {
		.get_mode = stat40_get_mode,
		.get_size = stat40_get_size,
#ifndef ENABLE_COMPACT
		.set_mode = stat40_set_mode,
		.set_size = stat40_set_size
#else
		.set_mode = NULL,
		.set_size = NULL
#endif
	    }
	}
    }
};

static reiser4_plugin_t *stat40_start(reiser4_core_t *c) {
    core = c;
    return &stat40_plugin;
}

plugin_register(stat40_start);

