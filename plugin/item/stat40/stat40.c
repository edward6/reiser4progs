/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   stat40.c -- reiser4 stat data plugin. */

#include "stat40.h"
#include "stat40_repair.h"
#include <sys/stat.h>

reiser4_core_t *stat40_core = NULL;

/* The function which implements stat40 layout pass. This function is used for
   all statdata extension-related actions. For example for reading, or
   counting. */
errno_t stat40_traverse(reiser4_place_t *place, 
			ext_func_t ext_func,
			void *data)
{
	uint16_t extmask = 0;
	uint16_t chunks = 0;
	stat_entity_t stat;
	uint16_t i;
	
	aal_assert("umka-1197", place != NULL);
	aal_assert("umka-2059", ext_func != NULL);
    
	aal_memset(&stat, 0, sizeof(stat));
	stat.place = place;
	
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
			
			extmask = st40_get_extmask(stat_body(&stat));
			
			stat.plug = NULL;
			
			/* Call the callback for every read extmask. */
			res = ext_func(&stat, extmask << (chunks * 16), data);
			if (res) return res;
			
			chunks++;
			stat.offset += sizeof(stat40_t);

			if (i > 0) continue;
		}

		/* If extension is not present, we going to the next one */
		if (!((1 << (i - (chunks - 1) * 16)) & extmask))
			continue;

		/* Getting extension plugin from the plugin factory */
		stat.plug = (reiser4_sdext_plug_t *)
			stat40_core->factory_ops.ifind(SDEXT_PLUG_TYPE,i);
		
		if (!stat.plug)
			continue;
		
		/* Call the callback for every found extension. */
		if ((res = ext_func(&stat, extmask, data)))
			return res;

		if (stat.plug->info)
			stat.plug->info(&stat);

		/* Calculating the pointer to the next extension body */
		stat.offset += objcall(&stat, length, NULL);
	}
 
	return 0;
}

/* Callback for opening one extension */
static errno_t cb_open_ext(stat_entity_t *stat, uint64_t extmask, void *data) {
	stat_hint_t *stath;
	trans_hint_t *hint;

	/* Method open is not defined, this probably means, we only interested
	   in symlink's length method in order to reach other symlinks body. So,
	   we retrun 0 here. */
	if (!stat->plug || !stat->plug->open)
		return 0;
	
	hint = (trans_hint_t *)data;
	stath = hint->specific;

	/* Reading mask into hint */
	stath->extmask |= ((uint64_t)1 << stat->plug->p.id.id);

	/* We load @ext if its hint present in @stath */
	if (stath->ext[stat->plug->p.id.id]) {
		void *sdext = stath->ext[stat->plug->p.id.id];

		return objcall(stat, open, sdext);
	}
	
	return 0;
}

static inline reiser4_plug_t *stat40_file_mode(tree_entity_t *tree, 
					       uint16_t mode) 
{
	if (S_ISLNK(mode))
		return tree->tset[TSET_SYMFILE];
	else if (S_ISREG(mode))
		return tree->tset[TSET_REGFILE];
	else if (S_ISDIR(mode))
		return tree->tset[TSET_DIRFILE];
	else if (S_ISCHR(mode))
		return tree->tset[TSET_SPLFILE];
	else if (S_ISBLK(mode))
		return tree->tset[TSET_SPLFILE];
	else if (S_ISFIFO(mode))
		return tree->tset[TSET_SPLFILE];
	else if (S_ISSOCK(mode))
		return tree->tset[TSET_SPLFILE];

	return NULL;
}

static inline reiser4_plug_t *stat40_dir_mode(tree_entity_t *tree, 
					      uint16_t mode) 
{
	return NULL;
}

