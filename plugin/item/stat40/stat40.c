/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40.c -- reiser4 stat data plugin. */

#include "stat40.h"
#include "stat40_repair.h"
#include <sys/stat.h>

reiser4_core_t *stat40_core;

/* The function which implements stat40 layout pass. This function is used for
   all statdata extension-related actions. For example for reading, or
   counting. */
errno_t stat40_traverse(reiser4_place_t *place, ext_func_t ext_func, 
			sdext_entity_t *sdext, void *data) 
{
	uint16_t i, len;
	uint16_t chunks = 0;
	uint16_t extmask = 0;

	aal_assert("umka-1197", place != NULL);
	aal_assert("umka-2059", ext_func != NULL);
	aal_assert("vpf-1386",  sdext != NULL);
    
	sdext->offset = 0;
	sdext->body = place->body;
	sdext->sdlen = place->len;
		
	/* Loop though the all possible extensions and calling passed @ext_func
	   for each of them if corresponing extension exists. */
	for (i = 0; i < STAT40_EXTNR; i++) {
		errno_t res;

		if (i == 0 || ((i + 1) % 16 == 0)) {
			/* Check if next pack exists. */
			if (i > 0) {
				if (!((1 << (16 - 1)) & extmask) || 
				    i + 1 == STAT40_EXTNR)
				{
					break;
				}
			}
			
			extmask = *((uint16_t *)sdext->body);
			
			sdext->plug = NULL;
			
			/* Call the callback for every read extmask. */
			if ((res = ext_func(sdext, extmask << (chunks * 16),
					    data)))
				return res;

			chunks++;
			sdext->body += sizeof(d16_t);
			sdext->offset += sizeof(d16_t);

			if (i > 0) continue;
		}

		/* If extension is not present, we going to the next one */
		if (!((1 << (i - (chunks - 1) * 16)) & extmask))
			continue;

		/* Getting extension plugin from the plugin factory */
		if (!(sdext->plug = stat40_core->factory_ops.ifind(SDEXT_PLUG_TYPE, i)))
			continue;
		
		len = plug_call(sdext->plug->o.sdext_ops, 
				length, sdext->body);

		/* Call the callback for every found extension. */
		if ((res = ext_func(sdext, extmask, data)))
			return res;

		/* Calculating the pointer to the next extension body */
		sdext->body += len;
		sdext->offset += len;
	}
 
	return 0;
}

