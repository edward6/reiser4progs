/*
    stat40.c -- reiser4 default stat data plugin.
    Copyright (C) 1996-2002 Hans Reiser.
    Author Yury Umanets.
*/

#include "stat40.h"

static reiser4_core_t *core = NULL;

static errno_t stat40_confirm(reiser4_body_t *body) {
    aal_assert("umka-1008", body != NULL, return -1);
    return 0;
}

#ifndef ENABLE_COMPACT

static errno_t stat40_init(reiser4_body_t *body, 
    reiser4_item_hint_t *hint)
{
    uint8_t i;
    stat40_t *stat;
    
    reiser4_body_t *extention;
    reiser4_statdata_hint_t *stat_hint;
    
    aal_assert("vpf-076", body != NULL, return -1); 
    aal_assert("vpf-075", hint != NULL, return -1);
    
    stat = (stat40_t *)body;
    stat_hint = (reiser4_statdata_hint_t *)hint->hint;
    
    st40_set_extmask(stat, stat_hint->extmask);
 
    if (!stat_hint->extmask)
	return 0;
    
    extention = ((void *)stat) + sizeof(stat40_t);
	
    for (i = 0; i < sizeof(uint64_t)*8; i++) {
	reiser4_plugin_t *plugin;
	
	if (!(((uint64_t)1 << i) & stat_hint->extmask))
	    continue;
	    
	if (!(plugin = core->factory_ops.plugin_ifind(SDEXT_PLUGIN_TYPE, i))) {
	    aal_exception_warn("Can't find stat data extention plugin "
	        "by its id 0x%x.", i);
	    continue;
	}
	
	plugin_call(return -1, plugin->sdext_ops, init, extention, 
	    stat_hint->extentions.hint[i]);
	
	/* 
	    Getting pointer to the next extention. It is evaluating as previous 
	    pointer plus its size.
	*/
	extention += plugin_call(return -1, plugin->sdext_ops, length,);
    }
    
    return 0;
}

static errno_t stat40_estimate(uint32_t pos, 
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
    for (i = 0; i < sizeof(uint64_t)*8; i++) {
        reiser4_plugin_t *plugin;
	
        if (!(((uint64_t)1 << i) & stat_hint->extmask))
	   continue;
	    
	if (!(plugin = core->factory_ops.plugin_ifind(SDEXT_PLUGIN_TYPE, i))) {
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
static errno_t stat40_insert(reiser4_body_t *body, 
    uint32_t pos, reiser4_item_hint_t *hint)
{
    return -1;
}

/* This method deletes the stat data extentions */
static uint16_t stat40_remove(reiser4_body_t *body, 
    uint32_t pos)
{
    return -1;
}

extern errno_t stat40_check(reiser4_body_t *, uint16_t);

#endif

static errno_t stat40_valid(reiser4_body_t *body) {
    aal_assert("umka-1007", body != NULL, return -1);
    return 0;
}

/* This function returns stat data extention count */
static uint32_t stat40_count(reiser4_body_t *body) {
    uint64_t extmask;
    uint8_t i, count = 0;

    extmask = st40_get_extmask((stat40_t *)body);
    
    for (i = 0; i < sizeof(uint64_t)*8; i++)
	count += (((uint64_t)1 << i) & extmask);
    
    return count;
}

static reiser4_body_t *stat40_extbody(reiser4_body_t *body, 
    uint8_t bit)
{
    uint8_t i;
    uint64_t extmask;
    reiser4_body_t *extbody;
   
    aal_assert("umka-1191", body != NULL, return NULL);
    
    extbody = ((void *)body) + sizeof(stat40_t);
    extmask = st40_get_extmask((stat40_t *)body);
    
    for (i = 0; i < bit; i++) {
        reiser4_plugin_t *plugin;
	
        if (!(((uint64_t)1 << i) & extmask))
	   continue;
	    
	if (!(plugin = core->factory_ops.plugin_ifind(SDEXT_PLUGIN_TYPE, i))) {
	    aal_exception_warn("Can't find stat data extention plugin "
	        "by its id 0x%x.", i);
	    continue;
	}
	
	extbody = (void *)extbody + plugin_call(return NULL, 
	    plugin->sdext_ops, length,);
    }

    return extbody;
}

static errno_t stat40_open_sdext(reiser4_body_t *body, 
    uint8_t bit, stat40_sdext_t *sdext)
{
    uint64_t extmask;
    
    aal_assert("umka-1193", body != NULL, return -1);
    aal_assert("umka-1194", sdext != NULL, return -1);

    extmask = st40_get_extmask((stat40_t *)body);

    if (!(((uint64_t)1 << bit) & extmask)) {
	aal_exception_error("Stat data extention 0x%x "
	    "is not present.", bit);
	return -1;
    }
    
    if (!(sdext->plugin = 
	core->factory_ops.plugin_ifind(SDEXT_PLUGIN_TYPE, bit))) 
    {
	aal_exception_error("Can't find stat data extention plugin "
	    "by its id %x.", bit);
	return -1;
    }
    
    return -((sdext->body = stat40_extbody(body, bit)) == NULL);
}

static uint16_t stat40_get_mode(reiser4_body_t *body) {
    stat40_sdext_t sdext;
    reiser4_sdext_lw_hint_t hint;
    
    aal_assert("umka-710", body != NULL, return 0);
    
    if (stat40_open_sdext(body, SDEXT_LW_ID, &sdext))
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

#ifndef ENABLE_COMPACT

static errno_t stat40_set_mode(reiser4_body_t *body, 
    uint16_t mode)
{
    stat40_sdext_t sdext;
    reiser4_sdext_lw_hint_t hint;
    
    aal_assert("umka-1192", body != NULL, return 0);
    
    if (stat40_open_sdext(body, SDEXT_LW_ID, &sdext))
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

#endif

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
        .init	    = stat40_init,
        .estimate   = stat40_estimate,
        .insert	    = stat40_insert,
        .remove	    = stat40_remove,
        .check	    = stat40_check,
#else
        .init	    = NULL,
        .estimate   = NULL,
        .insert	    = NULL,
        .remove	    = NULL,
        .check	    = NULL,
#endif
        .maxkey	    = NULL,
        .lookup	    = NULL,
        .print	    = NULL,
	    
        .count	    = stat40_count,
        .confirm    = stat40_confirm,
        .valid	    = stat40_valid,
	
	.specific = {
	    .statdata = {
		.get_mode = stat40_get_mode,
#ifndef ENABLE_COMPACT
		.set_mode = stat40_set_mode
#else
		.set_mode = NULL
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