/* Decodes the object plug from the mode if needed. */
static void stat40_decode_pset(tree_entity_t *tree,
			       sdhint_plug_t *plugh,
			       sdhint_lw_t *lwh)
{
	aal_assert("vpf-1630", tree != NULL);
	aal_assert("vpf-1631", plugh != NULL);
	aal_assert("vpf-1632", lwh != NULL);
	
	/* Object plugin does not need to be set. */
	if (!plugh->plug[PSET_OBJ]) {
		plugh->plug[PSET_OBJ] = stat40_file_mode(tree, lwh->mode);
		plugh->plug_mask |= (1 << PSET_OBJ);
	}
}

/* Fetches whole statdata item with extensions into passed @buff */
static int64_t stat40_fetch_units(reiser4_place_t *place, trans_hint_t *hint) {
	bool_t lw_local = 0;
	sdhint_lw_t lwh;
	void **exts;
	
	aal_assert("umka-1415", hint != NULL);
	aal_assert("umka-1414", place != NULL);
	aal_assert("vpf-1633", place->node != NULL);
	
	exts = ((stat_hint_t *)hint->specific)->ext;
	
	/* If plug_hint is fetched, lw is needed also to adjust PSET_OBJ. */
	if (exts[SDEXT_PSET_ID]) {
		if (!exts[SDEXT_LW_ID]) {
			exts[SDEXT_LW_ID] = &lwh;
			lw_local = 1;
		}
	}
	
	if (stat40_traverse(place, cb_open_ext, hint))
		return -EINVAL;
	
	/* Adjust PSET_OBJ. */
	if (exts[SDEXT_PSET_ID]) {
		stat40_decode_pset(place->node->tree,
				    (sdhint_plug_t *)exts[SDEXT_PSET_ID],
				    (sdhint_lw_t *)exts[SDEXT_LW_ID]);
		if (lw_local)
			exts[SDEXT_LW_ID] = NULL;
	}

	return 1;
}

/* This function returns unit count. This value must be 1 if item has not
   units. It is because balancing code assumes that if item has more than one
   unit the it may be shifted out. That is because w ecan't return the number of
   extensions here. Extensions are the statdata private bussiness. */
static uint32_t stat40_units(reiser4_place_t *place) {
	return 1;
}

#ifndef ENABLE_MINIMAL
static errno_t stat40_encode_pset(reiser4_place_t *place, trans_hint_t *hint) {
	stat_hint_t *sd_hint;
	sdhint_plug_t *plugh;
	tree_entity_t *tree;
	uint16_t mode;
	errno_t res;
	
	aal_assert("vpf-1634", hint != NULL);
	aal_assert("vpf-1635", place != NULL);
	aal_assert("vpf-1636", place->node != NULL);
	
	if (!hint->specific || !((stat_hint_t *)hint->specific)->ext)
		return 0;
	
	sd_hint = (stat_hint_t *)hint->specific;
	plugh = ((sdhint_plug_t *)sd_hint->ext[SDEXT_PSET_ID]);
	
	if (!plugh || !(plugh->plug_mask & (1 << PSET_OBJ)))
		return 0;
	
	/* If LW hint is not present, fetch it from disk. */
	if (!sd_hint->ext[SDEXT_LW_ID]) {
		trans_hint_t trans;
		stat_hint_t stat;
		sdhint_lw_t lwh;
		
		aal_memset(&stat, 0, sizeof(stat));
		
		trans.specific = &stat;
		stat.ext[SDEXT_LW_ID] = &lwh;
		
		if ((res = stat40_fetch_units(place, &trans)) != 1)
			return res;
		
		/* If there is no LW extention at all, nothing to encode. */
		if (!(stat.extmask & (1 << SDEXT_LW_ID)))
			return 0;
		
		mode = lwh.mode;
	} else {
		mode = ((sdhint_lw_t *)sd_hint->ext[SDEXT_LW_ID])->mode;
	}
	
	tree = place->node->tree;
	
	/* These all is performed on plugh hint, not on the object. So this 
	   altered plug_mask will not be reflected in the object plug_mask. */
	if (plugh->plug[PSET_OBJ] == stat40_file_mode(tree, mode))
		plugh->plug_mask &= ~(1 << PSET_OBJ);
	
	plugh->plug_mask &= ~(1 << PSET_DIR);
	
	/* Throw away the SD_PLUG if mask is empty. */
	if (!plugh->plug_mask)
		sd_hint->extmask &= ~(1 << SDEXT_PSET_ID);
	
	return 0;
}