/* Callback for opening one extension */
static errno_t callback_open_ext(sdext_entity_t *sdext,
				 uint64_t extmask, void *data)
{
	trans_hint_t *hint;
	statdata_hint_t *stat_hint;

	/* Method open is not defined, this probably means, we only interested
	   in symlink's length method in order to reach other symlinks body. So,
	   we retrun 0 here. */
	if (!sdext->plug || !sdext->plug->o.sdext_ops->open)
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

/* Fetches whole statdata item with extensions into passed @buff */
static int64_t stat40_fetch_units(reiser4_place_t *place, trans_hint_t *hint) {
	sdext_entity_t sdext;

	aal_assert("umka-1415", hint != NULL);
	aal_assert("umka-1414", place != NULL);

	if (stat40_traverse(place, callback_open_ext, &sdext, hint))
		return -EINVAL;

	return 1;
}

/* This function returns unit count. This value must be 1 if item has not
   units. It is because balancing code assumes that if item has more than one
   unit the it may be shifted out. That is because w ecan't return the number of
   extensions here. Extensions are the statdata private bussiness. */
static uint32_t stat40_units(reiser4_place_t *place) {
	return 1;
}

#ifndef ENABLE_STAND_ALONE
/* Estimates how many bytes will be needed for creating statdata item described
   by passed @hint at passed @pos. */
static errno_t stat40_prep_insert(reiser4_place_t *place, trans_hint_t *hint) {
	uint16_t i;
	statdata_hint_t *stat_hint;
    
	aal_assert("vpf-074", hint != NULL);

	hint->len = 0;
	
	if (place->pos.unit == MAX_UINT32)
		hint->len = sizeof(stat40_t);
	
	stat_hint = (statdata_hint_t *)hint->specific;

	aal_assert("umka-2360", stat_hint->extmask != 0);
    
	/* Estimating the all stat data extensions */
	for (i = 0; i < STAT40_EXTNR; i++) {
		reiser4_plug_t *plug;

		/* Check if extension is present in mask */
		if (!(((uint64_t)1 << i) & stat_hint->extmask))
			continue;

		aal_assert("vpf-773", stat_hint->ext[i] != NULL);
		
		/* If we are on the extension which is multiple of 16 (each mask
		   has 16 bits) then we add to hint's len the size of next
		   mask. */
		if ((i + 1) % 16 == 0) {
			hint->len += sizeof(d16_t);
			continue;
		}

		/* Getting extension plugin */
		if (!(plug = stat40_core->factory_ops.ifind(SDEXT_PLUG_TYPE, i))) {
			aal_warn("Can't find stat data extension plugin "
				 "by its id 0x%x.", i);
			return -EINVAL;
		}

		/* Calculating length of the corresponding extension and add it
		   to the estimated value. */
		hint->len += plug_call(plug->o.sdext_ops, length, 
				       stat_hint->ext[i]);
	}
	
	return 0;
}

/* Function for modifying stat40. */
static int64_t stat40_modify(reiser4_place_t *place, trans_hint_t *hint, int insert) {
	statdata_hint_t *stat_hint;
	uint16_t extmask = 0;
	void *extbody;
	uint16_t i;
    
	extbody = (void *)place->body;
	stat_hint = (statdata_hint_t *)hint->specific;

	if (place->pos.unit == MAX_UINT32 && insert)
		((stat40_t *)extbody)->extmask = 0;
	
	if (!stat_hint->extmask)
		return 0;
    
	for (i = 0; i < STAT40_EXTNR; i++) {
		reiser4_plug_t *plug;
	    
		/* Stat data contains 16 bit mask of extensions used in it. The
		   first 15 bits of the mask denote the first 15 extensions in
		   the stat data. And the bit number is the stat data extension
		   plugin id. If the last bit turned on, then one more 16 bit
		   mask present and so on. So, we should add sizeof(mask) to
		   extension body pointer, in the case we are on bit dedicated
		   to indicating if next extension exists or not. */
		/* Check if we are on next extension mask. */
		if (i == 0 || ((i + 1) % 16 == 0)) {
			if (i > 0) {
				if (!((1 << (16 - 1)) & extmask) || 
				    i + 1 == STAT40_EXTNR)
				{
					break;
				}
			}
			
			extmask = *((uint16_t *)extbody);

			if (insert) {
				/* Calculating new extmask in order to 
				   update old one. */
				extmask |= (((stat_hint->extmask >> i) &
					     0x000000000000ffff));

				/* Update mask.*/
				*((uint16_t *)extbody) = extmask;
			}
			
			extbody += sizeof(d16_t);

			if (i > 0) continue;
		}

		/* Check if extension is present in mask */
		if (!(((uint64_t)1 << i) & extmask))
			continue;

		/* Getting extension plugin by extent number. */
		if (!(plug = stat40_core->factory_ops.ifind(SDEXT_PLUG_TYPE, i))) {
			aal_warn("Can't find stat data extension plugin "
				 "by its id 0x%x.", i);
			return -EINVAL;
		}

		/* Initializing extension data at passed area */
		if (stat_hint->ext[i]) {
			if (insert) {
				uint32_t extsize;

				/* Moving the rest of stat data to the right 
				   to keep stat data extension packed. */
				extsize = plug_call(plug->o.sdext_ops, length,
						    stat_hint->ext[i]);

				aal_memmove(extbody + extsize, extbody, place->len -
					    ((extbody + extsize) - place->body));
			}
			
			plug_call(plug->o.sdext_ops, init, extbody,
				  stat_hint->ext[i]);
		}

		/* Getting pointer to the next extension. It is evaluating as
		   the previous pointer plus its size. */
		extbody += plug_call(plug->o.sdext_ops, length, extbody);
	}
    
	place_mkdirty(place);
	return 1;
}

/* This method is for insert stat data extensions. */
static int64_t stat40_insert_units(reiser4_place_t *place, trans_hint_t *hint) {
	aal_assert("vpf-076", place != NULL); 
	aal_assert("vpf-075", hint != NULL);

	return stat40_modify(place, hint, 1);
}

/* This method is for update stat data extensions. */
static int64_t stat40_update_units(reiser4_place_t *place, trans_hint_t *hint) {
	aal_assert("umka-2588", place != NULL); 
	aal_assert("umka-2589", hint != NULL);

	return stat40_modify(place, hint, 0);
}

/* Removes stat data extensions marked in passed hint stat data extensions
   mask. Needed for fsck. */
static errno_t stat40_remove_units(reiser4_place_t *place, trans_hint_t *hint) {
	statdata_hint_t *stat_hint;
	reiser4_plug_t *plug;
	uint16_t old_extmask = 0;
	uint16_t new_extmask;
	uint16_t extsize;
	uint16_t chunks = 0;
	void *extbody;
	uint16_t i;

	aal_assert("umka-2590", place != NULL);
	aal_assert("umka-2591", hint != NULL);

	hint->overhead = 0;
	hint->len = 0;
	
	extbody = (void *)place->body;
	stat_hint = (statdata_hint_t *)hint->specific;

	for (i = 0; i < STAT40_EXTNR; i++) {
		/* Check if we are on next extension mask. */
		if (i == 0 || ((i + 1) % 16 == 0)) {
			/* Getting current old mask. It is needed to calculate
			   extbody correctly to shrink stat data. */
			if (i > 0) {
				if (!((1 << (16 - 1)) & old_extmask) || 
				    i + 1 == STAT40_EXTNR)
				{
					break;
				}
			}
			
			old_extmask = *((uint16_t *)extbody);

			
			/* Calculating new extmask in order to update old
			   one. */
			new_extmask = old_extmask & ~(((stat_hint->extmask >> i) &
						       0x000000000000ffff));

			/* Update mask.*/
			*((uint16_t *)extbody) = new_extmask;
				
			chunks++;
			extbody += sizeof(d16_t);

			if (i > 0) continue;
		}

		/* Check if we're interested in this extension. */
		if (!(((uint64_t)1 << i) & old_extmask))
			continue;

		/* Getting extension plugin by extent number. */
		if (!(plug = stat40_core->factory_ops.ifind(SDEXT_PLUG_TYPE, i))) {
			aal_warn("Can't find stat data extension plugin "
				 "by its id 0x%x.", i);
			return -EINVAL;
		}

		extsize = plug_call(plug->o.sdext_ops, length, extbody);
		
		if ((((uint64_t)1 << i) & stat_hint->extmask)) {
			/* Moving the rest of stat data to left in odrer to 
			   keep stat data extension packed. */
			aal_memmove(extbody, extbody + extsize, place->len -
				    ((extbody + extsize) - place->body));

			hint->len += extsize;
		} else {
			/* Setting pointer to the next extension. */
			extbody += extsize;
		}
	}
	
	place_mkdirty(place);
	return 0;
}

/* Helper structrure for keeping track of stat data extension body */
struct body_hint {
	void *body;
	uint8_t ext;
};

typedef struct body_hint body_hint_t;

/* Callback function for finding stat data extension body by bit */
static errno_t callback_body_ext(sdext_entity_t *sdext,
				 uint64_t extmask, void *data)
{
	body_hint_t *hint = (body_hint_t *)data;
	if (!sdext->plug) return 0;
	
	hint->body = sdext->body;
	return -(sdext->plug->id.id >= hint->ext);
}

/* Finds extension body by number of bit in 64bits mask */
void *stat40_sdext_body(reiser4_place_t *place, uint8_t bit) {
	struct body_hint hint = {NULL, bit};
	sdext_entity_t sdext;

	if (stat40_traverse(place, callback_body_ext, &sdext, &hint) < 0)
		return NULL;
	
	return hint.body;
}

/* Helper structure for keeping track of presence of a stat data extension. */
struct present_hint {
	int present;
	uint8_t ext;
};

typedef struct present_hint present_hint_t;

/* Callback for getting presence information for certain stat data extension. */
static errno_t callback_present_ext(sdext_entity_t *sdext,
				    uint64_t extmask, void *data)
{
	present_hint_t *hint = (present_hint_t *)data;
	if (!sdext->plug) return 0;

	hint->present = (sdext->plug->id.id == hint->ext);
	return hint->present;
}

/* Determines if passed extension denoted by @bit present in statdata item */
int stat40_sdext_present(reiser4_place_t *place, uint8_t bit) {
	present_hint_t hint = {0, bit};
	sdext_entity_t sdext;

	if (!stat40_traverse(place, callback_present_ext, &sdext, &hint) < 0)
		return 0;

	return hint.present;
}
#endif

/* Get the plugin id of the type @type if stored in SD. */
static rid_t stat40_object_plug(reiser4_place_t *place, rid_t type) {
	trans_hint_t hint;
	statdata_hint_t stat;
	sdext_lw_hint_t lw_hint;
	
	aal_assert("vpf-1074", place != NULL);

	aal_memset(&stat, 0, sizeof(stat));

	/* FIXME-UMKA: Here should be stat data extensions inspected first in
	   order to find non-standard object plugin. And only if it is not
	   found, we should take a look to mode field of the lw extension. */
	if (type == OBJECT_PLUG_TYPE) {
		hint.specific = &stat;
		stat.ext[SDEXT_LW_ID] = &lw_hint;

		if (stat40_fetch_units(place, &hint) != 1)
			return INVAL_PID;

#ifndef ENABLE_STAND_ALONE	
		if (S_ISLNK(lw_hint.mode))
			return stat40_core->profile_ops.value(PROF_SYM);
		else if (S_ISREG(lw_hint.mode))
			return stat40_core->profile_ops.value(PROF_REG);
		else if (S_ISDIR(lw_hint.mode))
			return stat40_core->profile_ops.value(PROF_DIR);
		else if (S_ISCHR(lw_hint.mode))
			return stat40_core->profile_ops.value(PROF_SPL);
		else if (S_ISBLK(lw_hint.mode))
			return stat40_core->profile_ops.value(PROF_SPL);
		else if (S_ISFIFO(lw_hint.mode))
			return stat40_core->profile_ops.value(PROF_SPL);
		else if (S_ISSOCK(lw_hint.mode))
			return stat40_core->profile_ops.value(PROF_SPL);
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
	.fuse		  = NULL,
	.prep_shift	  = NULL,
	.shift_units	  = NULL,
	.update_key	  = NULL,
	.maxreal_key	  = NULL,
	.mergeable	  = NULL,
	.collision	  = NULL,
#endif
	.lookup		  = NULL,
	.fetch_key	  = NULL,
	.maxposs_key	  = NULL,

	.units		  = stat40_units,
};

static item_object_ops_t object_ops = {
	.read_units	  = NULL,
	
#ifndef ENABLE_STAND_ALONE
	.prep_write	  = NULL,
	.write_units	  = NULL,

	.prep_insert	  = stat40_prep_insert,
	.insert_units	  = stat40_insert_units,
	.update_units	  = stat40_update_units,
	.remove_units	  = stat40_remove_units,
	
	.trunc_units	  = NULL,
	.layout		  = NULL,
	.size		  = NULL,
	.bytes		  = NULL,
	.overhead	  = NULL,
#endif
	.fetch_units	  = stat40_fetch_units,
	.object_plug	  = stat40_object_plug
};

static item_repair_ops_t repair_ops = {
#ifndef ENABLE_STAND_ALONE
	.check_struct	  = stat40_check_struct,
	.check_layout	  = NULL,
	
	.prep_merge	  = NULL,
	.merge		  = NULL,
	
	.pack		  = NULL,
	.unpack		  = NULL
#endif
};

static item_debug_ops_t debug_ops = {
#ifndef ENABLE_STAND_ALONE	
	.print		  = stat40_print,
#endif
};

static item_tree_ops_t tree_ops = {
	.down_link	  = NULL,
#ifndef ENABLE_STAND_ALONE
	.update_link	  = NULL
#endif
};

static reiser4_item_ops_t stat40_ops = {
	.tree		  = &tree_ops,
	.debug		  = &debug_ops,
	.object		  = &object_ops,
	.repair		  = &repair_ops,
	.balance	  = &balance_ops
};

static reiser4_plug_t stat40_plug = {
	.cl    = class_init,
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
	stat40_core = c;
	return &stat40_plug;
}

plug_register(stat40, stat40_start, NULL);

