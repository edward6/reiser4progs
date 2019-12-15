/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_plug.c -- plugin id stat data extension plugin. */

#include "sdext_plug.h"

reiser4_core_t *sdext_pset_core = NULL;

#ifndef ENABLE_MINIMAL
static void sdext_plug_info(stat_entity_t *stat) {
	sdext_plug_t *ext;
	uint8_t i;

	stat->info.digest = NULL;
	
	/* When inserting new extentions, nothing to be done. */
	if (stat->plug->p.id.id != SDEXT_PSET_ID || !stat) 
		return;
	
	ext = (sdext_plug_t *)stat_body(stat);
	for (i = 0; i < sdext_plug_get_count(ext); i++) {
		rid_t mem, id;
		
		mem = sdext_plug_get_member(ext, i);
		id = sdext_plug_get_pid(ext, i);

		if (mem != SDEXT_PSET_ID)
			continue;
		
		stat->info.digest = sdext_pset_core->pset_ops.find(mem, id, 1);
		if (stat->info.digest == INVAL_PTR)
			stat->info.digest = NULL;
		
		return;
	}
}
#endif

uint32_t sdext_plug_length(stat_entity_t *stat, void *hint) {
	uint16_t count = 0;
	uint64_t mask;
	
	aal_assert("vpf-1844", stat != NULL || hint != NULL);
	
	/* If hint is given, count its pset. */
	if (hint) {
		sdhint_plug_t *h = (sdhint_plug_t *)hint;
		
		mask = h->plug_mask;
		
		while (mask) {
			if (mask & 1) count++;
			mask >>= 1;
		}
	} else {
		sdext_plug_t *plug = (sdext_plug_t *)stat_body(stat);
		
		count = sdext_plug_get_count(plug);
	}
	
	/* Count on-disk pset. */
	return sizeof(sdext_plug_slot_t) * count + 
		(count ? sizeof(sdext_plug_t) : 0);
}

static errno_t sdext_plug_open(stat_entity_t *stat, void *hint) {
	sdhint_plug_t *plugh;
	sdext_plug_t *ext;
	int is_pset;
	uint16_t i;
	
	aal_assert("vpf-1597", stat != NULL);
	aal_assert("vpf-1598", hint != NULL);

	plugh = (sdhint_plug_t *)hint;
	ext = (sdext_plug_t *)stat_body(stat);

	is_pset = stat->plug->p.id.id == SDEXT_PSET_ID;
	
	aal_memset(plugh, 0, sizeof(sdhint_plug_t));
	
	for (i = 0; i < sdext_plug_get_count(ext); i++) {
		rid_t mem, id;
		
		mem = sdext_plug_get_member(ext, i);
		id = sdext_plug_get_pid(ext, i);
		
		/* Check the member id valideness. */
		if (mem >= PSET_STORE_LAST)
			return -EIO;
		
		/* Check if we met this member already. */
		if (plugh->plug_mask & (1 << mem))
			return -EIO;
		
		/* Obtain the plugin by the id. */
		plugh->plug[mem] = sdext_pset_core->pset_ops.find(
						mem, id, is_pset);
		
		if (plugh->plug[mem] == INVAL_PTR) {
#ifndef ENABLE_MINIMAL
			aal_error("Node (%llu), item (%u): Failed to find "
				  "a plugin of the pset member (%u), id "
				  "(%u).",
				  (unsigned long long)place_blknr(stat->place),
				  stat->place->pos.item, mem, id);
			return -EIO;
#else
			plugh->plug[mem] = NULL;
#endif
		}
		
		/* For those where no plugin is found but the id is correct, 
		   keep the id in plug set, remember it is a parameter. */
		if (plugh->plug[mem] == NULL)
		  plugh->plug[mem] = (void *)((unsigned long)id);
		
		plugh->plug_mask |= (1 << mem);
	}

	return 0;
}

#ifndef ENABLE_MINIMAL
static errno_t sdext_plug_init(stat_entity_t *stat, void *hint) {
	sdhint_plug_t *plugh;
	tree_entity_t *tree;
	sdext_plug_t *ext;
	uint16_t count = 0;
	uint16_t id;
	rid_t mem;
	
	aal_assert("vpf-1600", stat != NULL);
	aal_assert("vpf-1599", hint != NULL);
	
	plugh = (sdhint_plug_t *)hint;
	ext = (sdext_plug_t *)stat_body(stat);
	tree = stat->place->node->tree;
	
	for (mem = 0; mem < PSET_STORE_LAST; mem++) {
		/* Find the plugin to be stored. */
		if (!(plugh->plug_mask & (1 << mem)))
			continue;

		sdext_plug_set_member(ext, count, mem);

		id = (tree->param_mask & (1 << mem)) ? 
			((unsigned long)plugh->plug[mem]) :
			plugh->plug[mem]->id.id;
		
		sdext_plug_set_pid(ext, count, id);
		count++;
	}
	
	sdext_plug_set_count(ext, count);
	
	return 0;
}

extern errno_t sdext_plug_check_struct(stat_entity_t *stat, 
				       repair_hint_t *hint);

extern void sdext_plug_print(stat_entity_t *stat, 
			     aal_stream_t *stream, 
			     uint16_t options);

#endif

reiser4_sdext_plug_t sdext_pset_plug = {
	.p = {
		.id    = {SDEXT_PSET_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "sdext_plugin_set",
		.desc  = "Plugin Set StatData extension plugin.",
#endif
	},

#ifndef ENABLE_MINIMAL
	.init	 	= sdext_plug_init,
	.info		= sdext_plug_info,
	.print   	= sdext_plug_print,
	.check_struct   = sdext_plug_check_struct,
#else
	.info		= NULL,
#endif
	.open	 	= sdext_plug_open,
	.length	 	= sdext_plug_length
};

reiser4_sdext_plug_t sdext_hset_plug = {
	.p = {
		.id    = {SDEXT_HSET_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "sdext_heir_set",
		.desc  = "Heir Set StatData extension plugin.",
#endif
	},

#ifndef ENABLE_MINIMAL
	.init	 	= sdext_plug_init,
	.info		= sdext_plug_info,
	.print   	= sdext_plug_print,
	.check_struct   = sdext_plug_check_struct,
#else
	.info		= NULL,
#endif
	.open	 	= sdext_plug_open,
	.length	 	= sdext_plug_length
};
