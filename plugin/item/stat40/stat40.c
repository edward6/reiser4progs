/*
  stat40.c -- reiser4 default stat data plugin.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include "stat40.h"
#include <aux/aux.h>

static reiser4_core_t *core = NULL;

#define stat40_body(item) ((stat40_t *)item->body)

/*
  The function which implements stat40 layout pass. This function is used for
  all statdata extention-related actions. For example for opening, of counting.
*/
errno_t stat40_traverse(item_entity_t *item,
			stat40_ext_func_t ext_func,
			void *data)
{
	uint16_t i;
	uint16_t len;
	stat40_t *stat;
	sdext_entity_t sdext;

	uint16_t chunks = 0;
	uint16_t extmask = 0;

	aal_assert("umka-1197", item != NULL);
	aal_assert("umka-2059", ext_func != NULL);
    
	stat = stat40_body(item);

	sdext.offset = 0;
	sdext.body = item->body;

	/*
	  Loop though the all possible extentions and calling passed @ext_func
	  for each of them if corresponing extention exists.
	*/
	for (i = 0; i < sizeof(uint64_t) * 8; i++) {
		errno_t res;

		if (i == 0 || ((i + 1) % 16 == 0)) {

			if (i > 0) {
				if (!((1 << (i - chunks)) & extmask))
					break;
			}
			
			extmask = *((uint16_t *)sdext.body);

			/* Clear the last bit in last mask */
			if ((1 << (i - chunks)) & 0x2f) {
				if (!(extmask & 0x8000))
					extmask &= ~0x8000;
			}

			chunks++;
			sdext.body += sizeof(d16_t);
			sdext.offset += sizeof(d16_t);
		}

		/* If extention is not present, we going to the next one */
		if (!((1 << (i - (chunks - 1))) & extmask))
			continue;

		/* Getting extention plugin from the plugin factory */
		if (!(sdext.plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			return 0;
		}

		/*
		  Okay, extention is present, calling callback function for it
		  and if result is not good, returning it to teh caller.
		*/
		if ((res = ext_func(&sdext, extmask, data)))
			return res;

		len = plugin_call(sdext.plugin->sdext_ops, length, 
				  sdext.body);

		/* Calculating the pointer to the next extention body */
		sdext.body += len;
		sdext.offset += len;
	}
    
	return 0;
}

/* Callback for opening one extention */
static errno_t callback_open_ext(sdext_entity_t *sdext,
				 uint16_t extmask, 
				 void *data)
{
	create_hint_t *hint;
	statdata_hint_t *stat_hint;

	hint = (create_hint_t *)data;
	stat_hint = hint->type_specific;

	/* Reading mask into hint */
	stat_hint->extmask |= ((uint64_t)1 << sdext->plugin->h.id);

	/* We load @ext if its hint present in item hint */
	if (stat_hint->ext[sdext->plugin->h.id]) {
		void *sdext_hint = stat_hint->ext[sdext->plugin->h.id]; 

		return plugin_call(sdext->plugin->sdext_ops, open,
				   sdext->body, sdext_hint);
	}
	
	return 0;
}

/* Fetches whole statdata item with extentions into passed @buff */
static int32_t stat40_read(item_entity_t *item, void *buff,
			   uint32_t pos, uint32_t count)
{
	errno_t res;
	
	aal_assert("umka-1414", item != NULL);
	aal_assert("umka-1415", buff != NULL);

	if ((res = stat40_traverse(item, callback_open_ext, buff)))
		return res;

	return 1;
}

static int stat40_data(void) {
	return 1;
}

#ifndef ENABLE_STAND_ALONE
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
static errno_t stat40_estimate(item_entity_t *item, uint32_t pos,
			       uint32_t count, create_hint_t *hint) 
{
	uint16_t i;
	statdata_hint_t *stat_hint;
    
	aal_assert("vpf-074", hint != NULL);
	
	hint->len = sizeof(stat40_t);
	stat_hint = (statdata_hint_t *)hint->type_specific;
    
	if (!stat_hint->extmask) {
		aal_exception_warn("Empty extention mask is detected "
				   "while estimating stat data.");
		return 0;
	}
    
	/* Estimating the all stat data extentions */
	for (i = 0; i < sizeof(uint64_t) * 8; i++) {
		reiser4_plugin_t *plugin;

		/* Check if extention is present in mask */
		if (!(((uint64_t)1 << i) & stat_hint->extmask))
			continue;

		aal_assert("vpf-773", stat_hint->ext[i] != NULL);
		
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

/* This method writes the stat data extentions */
static errno_t stat40_insert(item_entity_t *item,
			     create_hint_t *hint,
			     uint32_t pos)
{
	uint16_t i;
	body_t *extbody;
	statdata_hint_t *stat_hint;
    
	aal_assert("vpf-076", item != NULL); 
	aal_assert("vpf-075", hint != NULL);

	extbody = (body_t *)item->body;
	stat_hint = (statdata_hint_t *)hint->type_specific;
    
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
		if (i % 16 == 0) {
			uint16_t extmask;

			extmask = (stat_hint->extmask >> i) &
				0x000000000000ffff;
			
			st40_set_extmask((stat40_t *)extbody, extmask);
			extbody = (void *)extbody + sizeof(d16_t);
		}

		/* Getting extention plugin */
		if (!(plugin = core->factory_ops.ifind(SDEXT_PLUGIN_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			continue;
		}

		/* Initializing extention data at passed area */
		if (stat_hint->ext[i]) {
			plugin_call(plugin->sdext_ops, init, extbody,
				    stat_hint->ext[i]);
		}
	
		/* 
		   Getting pointer to the next extention. It is evaluating as
		   the previous pointer plus its size.
		*/
		extbody += plugin_call(plugin->sdext_ops, length, extbody);
	}
    
	return 0;
}

extern errno_t stat40_check(item_entity_t *, uint8_t);

#endif

/*
  This function returns unit count. This value must be 1 if item has not
  units. It is because balancing code assumes that if item has more than one
  unit the it may be shifted out. That is because w ecan't return the number of
  extentions here. Extentions are the statdata private bussiness.
*/
static uint32_t stat40_units(item_entity_t *item) {
	return 1;
}

#ifndef ENABLE_STAND_ALONE

/* Helper structrure for keeping track of stat data extention body */
struct body_hint {
	body_t *body;
	uint8_t ext;
};

typedef struct body_hint body_hint_t;

/* Callback function for finding stat data extention body by bit */
static errno_t callback_body_ext(sdext_entity_t *sdext,
				 uint16_t extmask, 
				 void *data)
{
	body_hint_t *hint = (body_hint_t *)data;

	hint->body = sdext->body;
	return -(sdext->plugin->h.id >= hint->ext);
}

/* Finds extention body by number of bit in 64bits mask */
static body_t *stat40_sdext_body(item_entity_t *item, 
					 uint8_t bit)
{
	struct body_hint hint = {NULL, bit};

	if (stat40_traverse(item, callback_body_ext, &hint) < 0)
		return NULL;
	
	return hint.body;
}

/*
  Helper structure for keeping track of presence of a stat data
  extention.
*/
struct present_hint {
	int present;
	uint8_t ext;
};

typedef struct present_hint present_hint_t;

/*
  Callback for getting presence information for certain stat data
  extention.
*/
static errno_t callback_present_ext(sdext_entity_t *sdext,
				    uint16_t extmask, 
				    void *data)
{
	present_hint_t *hint = (present_hint_t *)data;

	hint->present = (sdext->plugin->h.id == hint->ext);
	return hint->present;
}

/* Determines if passed extention denoted by @bit present in statdata item */
static int stat40_sdext_present(item_entity_t *item, 
				uint8_t bit)
{
	present_hint_t hint = {0, bit};

	if (!stat40_traverse(item, callback_present_ext, &hint) < 0)
		return 0;

	return hint.present;
}

/* Callback for counting the number of stat data extentions in use */
static errno_t callback_count_ext(sdext_entity_t *sdext,
				  uint16_t extmask, 
				  void *data)
{
        (*(uint32_t *)data)++;
        return 0;
}

/* This function returns stat data extention count */
static uint32_t stat40_sdext_count(item_entity_t *item) {
        uint32_t count = 0;

        if (stat40_traverse(item, callback_count_ext, &count) < 0)
                return 0;

        return count;
}

/* Prints extention into @stream */
static errno_t callback_print_ext(sdext_entity_t *sdext,
				  uint16_t extmask, 
				  void *data)
{
	int print_mask;
	uint16_t length;
	aal_stream_t *stream = (aal_stream_t *)data;

	print_mask = (sdext->plugin->h.id == 0 ||
		      (sdext->plugin->h.id + 1) % 16 == 0);
	
	if (print_mask)	{
		aal_stream_format(stream, "mask:\t\t0x%x\n",
				  extmask);
	}
				
	aal_stream_format(stream, "label:\t\t%s\n",
			  sdext->plugin->h.label);
	
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  sdext->plugin->h.desc);
	
	aal_stream_format(stream, "offset:\t\t%u\n",
			  sdext->offset);
	
	length = plugin_call(sdext->plugin->sdext_ops,
			     length, sdext->body);
	
	aal_stream_format(stream, "len:\t\t%u\n", length);
	
	plugin_call(sdext->plugin->sdext_ops, print,
		    sdext->body, stream, 0);
	
	return 0;
}

/* Prints stat data item into passed @stream */
static errno_t stat40_print(item_entity_t *item,
			    aal_stream_t *stream,
			    uint16_t options)
{
	aal_assert("umka-1407", item != NULL);
	aal_assert("umka-1408", stream != NULL);
    
	aal_stream_format(stream, "STATDATA PLUGIN=%s LEN=%u, KEY=",
			  item->plugin->h.label, item->len);
		
	if (plugin_call(item->key.plugin->key_ops, print,
			&item->key, stream, options))
	{
		return -EINVAL;
	}
	
	aal_stream_format(stream, " UNITS=1\n");

	aal_stream_format(stream, "exts:\t\t%u\n",
			  stat40_sdext_count(item));

	return stat40_traverse(item, callback_print_ext,
			       (void *)stream);
}

static errno_t stat40_feel(item_entity_t *item,
			   key_entity_t *start,
			   key_entity_t *end,
			   copy_hint_t *hint)
{
	aal_assert("umka-2151", item != NULL);
	aal_assert("umka-2153", hint != NULL);

	hint->count = 1;
	hint->len = item->len;
	
	return 0;
}

static errno_t stat40_copy(item_entity_t *dst_item,
			   uint32_t dst_pos,
			   item_entity_t *src_item,
			   uint32_t src_pos,
			   key_entity_t *start,
			   key_entity_t *end,
			   copy_hint_t *hint)
{
	aal_assert("umka-2144", hint != NULL);
	aal_assert("umka-2142", dst_item != NULL);
	aal_assert("umka-2145", src_item != NULL);
	
	aal_memcpy(dst_item->body, src_item->body,
		   hint->len);
	
	return 0;
}
#endif

/* Stat data plugin preparing */
static reiser4_plugin_t stat40_plugin = {
	.item_ops = {
		.h = {
			.handle = EMPTY_HANDLE,
			.id = ITEM_STATDATA40_ID,
			.group = STATDATA_ITEM,
			.type = ITEM_PLUGIN_TYPE,
			.label = "stat40",
#ifndef ENABLE_STAND_ALONE
			.desc = "Stat data item for reiser4, ver. " VERSION
#else
			.desc = ""
#endif
		},
		
#ifndef ENABLE_STAND_ALONE
		.estimate	= stat40_estimate,
		.feel           = stat40_feel,
		.copy           = stat40_copy,
		.insert		= stat40_insert,
		.init		= stat40_init,
		.check		= stat40_check,
		.print		= stat40_print,
		
		.write          = NULL,
		.layout         = NULL,
		.remove		= NULL,
		.shift          = NULL,
		.predict        = NULL,
		.set_key	= NULL,
		.layout_check	= NULL,
		.maxreal_key    = NULL,
		.gap_key	= NULL,
#endif
		.data		= stat40_data,
		.read           = stat40_read,
		.units		= stat40_units,
        
		.lookup		= NULL,
		.branch         = NULL,
		.mergeable      = NULL,

		.maxposs_key	= NULL,
		.get_key	= NULL
	}
};

static reiser4_plugin_t *stat40_start(reiser4_core_t *c) {
	core = c;
	return &stat40_plugin;
}

plugin_register(stat40, stat40_start, NULL);

