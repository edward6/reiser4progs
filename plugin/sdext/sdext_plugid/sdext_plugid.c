/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_plugid.c -- plugin id stat data extension plugin. */

#include "sdext_plugid.h"

reiser4_core_t *sdext_plugid_core = NULL;

static uint16_t sdext_plugid_length(stat_entity_t *stat, void *hint) {
	/* If hint is given, count its pset. */
	if (hint) {
		sdext_plugid_hint_t *h = (sdext_plugid_hint_t *)hint;
		uint8_t count = 0;
		uint8_t i;

		for (i = 0; i < OPSET_STORE_LAST; i++)
			count += (h->pset[i]) ? 1 : 0;
		
		return sizeof(sdext_plugid_slot_t) * count +
			sizeof(sdext_plugid_t);
	}
	
	/* Count on-disk pset. */
	return sizeof(sdext_plugid_t) + sizeof(sdext_plugid_slot_t) *
		sdext_plugid_get_count((sdext_plugid_t *)stat_body(stat));
}

static errno_t sdext_plugid_open(stat_entity_t *stat, void *hint) {
	sdext_plugid_hint_t *h;
	sdext_plugid_t *ext;
	uint16_t i;
	
	aal_assert("vpf-1597", stat != NULL);
	aal_assert("vpf-1598", hint != NULL);

	h = (sdext_plugid_hint_t *)hint;
	ext = (sdext_plugid_t *)stat_body(stat);
	
	aal_memset(h, 0, sizeof(*h));
	
	for (i = 0; i < sdext_plugid_get_count(ext); i++) {
		rid_t mem, id;

		mem = sdext_plugid_get_member(ext, i);
		id = sdext_plugid_get_pid(ext, i);
		
		/* Check the member id valideness. */
		if (mem >= OPSET_STORE_LAST)
			return -EIO;
		
		/* Check if we met this member already. */
		if (h->pset[mem])
			return -EIO;

		/* Obtain the plugin by the id. */
		h->pset[mem] = sdext_plugid_core->pset_ops.find(mem, id);
		
		if (!h->pset[mem])
			h->pset[mem] = INVAL_PTR;
	}

	return 0;
}

#ifndef ENABLE_STAND_ALONE
static errno_t sdext_plugid_init(stat_entity_t *stat, void *hint) {
	sdext_plugid_hint_t *h;
	sdext_plugid_t *ext;
	uint8_t count = 0;
	rid_t mem;
	
	aal_assert("vpf-1600", stat != NULL);
	aal_assert("vpf-1599", hint != NULL);
	
	h = (sdext_plugid_hint_t *)hint;
	ext = (sdext_plugid_t *)stat_body(stat);
	
	for (mem = 0; mem < OPSET_STORE_LAST; mem++) {
		/* Find the plugin to be stored. */
		if (!h->pset[mem])
			continue;

		sdext_plugid_set_member(ext, count, mem);
		if (h->pset[mem] == INVAL_PTR)
			sdext_plugid_set_pid(ext, count, 0);
		else
			sdext_plugid_set_pid(ext, count, h->pset[mem]->id.id);
		
		count++;
	}
	
	sdext_plugid_set_count(ext, count);

	return 0;
}

extern errno_t sdext_plugid_check_struct(stat_entity_t *stat, 
					 uint8_t mode);

extern void sdext_plugid_print(stat_entity_t *stat, 
			       aal_stream_t *stream, 
			       uint16_t options);

#endif

static reiser4_sdext_ops_t sdext_plugid_ops = {
	.open	 	= sdext_plugid_open,
#ifndef ENABLE_STAND_ALONE
	.init	 	= sdext_plugid_init,
	.print   	= sdext_plugid_print,
	.check_struct   = sdext_plugid_check_struct,
#endif
	.length	 	= sdext_plugid_length
};

static reiser4_plug_t sdext_plugid_plug = {
	.cl    = class_init,
	.id    = {SDEXT_PLUG_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_STAND_ALONE
	.label = "sdext_plugid",
	.desc  = "Plugin id stat data extension for reiser4, ver. " VERSION,
#endif
	.o = {
		.sdext_ops = &sdext_plugid_ops
	}
};

static reiser4_plug_t *sdext_plugid_start(reiser4_core_t *c) {
	sdext_plugid_core = c;
	return &sdext_plugid_plug;
}

plug_register(sdext_plugid, sdext_plugid_start, NULL);

