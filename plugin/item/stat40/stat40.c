/*
  stat40.c -- reiser4 default stat data plugin.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "stat40.h"
#include <sys/stat.h>

#include <aux/aux.h>

static reiser4_core_t *core = NULL;

#define stat40_body(item) ((stat40_t *)item->body)

/* Type for stat40 layout callback function */
typedef int (*stat40_ext_func_t) (uint8_t, reiser4_plugin_t *, uint16_t,
				  rbody_t *, void *);

/*
  The function which implements stat40 layout pass. This function is used for
  all statdata extention-related actions. For example for opening, of counting.
*/
static errno_t stat40_traverse(item_entity_t *item,
			       stat40_ext_func_t func,
			       void *data)
{
	uint8_t i;
	stat40_t *stat;
	uint16_t extmask;
	
	rbody_t *extbody;
	reiser4_plugin_t *plugin;

	aal_assert("umka-1197", item != NULL);
    
	stat = stat40_body(item);
	extmask = st40_get_extmask(stat);
	extbody = (void *)stat + sizeof(stat40_t);

	/*
	  Loop though the all possible extentions and calling passed @func for
	  each of them if corresponing extention exists.
	*/
	for (i = 0; i < sizeof(uint64_t) * 8; i++) {
		errno_t res;

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

		/* If extention is not present, we going to the next one */
		if (!(((uint64_t)1 << i) & extmask))
			continue;

		/* Getting extention plugin from the plugin factory */
		if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			return 0;
		}

		/*
		  Okay, extention is present, calling callback fucntion for it
		  and if result is not good, returning it to teh caller.
		*/
		if (!(res = func(i, plugin, extmask, extbody, data)))
			return res;

		/* Calculating the pointer to the next extention body */
		extbody = (void *)extbody + plugin_call(plugin->sdext_ops,
							length, extbody);
	}
    
	return 0;
}

/* Callback for opening one extention */
static errno_t callback_open_ext(uint8_t ext, reiser4_plugin_t *plugin,
				 uint16_t extmask, rbody_t *extbody,
				 void *data)
{
	reiser4_statdata_hint_t *hint;

	hint = ((reiser4_item_hint_t *)data)->hint;

	/* Reading mask into hint */
	if ((ext + 1) % 16 == 0) {
		hint->extmask <<= 16;
		hint->extmask |= extmask;
	}

	/* We load @ext if its hint present in item hint */
	if (hint->ext[ext]) {

		/* Calling loading the corresponding statdata extention */
		if (plugin_call(plugin->sdext_ops, open, extbody, hint->ext[ext]))
			return -1;
	}
	
	return 1;
}

/* Fetches whole statdata item with extentions into passed @buff */
static int32_t stat40_read(item_entity_t *item, void *buff,
			   uint32_t pos, uint32_t count)
{
	aal_assert("umka-1414", item != NULL);
	aal_assert("umka-1415", buff != NULL);

	if (stat40_traverse(item, callback_open_ext, buff))
		return -1;

	return 1;
}

#ifndef ENABLE_ALONE

/* Prepares item area */
static errno_t stat40_init(item_entity_t *item) {
	aal_assert("umka-1670", item != NULL);
	
	aal_memset(item->body, 0, item->len);
	return 0;
}

/*
  Estimates how many bytes will be needed for creating statdata item described
  by passed @hint at passed @pos.
*/
static errno_t stat40_estimate(item_entity_t *item, void *buff,
			       uint32_t pos, uint32_t count) 
{
	uint8_t i;
	reiser4_item_hint_t *hint;
	reiser4_statdata_hint_t *stat_hint;
    
	aal_assert("vpf-074", buff != NULL);

	hint = (reiser4_item_hint_t *)buff;
	stat_hint = (reiser4_statdata_hint_t *)hint->hint;

	hint->len = sizeof(stat40_t);
    
	if (!stat_hint->extmask) {
		aal_exception_warn("Empty extention mask detected "
				   "while estimating stat data.");
		return 0;
	}
    
	/* Estimating the all stat data extentions */
	for (i = 0; i < sizeof(uint64_t) * 8; i++) {
		reiser4_plugin_t *plugin;

		/* Check if extention is present in mask */
		if (!(((uint64_t)1 << i) & stat_hint->extmask))
			continue;

		/*
		  If we are on the extention which is multiple of 16 (each mask
		  has 16 bits) then we add to hint's len the size of next mask.
		*/
		if ((i + 1) % 16 == 0) {
			hint->len += sizeof(d16_t);
			continue;
		}

		/* Getting extention plugin */
		if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			continue;
		}

		/*
		  Calculating length of the corresponding extention and add it
		  to the estimated value.
		*/
		hint->len += plugin_call(plugin->sdext_ops, length,
					 stat_hint->ext[i]);
	}
	
	return 0;
}

