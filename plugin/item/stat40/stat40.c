/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40.c -- reiser4 default stat data plugin. */

#include "stat40.h"
#include "stat40_repair.h"
#include <sys/stat.h>

static reiser4_core_t *core = NULL;

/* The function which implements stat40 layout pass. This function is used for
   all statdata extention-related actions. For example for reading, or
   counting. */
errno_t stat40_traverse(place_t *place, ext_func_t ext_func, void *data) {
	uint16_t i, len;
	uint16_t chunks = 0;
	uint16_t extmask = 0;

	sdext_entity_t sdext;

	aal_assert("umka-1197", place != NULL);
	aal_assert("umka-2059", ext_func != NULL);
    
	sdext.offset = 0;
	sdext.body = place->body;

	/* Loop though the all possible extentions and calling passed @ext_func
	   for each of them if corresponing extention exists. */
	for (i = 0; i < STAT40_EXTNR; i++) {
		errno_t res;

		if (i == 0 || ((i + 1) % 16 == 0)) {

			/* Check if next pack exists. */
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
		if (!(sdext.plug = core->factory_ops.ifind(SDEXT_PLUG_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			return 0;
		}

		/* Okay, extention is present, calling callback function for it
		   and if result is not good, returning it to teh caller. */
		if ((res = ext_func(&sdext, extmask, data)))
			return res;

		len = plug_call(sdext.plug->o.sdext_ops, length, 
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
	trans_hint_t *hint;
	statdata_hint_t *stat_hint;

	/* Method open is not defined, this probably means, we only interested
	   in symlink's length method in order to reach other symlinks body. So,
	   we retrun 0 here. */
	if (!sdext->plug->o.sdext_ops->open)
		return 0;
	
	hint = (trans_hint_t *)data;
	stat_hint = hint->specific;

	/* Reading mask into hint */
	stat_hint->extmask |= ((uint64_t)1 << sdext->plug->id.id);

	/* We load @ext if its hint present in @stat_hint */
	if (stat_hint->ext[sdext->plug->id.id]) {
		void *sdext_hint = stat_hint->ext[sdext->plug->id.id]; 

		return plug_call(sdext->plug->o.sdext_ops, open,
				 sdext->body, sdext_hint);
	}
	
	return 0;
}

/* Fetches whole statdata item with extentions into passed @buff */
static int64_t stat40_fetch_units(place_t *place, trans_hint_t *hint) {
	aal_assert("umka-1415", hint != NULL);
	aal_assert("umka-1414", place != NULL);

	if (stat40_traverse(place, callback_open_ext, hint))
		return -EINVAL;

	return 1;
}

static errno_t stat40_maxposs_key(place_t *place, 
				  key_entity_t *key)
{
	aal_assert("umka-2421", key != NULL);
	aal_assert("umka-2420", place != NULL);
	
	return plug_call(place->key.plug->o.key_ops,
			 assign, key, &place->key);
}

/* This function returns unit count. This value must be 1 if item has not
   units. It is because balancing code assumes that if item has more than one
   unit the it may be shifted out. That is because w ecan't return the number of
   extentions here. Extentions are the statdata private bussiness. */
static uint32_t stat40_units(place_t *place) {
	return 1;
}

#ifndef ENABLE_STAND_ALONE
/* Estimates how many bytes will be needed for creating statdata item described
   by passed @hint at passed @pos. */
static errno_t stat40_prep_insert(place_t *place, trans_hint_t *hint) {
	uint16_t i;
	statdata_hint_t *stat_hint;
    
	aal_assert("vpf-074", hint != NULL);

	hint->len = 0;
	
	if (place->pos.unit == MAX_UINT32)
		hint->len = sizeof(stat40_t);
	
	stat_hint = (statdata_hint_t *)hint->specific;

	aal_assert("umka-2360", stat_hint->extmask != 0);
    
	/* Estimating the all stat data extentions */
	for (i = 0; i < STAT40_EXTNR; i++) {
		reiser4_plug_t *plug;

		/* Check if extention is present in mask */
		if (!(((uint64_t)1 << i) & stat_hint->extmask))
			continue;

		aal_assert("vpf-773", stat_hint->ext[i] != NULL);
		
		/* If we are on the extention which is multiple of 16 (each mask
		   has 16 bits) then we add to hint's len the size of next
		   mask. */
		if ((i + 1) % 16 == 0) {
			hint->len += sizeof(d16_t);
			continue;
		}

		/* Getting extention plugin */
		if (!(plug = core->factory_ops.ifind(SDEXT_PLUG_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			continue;
		}

		/* Calculating length of the corresponding extention and add it
		   to the estimated value. */
		hint->len += plug_call(plug->o.sdext_ops,
				       length, stat_hint->ext[i]);
	}
	
	return 0;
}

/* Function for modifying stat40. */
static int64_t stat40_modify(place_t *place, trans_hint_t *hint, int insert) {
	uint16_t i;
	void *extbody;
	statdata_hint_t *stat_hint;
    
	extbody = (void *)place->body;
	stat_hint = (statdata_hint_t *)hint->specific;

	if (place->pos.unit == MAX_UINT32 && insert)
		((stat40_t *)extbody)->extmask = 0;
	
	if (!stat_hint->extmask)
		return 0;
    
	for (i = 0; i < STAT40_EXTNR; i++) {
		reiser4_plug_t *plug;

		/* Check if extention is present */
		if (!(((uint64_t)1 << i) & stat_hint->extmask))
			continue;
	    
		/* Stat data contains 16 bit mask of extentions used in it. The
		   first 15 bits of the mask denote the first 15 extentions in
		   the stat data. And the bit number is the stat data extention
		   plugin id. If the last bit turned on, then one more 16 bit
		   mask present and so on. So, we should add sizeof(mask) to
		   extention body pointer, in the case we are on bit dedicated
		   to indicating if next extention exists or not. */
		if (i % 16 == 0) {
			if (insert) {
				uint16_t extmask;
			
				/* Modifying extentions mask. */
				extmask = ((stat_hint->extmask >> i) &
					   0x000000000000ffff);

				extmask |= st40_get_extmask((stat40_t *)extbody);
				st40_set_extmask((stat40_t *)extbody, extmask);
			}
			
			extbody = (void *)extbody + sizeof(d16_t);
		}

		/* Getting extention plugin by extent number. */
		if (!(plug = core->factory_ops.ifind(SDEXT_PLUG_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			continue;
		}

		/* Initializing extention data at passed area */
		if (stat_hint->ext[i]) {
			plug_call(plug->o.sdext_ops, init, extbody,
				  stat_hint->ext[i]);
		}
	
		/* Getting pointer to the next extention. It is evaluating as
		   the previous pointer plus its size. */
		extbody += plug_call(plug->o.sdext_ops, length, extbody);
	}
    
	place_mkdirty(place);
	return 1;
}

/* This method is for insert stat data extentions. */
static int64_t stat40_insert_units(place_t *place, trans_hint_t *hint) {
	aal_assert("vpf-076", place != NULL); 
	aal_assert("vpf-075", hint != NULL);

	return stat40_modify(place, hint, 1);
}

/* This method is for update stat data extentions. */
static int64_t stat40_update_units(place_t *place, trans_hint_t *hint) {
	aal_assert("umka-2588", place != NULL); 
	aal_assert("umka-2589", hint != NULL);

	return stat40_modify(place, hint, 0);
}

/* Removes stat data extentions marked in passed hint stat data extentions
   mask. Needed for fsck. */
static errno_t stat40_remove_units(place_t *place, trans_hint_t *hint) {
	uint16_t i;
	void *extbody;
	uint16_t chunks = 0;
	reiser4_plug_t *plug;
	statdata_hint_t *stat_hint;
		
	aal_assert("umka-2590", place != NULL);
	aal_assert("umka-2591", hint != NULL);

	hint->ohd = 0;
	hint->len = 0;
	
	extbody = (void *)place->body;
	stat_hint = (statdata_hint_t *)hint->specific;

	for (i = 0; i < STAT40_EXTNR; i++) {
		uint16_t extsize;
		uint16_t new_extmask;
		uint16_t old_extmask = 0;

		/* Check if we are on next extention mask. */
		if (i == 0 || ((i + 1) % 16 == 0)) {
			/* Getting current old mask. It is needed to calculate
			   extbody correctly to shrink stat data. */
			old_extmask = *((uint16_t *)extbody);

			if (i > 0) {
				if (!((1 << (i - chunks)) & old_extmask))
					break;
			}
			
			/* Clear the last bit in last mask */
			if ((1 << (i - chunks)) & 0x2f) {
				if (!(old_extmask & 0x8000))
					old_extmask &= ~0x8000;
			}

			/* Calculating new extmask in order to update old
			   one. */
			new_extmask = old_extmask & ~(((stat_hint->extmask >> i) &
						       0x000000000000ffff));

			/* Update mask.*/
			*((uint16_t *)extbody) = new_extmask;
				
			chunks++;
			extbody += sizeof(d16_t);
		}

		/* Check if we're interested in this extention. */
		if (!(((uint64_t)1 << i) & old_extmask))
			continue;

		/* Getting extention plugin by extent number. */
		if (!(plug = core->factory_ops.ifind(SDEXT_PLUG_TYPE, i))) {
			aal_exception_warn("Can't find stat data extention plugin "
					   "by its id 0x%x.", i);
			return -EINVAL;
		}

		extsize = plug_call(plug->o.sdext_ops, length, extbody);
		
		/* Moving the rest of stat data to left in odrer to keep stat
		   data extention packed. */
		aal_memmove(extbody, extbody + extsize, place->len -
			    ((extbody + extsize) - place->body));
		
		/* Getting pointer to the next extention. It is evaluating as
		   the previous pointer plus its size. */
		extbody += extsize;
	}
	
	place_mkdirty(place);
	return 0;
}

/* Helper structrure for keeping track of stat data extention body */
struct body_hint {
	void *body;
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
	return -(sdext->plug->id.id >= hint->ext);
}

/* Finds extention body by number of bit in 64bits mask */
void *stat40_sdext_body(place_t *place, uint8_t bit) {
	struct body_hint hint = {NULL, bit};

	if (stat40_traverse(place, callback_body_ext, &hint) < 0)
		return NULL;
	
	return hint.body;
}

/* Helper structure for keeping track of presence of a stat data extention. */
struct present_hint {
	int present;
	uint8_t ext;
};

typedef struct present_hint present_hint_t;

/* Callback for getting presence information for certain stat data extention. */
static errno_t callback_present_ext(sdext_entity_t *sdext,
				    uint16_t extmask, 
				    void *data)
{
	present_hint_t *hint = (present_hint_t *)data;

	hint->present = (sdext->plug->id.id == hint->ext);
	return hint->present;
}

/* Determines if passed extention denoted by @bit present in statdata item */
int stat40_sdext_present(place_t *place, uint8_t bit) {
	present_hint_t hint = {0, bit};

	if (!stat40_traverse(place, callback_present_ext, &hint) < 0)
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
static uint32_t stat40_sdext_count(place_t *place) {
        uint32_t count = 0;

        if (stat40_traverse(place, callback_count_ext, &count) < 0)
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

	print_mask = (sdext->plug->id.id == 0 ||
		      (sdext->plug->id.id + 1) % 16 == 0);
	
	if (print_mask)	{
		aal_stream_format(stream, "mask:\t\t0x%x\n",
				  extmask);
	}
				
	aal_stream_format(stream, "plugin:\t\t%s\n",
			  sdext->plug->label);
	
	aal_stream_format(stream, "offset:\t\t%u\n",
			  sdext->offset);
	
	length = plug_call(sdext->plug->o.sdext_ops,
			   length, sdext->body);
	
	aal_stream_format(stream, "len:\t\t%u\n", length);
	
	plug_call(sdext->plug->o.sdext_ops, print,
		  sdext->body, stream, 0);
	
	return 0;
}

/* Prints statdata item into passed @stream */
static errno_t stat40_print(place_t *place,
			    aal_stream_t *stream,
			    uint16_t options)
{
	aal_assert("umka-1407", place != NULL);
	aal_assert("umka-1408", stream != NULL);
    
	aal_stream_format(stream, "STATDATA PLUGIN=%s LEN=%u, KEY=[%s] "
			  "UNITS=1\n", place->plug->label, place->len,
			  core->key_ops.print(&place->key, PO_DEFAULT));
		
	aal_stream_format(stream, "exts:\t\t%u\n", stat40_sdext_count(place));
	return stat40_traverse(place, callback_print_ext, (void *)stream);
}
#endif

/* Get the plugin id of the type @type if stored in SD. */
static rid_t stat40_object_plug(place_t *place, rid_t type) {
	trans_hint_t hint;
	statdata_hint_t stat;
	sdext_lw_hint_t lw_hint;
	
	aal_assert("vpf-1074", place != NULL);

	aal_memset(&stat, 0, sizeof(stat));

	/* FIXME-UMKA: Here should be stat data extentions inspected first in
	   order to find non-standard object plugin. And only if it is not
	   found, we should take a look to mode field of the lw extention. */
	if (type == OBJECT_PLUG_TYPE) {
		hint.specific = &stat;
		stat.ext[SDEXT_LW_ID] = &lw_hint;

		if (stat40_fetch_units(place, &hint) != 1)
			return INVAL_PID;

#ifndef ENABLE_STAND_ALONE	
		if (S_ISLNK(lw_hint.mode))
			return core->param_ops.value("symlink");
		else if (S_ISREG(lw_hint.mode))
			return core->param_ops.value("regular");
		else if (S_ISDIR(lw_hint.mode))
			return core->param_ops.value("directory");
		else if (S_ISCHR(lw_hint.mode))
			return core->param_ops.value("special");
		else if (S_ISBLK(lw_hint.mode))
			return core->param_ops.value("special");
		else if (S_ISFIFO(lw_hint.mode))
			return core->param_ops.value("special");
		else if (S_ISSOCK(lw_hint.mode))
			return core->param_ops.value("special");
#else
		if (S_ISLNK(lw_hint.mode))
			return OBJECT_SYM40_ID;
		else if (S_ISREG(lw_hint.mode))
			return OBJECT_REG40_ID;
		else if (S_ISDIR(lw_hint.mode))
			return OBJECT_DIR40_ID;
#endif
	}
	
	return INVAL_PID;
}

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_STAND_ALONE
	.prep_shift       = NULL,
	.shift_units      = NULL,
	.update_key       = NULL,
	.maxreal_key      = NULL,
	.mergeable        = NULL,
#endif
	.lookup           = NULL,
	.fetch_key	  = NULL,
	
	.units            = stat40_units,
	.maxposs_key	  = stat40_maxposs_key,
};

static item_object_ops_t object_ops = {
	.read_units       = NULL,
	
#ifndef ENABLE_STAND_ALONE
	.prep_write       = NULL,
	.write_units      = NULL,

	.prep_insert      = stat40_prep_insert,
	.insert_units     = stat40_insert_units,
	.update_units     = stat40_update_units,
	.remove_units     = stat40_remove_units,
	
	.trunc_units      = NULL,
	.layout           = NULL,
	.size		  = NULL,
	.bytes		  = NULL,
#endif
	.fetch_units      = stat40_fetch_units,
	.object_plug      = stat40_object_plug
};

static item_repair_ops_t repair_ops = {
#ifndef ENABLE_STAND_ALONE	
	.prep_merge       = stat40_prep_merge,
	.merge_units      = stat40_merge_units,
	.check_struct     = stat40_check_struct,
	.check_layout	  = NULL
#endif
};

static item_debug_ops_t debug_ops = {
#ifndef ENABLE_STAND_ALONE	
	.print		  = stat40_print,
#endif
};

static item_tree_ops_t tree_ops = {
	.down_link        = NULL,
#ifndef ENABLE_STAND_ALONE
	.update_link      = NULL
#endif
};

static reiser4_item_ops_t stat40_ops = {
	.tree             = &tree_ops,
	.debug            = &debug_ops,
	.object           = &object_ops,
	.repair           = &repair_ops,
	.balance          = &balance_ops
};

static reiser4_plug_t stat40_plug = {
	.cl    = CLASS_INIT,
	.id    = {ITEM_STATDATA40_ID, STATDATA_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "stat40",
	.desc  = "Stat data item for reiser4, ver. " VERSION,
#endif
	.o = {
		.item_ops = &stat40_ops
	}
};

static reiser4_plug_t *stat40_start(reiser4_core_t *c) {
	core = c;
	return &stat40_plug;
}

plug_register(stat40, stat40_start, NULL);

