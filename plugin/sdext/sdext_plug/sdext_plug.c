/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_plug.c -- plugin id stat data extension plugin. */

#include "sdext_plug.h"

reiser4_core_t *sdext_plug_core = NULL;

uint32_t sdext_plug_length(stat_entity_t *stat, void *hint) {
	uint16_t count = 0;
	
	/* If hint is given, count its pset. */
	if (hint) {
		sdhint_plug_t *h = (sdhint_plug_t *)hint;
		uint16_t i;

		for (i = 0; i < OPSET_STORE_LAST; i++) {
			if (h->mask & (1 << i))
				count++;
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
	uint16_t i;
	
	aal_assert("vpf-1597", stat != NULL);
	aal_assert("vpf-1598", hint != NULL);

	plugh = (sdhint_plug_t *)hint;
	ext = (sdext_plug_t *)stat_body(stat);
	
	aal_memset(plugh, 0, sizeof(*plugh));
	
	for (i = 0; i < sdext_plug_get_count(ext); i++) {
		rid_t mem, id;

		mem = sdext_plug_get_member(ext, i);
		id = sdext_plug_get_pid(ext, i);
		
		/* Check the member id valideness. */
		if (mem >= OPSET_STORE_LAST)
			return -EIO;
		
		/* Check if we met this member already. */
		if (plugh->plug[mem])
			return -EIO;

		/* Obtain the plugin by the id. */
		plugh->plug[mem] = sdext_plug_core->pset_ops.find(mem, id);
		
		if (plugh->plug[mem] == INVAL_PTR) {
#ifndef ENABLE_MINIMAL
			aal_error("Node (%llu), item (%u): Failed to find "
				  "a plugin of the opset member (%u), id "
				  "(%u).", place_blknr(stat->place),
				  stat->place->pos.item, mem, id);
			return -EIO;
#else
			plugh->plug[mem] = NULL;
#endif
		}
#if 0		
		/* If the plug is valid but cannot be found (not implemented 
		   yet), set INVAL_PTR into the opset (to let upper levels to 
		   know that smth is stored here, but there is no such plugin). 
		 */
		if (!h->plug[mem])
			h->plug[mem] = INVAL_PTR;
#endif
		plugh->mask |= (1 << mem);
	}

	return 0;
}

#ifndef ENABLE_MINIMAL
static errno_t sdext_plug_init(stat_entity_t *stat, void *hint) {
	sdhint_plug_t *plugh;
	sdext_plug_t *ext;
	uint16_t count = 0;
	rid_t mem;
	
	aal_assert("vpf-1600", stat != NULL);
	aal_assert("vpf-1599", hint != NULL);
	
	plugh = (sdhint_plug_t *)hint;
	ext = (sdext_plug_t *)stat_body(stat);
	
	for (mem = 0; mem < OPSET_STORE_LAST; mem++) {
		/* Find the plugin to be stored. */
		if (!(plugh->mask & (1 << mem)))
			continue;

		sdext_plug_set_member(ext, count, mem);
		sdext_plug_set_pid(ext, count, plugh->plug[mem] ? 
				   plugh->plug[mem]->id.id : 0);

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

static reiser4_sdext_ops_t sdext_plug_ops = {
	.open	 	= sdext_plug_open,
#ifndef ENABLE_MINIMAL
	.init	 	= sdext_plug_init,
	.print   	= sdext_plug_print,
	.check_struct   = sdext_plug_check_struct,
#endif
	.length	 	= sdext_plug_length
};

static reiser4_plug_t sdext_plug_plug = {
	.cl    = class_init,
	.id    = {SDEXT_PLUG_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
	.label = "sdext_plug",
	.desc  = "Plugin id stat data extension plugin.",
#endif
	.o = {
		.sdext_ops = &sdext_plug_ops
	}
};

static reiser4_plug_t *sdext_plug_start(reiser4_core_t *c) {
	sdext_plug_core = c;
	return &sdext_plug_plug;
}

plug_register(sdext_plug, sdext_plug_start, NULL);