/* This method inserts the stat data extentions */
static int32_t stat40_write(item_entity_t *item, void *buff,
			    uint32_t pos, uint32_t count)
{
	uint8_t i;
	rbody_t *extbody;

	reiser4_item_hint_t *hint;
	reiser4_statdata_hint_t *stat_hint;
    
	aal_assert("vpf-076", item != NULL); 
	aal_assert("vpf-075", buff != NULL);

	extbody = (rbody_t *)item->body;

	hint = (reiser4_item_hint_t *)buff;
	stat_hint = (reiser4_statdata_hint_t *)hint->hint;
    
	if (!stat_hint->extmask)
		return 0;
    
	for (i = 0; i < sizeof(uint64_t) * 8; i++) {
		reiser4_plugin_t *plugin;

		/* Check if extention is present */
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

		/* Getting extention plugin */
		if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			continue;
		}

		/* Initializing extention data at passed area */
		plugin_call(plugin->sdext_ops, init, extbody, stat_hint->ext[i]);
	
		/* 
		   Getting pointer to the next extention. It is evaluating as
		   the previous pointer plus its size.
		*/
		extbody += plugin_call(plugin->sdext_ops, length, extbody);
	}
    
	return count;
}

extern errno_t stat40_check(item_entity_t *);

#endif

/* Here we probably should check all stat data extention masks */
static errno_t stat40_valid(item_entity_t *item) {
	aal_assert("umka-1007", item != NULL);
	return 0;
}

/*
  This function returns unit count. This value must be 1 if item has not
  units. It is because balancing code assumes that if item has more than one
  unit the it may be shifted out. That is because w ecan't return the number of
  extentions here. Extentions are the statdata private bussiness.
*/
static uint32_t stat40_units(item_entity_t *item) {
	return 1;
}

/* Helper structrure for keeping track of stat data extention body */
struct body_hint {
	rbody_t *body;
	uint8_t ext;
};

/* Callback function for finding stat data extention body by bit */
static int callback_body_ext(uint8_t ext, reiser4_plugin_t *plugin,
			     uint16_t extmask, rbody_t *extbody,
			     void *data)
{
	struct body_hint *hint = (struct body_hint *)data;

	hint->body = extbody;
	return (ext < hint->ext);
}

/* Finds extention body by number of bit in 64bits mask */
static rbody_t *stat40_sdext_body(item_entity_t *item, 
					 uint8_t bit)
{
	struct body_hint hint = {NULL, bit};

	if (stat40_traverse(item, callback_body_ext, &hint) < 0)
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
static int callback_present_ext(uint8_t ext, reiser4_plugin_t *plugin,
				uint16_t extmask, rbody_t *extbody,
				void *data)
{
	struct present_hint *hint = (struct present_hint *)data;
	
	hint->present = (ext == hint->ext);
	return !hint->present;
}

/* Determines if passed extention denoted by @bit present in statdata item */
static int stat40_sdext_present(item_entity_t *item, 
				uint8_t bit)
{
	struct present_hint hint = {0, bit};

	if (!stat40_traverse(item, callback_present_ext, &hint) < 0)
		return 0;

	return hint.present;
}

#ifndef ENABLE_ALONE

/* Callback for counting the number of stat data extentions in use */
static int callback_count_ext(uint8_t ext, reiser4_plugin_t *plugin,
			      uint16_t extmask, rbody_t *extbody,
			      void *data)
{
        (*(uint32_t *)data)++;
        return 1;
}

/* This function returns stat data extention count */
static uint32_t stat40_sdexts(item_entity_t *item) {
        uint32_t count = 0;

        if (stat40_traverse(item, callback_count_ext, &count) < 0)
                return 0;

        return count;
}

/* Prints extention into @stream */
static int callback_print_ext(uint8_t ext, reiser4_plugin_t *plugin,
			      uint16_t extmask, rbody_t *extbody,
			      void *data)
{
	aal_stream_t *stream = (aal_stream_t *)data;

	if (ext == 0 || (ext + 1) % 16 == 0)
		aal_stream_format(stream, "mask:\t\t0x%x\n", extmask);
				
	aal_stream_format(stream, "label:\t\t%s\n", plugin->h.label);
	aal_stream_format(stream, "plugin:\t\t%s\n", plugin->h.desc);
	
	plugin_call(plugin->sdext_ops, print, extbody, stream, 0);
	
	return 1;
}

/* Prints stat data item into passed @stream */
static errno_t stat40_print(item_entity_t *item,
			    aal_stream_t *stream,
			    uint16_t options)
{
	aal_assert("umka-1407", item != NULL);
	aal_assert("umka-1408", stream != NULL);
    
	aal_stream_format(stream, "STATDATA: len=%u, KEY: ", item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print, &item->key,
			stream, options))
		return -1;
	
	aal_stream_format(stream, " PLUGIN: 0x%x (%s)\n",
			  item->plugin->h.id, item->plugin->h.label);

	aal_stream_format(stream, "count:\t\t%u\n", stat40_sdexts(item));

	if (stat40_traverse(item, callback_print_ext, (void *)stream) < 0)
		return -1;

	return 0;
}