/* Estimates how many bytes will be needed for creating statdata item described
   by passed @hint at passed @pos. */
static errno_t stat40_prep_insert(reiser4_place_t *place, trans_hint_t *hint) {
	stat_hint_t *stath;
	errno_t res;
	uint16_t i;
    
	aal_assert("vpf-074", hint != NULL);

	hint->len = 0;
	
	if ((res = stat40_encode_pset(place, hint)))
		return res;
	
	if (place->pos.unit == MAX_UINT32)
		hint->len = sizeof(stat40_t);
	
	stath = (stat_hint_t *)hint->specific;

	aal_assert("umka-2360", stath->extmask != 0);
    
	/* Estimating the all stat data extensions */
	for (i = 0; i < STAT40_EXTNR; i++) {
		reiser4_sdext_plug_t *plug;

		/* Check if extension is present in mask */
		if (!(((uint64_t)1 << i) & stath->extmask))
			continue;

		aal_assert("vpf-773", stath->ext[i] != NULL);
		
		/* If we are on the extension which is multiple of 16 (each mask
		   has 16 bits) then we add to hint's len the size of next
		   mask. */
		if ((i + 1) % 16 == 0) {
			hint->len += sizeof(stat40_t);
			continue;
		}

		/* Getting extension plugin */
		plug = (reiser4_sdext_plug_t *)
			stat40_core->factory_ops.ifind(SDEXT_PLUG_TYPE, i);
		
		if (!plug) {
			aal_error("Can't find stat data extension plugin "
				  "by its id 0x%x.", i);
			return -EINVAL;
		}

		/* Calculating length of the corresponding extension and add it
		   to the estimated value. */
		hint->len += plugcall(plug, length, NULL, stath->ext[i]);
	}
	
	return 0;
}