#endif

/*
  Returns plugin of the file stat data item belongs to. In doing so, this
  function should discover all extetions first in order to find is some
  non-standard file plugin is in use. And then if it was not found, it should
  try determine file plugin by means of st_mode field inside ligh weight
  extention.
*/
static reiser4_plugin_t *stat40_belongs(item_entity_t *item) {
	uint32_t pid;
	uint64_t extmask;
	reiser4_item_hint_t hint;
	reiser4_statdata_hint_t stat;
	reiser4_sdext_lw_hint_t lw_hint;
	
	/*
	  Traverse all statdata extentions and try to find out a non-standard
	  file plugin. If it is not found, we detect file plugin by mode field.
	*/
	extmask = st40_get_extmask(stat40_body(item));

	/* FIXME-UMKA: Here should be checking for the extention first */
	if (!(((uint64_t)1 << SDEXT_LW_ID) & extmask))
		return NULL;

	aal_memset(&hint, 0, sizeof(hint));
	aal_memset(&stat, 0, sizeof(stat));
	
	hint.hint = &stat;
	stat.ext[SDEXT_LW_ID] = &lw_hint;
	
	if (stat40_read(item, &hint, 0, 1) != 1) {
		aal_exception_error("Can't open statdata extention (0x%x)",
				    SDEXT_LW_ID);
		return NULL;
	}

	/* Inspecting st_mode field */
	pid = FILE_SPECIAL40_ID;
	
	if (S_ISLNK(lw_hint.mode))
		pid = FILE_SYMLINK40_ID;
	else if (S_ISREG(lw_hint.mode))
		pid = FILE_REGULAR40_ID;
	else if (S_ISDIR(lw_hint.mode))
		pid = FILE_DIRTORY40_ID;

	/* Finding plugin by found id */
	return core->factory_ops.ifind(FILE_PLUGIN_TYPE, pid);
}

/* Stat data plugin preparing */
static reiser4_plugin_t stat40_plugin = {
	.item_ops = {
		.h = {
			.handle = empty_handle,
			.id = ITEM_STATDATA40_ID,
			.group = STATDATA_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "stat40",
			.desc = "Stat data for reiserfs 4.0, ver. " VERSION,
		},
		
#ifndef ENABLE_ALONE
		.estimate	= stat40_estimate,
		.write		= stat40_write,
		.init		= stat40_init,
		.check		= stat40_check,
		.print		= stat40_print,
#else
		.estimate	= NULL,
		.write		= NULL,
		.init		= NULL,
		.check		= NULL,
		.print		= NULL,
#endif
		.layout         = NULL,
		.remove		= NULL,
		.lookup		= NULL,
		.insert         = NULL,
		.mergeable      = NULL,
	    
		.shift          = NULL,
		.predict        = NULL,
		.branch         = NULL,

		.read           = stat40_read,
		.units		= stat40_units,
		.valid		= stat40_valid,
		.belongs        = stat40_belongs,
        
		.get_key	= NULL,
		.set_key	= NULL,
		
		.maxposs_key	= NULL,
		.utmost_key     = NULL,
		.gap_key	= NULL,
		.layout_check	= NULL
	}
};

static reiser4_plugin_t *stat40_start(reiser4_core_t *c) {
	core = c;
	return &stat40_plugin;
}

plugin_register(stat40_start, NULL);