/* Function for modifying stat40. */
static int64_t stat40_modify(reiser4_place_t *place, trans_hint_t *hint, int insert) {
	stat_hint_t *stath;
	stat_entity_t stat;
	uint16_t extmask = 0;
	uint16_t chunks = 0;
	uint16_t i;
	
	stath = (stat_hint_t *)hint->specific;
	
	aal_memset(&stat, 0, sizeof(stat));
	stat.place = place;

	/* If this is a new item being inserted, zero the on-disk mask. */
	if (place->pos.unit == MAX_UINT32 && insert)
		st40_set_extmask(stat_body(&stat), 0);

	if (!stath->extmask)
		return 0;
    
	for (i = 0; i < STAT40_EXTNR; i++) {
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
			
			extmask = st40_get_extmask(stat_body(&stat));

			if (insert) {
				/* Calculating new extmask in order to 
				   update old one. */
				extmask |= (((stath->extmask >> i) &
					     0x000000000000ffff));

				/* Update mask.*/
				st40_set_extmask(stat_body(&stat), extmask);
			}
			
			chunks++;
			stat.offset += sizeof(stat40_t);

			if (i > 0) continue;
		}

		/* Check if extension is present in mask */
		if (!((1 << (i << (chunks - 1) * 16)) & extmask))
			continue;

		/* Getting extension plugin by extent number. */
		stat.plug = (reiser4_sdext_plug_t *)
			stat40_core->factory_ops.ifind(SDEXT_PLUG_TYPE, i);
		
		if (!stat.plug) {
			aal_error("Can't find stat data extension plugin "
				  "by its id 0x%x.", i);
			return -EINVAL;
		}

		/* Initializing extension data at passed area */
		if (stath->ext[i]) {
			if (insert) {
				uint32_t extsize;

				/* Moving the rest of stat data to the right 
				   to keep stat data extension packed. */
				extsize = plugcall(stat.plug, length, NULL,
						   stath->ext[i]);

				aal_memmove(stat_body(&stat) + extsize, 
					    stat_body(&stat), place->len -
					    (stat.offset + extsize));

			}
			
			objcall(&stat, init, stath->ext[i]);
		}

		if (stat.plug->info)
			stat.plug->info(&stat);
		
		/* Getting pointer to the next extension. It is evaluating as
		   the previous pointer plus its size. */
		stat.offset += objcall(&stat, length, NULL);
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
	uint16_t old_extmask = 0;
	uint16_t new_extmask;
	uint16_t extsize;
	uint16_t chunks = 0;
	stat_entity_t stat;
	stat_hint_t *stath;
	uint16_t i;

	aal_assert("umka-2590", place != NULL);
	aal_assert("umka-2591", hint != NULL);

	hint->overhead = 0;
	hint->len = 0;
	
	stath = (stat_hint_t *)hint->specific;
	
	aal_memset(&stat, 0, sizeof(stat));
	stat.place = place;
	
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
			
			old_extmask = st40_get_extmask(stat_body(&stat));

			
			/* Calculating new extmask in order to update old
			   one. */
			new_extmask = old_extmask & ~(((stath->extmask >> i) &
						       0x000000000000ffff));

			/* Update mask.*/
			st40_set_extmask(stat_body(&stat), new_extmask);
				
			chunks++;
			stat.offset += sizeof(stat40_t);

			if (i > 0) continue;
		}

		/* Check if we're interested in this extension. */
		if (!((1 << (i - (chunks - 1) * 16)) & old_extmask))
			continue;

		/* Getting extension plugin by extent number. */
		stat.plug = (reiser4_sdext_plug_t *)
			stat40_core->factory_ops.ifind(SDEXT_PLUG_TYPE, i);
		
		if (!stat.plug) {
			aal_error("Can't find stat data extension plugin "
				 "by its id 0x%x.", i);
			return -EINVAL;
		}

		extsize = objcall(&stat, length, NULL);
		
		if ((((uint64_t)1 << i) & stath->extmask)) {
			/* Moving the rest of stat data to left in odrer to 
			   keep stat data extension packed. */
			aal_memmove(stat_body(&stat), 
				    stat_body(&stat) + extsize,
				    place->len - (stat.offset + extsize));

			hint->len += extsize;
		} else {
			/* Setting pointer to the next extension. */
			stat.offset += extsize;

			if (stat.plug->info)
				stat.plug->info(&stat);
		}
	}
	
	place_mkdirty(place);
	return 0;
}

#endif

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_MINIMAL
	.merge		  = NULL,
	.prep_shift	  = NULL,
	.shift_units	  = NULL,
	.update_key	  = NULL,
	.maxreal_key	  = NULL,
	.mergeable	  = NULL,
	.collision	  = NULL,
	.overhead	  = NULL,
#endif
	.init		  = NULL,
	.lookup		  = NULL,
	.fetch_key	  = NULL,
	.maxposs_key	  = NULL,

	.units		  = stat40_units,
};

static item_object_ops_t object_ops = {
	.read_units	  = NULL,
	
#ifndef ENABLE_MINIMAL
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
#endif
	.fetch_units	  = stat40_fetch_units
};

#ifndef ENABLE_MINIMAL
static item_repair_ops_t repair_ops = {
	.check_struct	  = stat40_check_struct,
	.check_layout	  = NULL,
	
	.prep_insert_raw  = stat40_prep_insert_raw,
	.insert_raw	  = stat40_insert_raw,
	
	.pack		  = NULL,
	.unpack		  = NULL
};

static item_debug_ops_t debug_ops = {
	.print		  = stat40_print,
};
#endif

reiser4_item_plug_t stat40_plug = {
	.p = {
		.id    = {ITEM_STAT40_ID, STAT_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "stat40",
		.desc  = "StatData item plugin.",
#endif
	},

	.object		  = &object_ops,
	.balance	  = &balance_ops,
#ifndef ENABLE_MINIMAL
	.repair		  = &repair_ops,
	.debug		  = &debug_ops,
#endif

};
