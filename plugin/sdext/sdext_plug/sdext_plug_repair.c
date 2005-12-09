/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt_repair.c -- large time stat data extension plugin recovery code. */

#ifndef ENABLE_MINIMAL
#include "sdext_plug.h"
#include <repair/plugin.h>

#include "sdext_plug.h"

#define PSET_MEMBER_LEN 14

char *pset_name[PSET_STORE_LAST] = {
	[PSET_OBJ]	= "object",
	[PSET_DIR]	= "directory",
	[PSET_PERM]	= "permission",
	[PSET_POLICY]	= "formatting",
	[PSET_HASH]	= "hash",
	[PSET_FIBRE]	= "fibration",
	[PSET_STAT]	= "statdata",
	[PSET_DIRITEM]	= "diritem",
	[PSET_CRYPTO]	= "crypto",
	[PSET_DIGEST]	= "digest",
	[PSET_COMPRESS]	= "compress",
	[PSET_CMODE]	= "compressMode",
	[PSET_CLUSTER]	= "cluster",
};

char *hset_name[HSET_LAST] = {
	/* The plugin to create children. */
	[HSET_CREATE]	= "heir_create",
	[HSET_HASH]	= "heir_hash",
	[HSET_FIBRE]	= "heir_fibration",
	[HSET_DIR_ITEM] = "heir_diritem",
};

errno_t sdext_plug_check_struct(stat_entity_t *stat, repair_hint_t *hint) {
	reiser4_place_t *place;
	uint64_t metmask = 0;
	uint64_t rmmask = 0;
	sdhint_plug_t plugh;
	sdext_plug_t *ext;
	uint16_t count, i;
	int32_t remove;
	uint32_t len;
	uint8_t last;
	int is_pset;
	void *dst;
	
	ext = (sdext_plug_t *)stat_body(stat);
	count = sdext_plug_get_count(ext);
	place = stat->place;
	
	is_pset = stat->plug->p.id.id == SDEXT_PSET_ID;
	last = is_pset ? PSET_STORE_LAST : HSET_LAST;
	
	if (count > last) {
		fsck_mess("Node (%llu), item (%u), [%s]: does not "
			  "look like a valid SD %s set extention: "
			  "wrong member count detected (%u).", 
			  place_blknr(place), place->pos.item,
			  print_key(sdext_pset_core, &place->key), 
			  is_pset ? "plugin" : "heir", count);

		return RE_FATAL;
	}
	
	len = sdext_plug_length(stat, NULL);

	if (len == 0 || stat->offset + len > place->len) {
		fsck_mess("Node (%llu), item (%u), [%s]: does not look like "
			  "a valid SD %s set extention: wrong member count "
			  "detected (%u).", place_blknr(place), place->pos.item,
			  print_key(sdext_pset_core, &place->key), 
			  is_pset ? "plugin" : "heir", count);
		return RE_FATAL;
	}
	    
	aal_memset(&plugh, 0, is_pset ? sizeof(sdhint_plug_t) : 
		   sizeof(sdhint_heir_t));
	remove = 0;
	
	for (i = 0; i < count; i++) {
		rid_t mem, id;

		mem = sdext_plug_get_member(ext, i);
		id = sdext_plug_get_pid(ext, i);

		if (mem >= last) {
			/* Unknown member. */
			fsck_mess("Node (%llu), item (%u), [%s]: the slot (%u) "
				  "contains the invalid %s set member (%u).",
				  place_blknr(place), place->pos.item,
				  print_key(sdext_pset_core, &place->key), 
				  i, is_pset ? "plugin" : "heir",mem);

			rmmask |= (1 << i);
			remove++;
		} else if (metmask & (1 << mem)) {
			/* Was met already. */
			fsck_mess("Node (%llu), item (%u), [%s]: the slot (%u) "
				  "contains the %s set member (%s) that was met "
				  "already.",place_blknr(place),place->pos.item,
				  print_key(sdext_pset_core, &place->key), i, 
				  is_pset ? "plugin" : "heir", is_pset ? 
				  pset_name[mem] : hset_name[mem]);

			rmmask |= (1 << i);
			remove++;
		} else {
			metmask |= (1 << i);
			
			/* Obtain the plugin. */
			plugh.plug[mem] = sdext_pset_core->pset_ops.find(
							mem, id, is_pset);

			/* Check if the member is valid. */
			if (plugh.plug[mem] == INVAL_PTR) {
				fsck_mess("Node (%llu), item (%u), [%s]: the "
					  "slot (%u) contains the invalid %s "
					  "set member (%s), id (%u).",
					  place_blknr(place), place->pos.item, 
					  print_key(sdext_pset_core, &place->key),
					  i, is_pset ? "plugin" : "heir", is_pset 
					  ? pset_name[mem] : hset_name[mem], id);
				
				rmmask |= (1 << i);
				remove++;
			}
		}
	}

	if (!rmmask) 
		return 0;
	
	/* Some broken slots are found. */
	if (hint->mode != RM_BUILD)
		return RE_FATAL;

	if (remove == count) {
		fsck_mess("Node (%llu), item (%u), [%s]: no slot left. Does "
			  "not look like a valid (%s) statdata extention.",
			  place_blknr(place), place->pos.item,
			  print_key(sdext_pset_core, &place->key),
			  stat->plug->p.label);
		return RE_FATAL;
	}
	
	/* Removing broken slots. */
	fsck_mess("Node (%llu), item (%u), [%s]: removing broken slots.",
		  place_blknr(place), place->pos.item, 
		  print_key(sdext_pset_core, &place->key));
	
	dst = stat_body(stat) + sizeof(sdext_plug_t);
	len -= sizeof(sdext_plug_t);
		
	for (i = 0; i < count; i++, dst += sizeof(sdext_plug_slot_t)) {
		len -= sizeof(sdext_plug_slot_t);
		
		if (!(rmmask & (1 << i)))
			continue;

		aal_memmove(dst, dst + sizeof(sdext_plug_slot_t), len);
		dst -=  sizeof(sdext_plug_slot_t);
	}
	
	sdext_plug_set_count(ext, count - remove);
	hint->len = remove * sizeof(sdext_plug_slot_t);
	
	return 0;
}

void sdext_plug_print(stat_entity_t *stat, 
		      aal_stream_t *stream, 
		      uint16_t options) 
{
	reiser4_plug_t *plug;
	sdext_plug_t *ext;
	uint16_t count, i;
	int is_pset;

	aal_assert("vpf-1603", stat != NULL);
	aal_assert("vpf-1604", stream != NULL);
	
	ext = (sdext_plug_t *)stat_body(stat);
	is_pset = stat->plug->p.id.id == SDEXT_PSET_ID;
	
	if (sizeof(sdext_plug_t) + sizeof(sdext_plug_slot_t) > 
	    stat->place->len - stat->offset)
	{
		aal_stream_format(stream, "No enough space (%u bytes) "
				  "for the %s set extention body.\n", 
				  stat->place->len - stat->offset,
				  is_pset ? "plugin" : "heir");
		return;
	}

	count = (stat->place->len - stat->offset - sizeof(sdext_plug_t)) / 
		sizeof(sdext_plug_slot_t);
	
	if (count >= sdext_plug_get_count(ext)) {
		count = sdext_plug_get_count(ext);
		aal_stream_format(stream, "%sset count: \t%u\n", 
				  is_pset ? "P" : "H", count);
	} else {
		aal_stream_format(stream, "%sset count: \t%u (fit to "
				  "place length %u)\n", is_pset ? "P" : "H",
				  sdext_plug_get_count(ext), count);
	}


	for (i = 0; i < count; i++) {
		rid_t mem, id;

		mem = sdext_plug_get_member(ext, i);
		id = sdext_plug_get_pid(ext, i);
		
		if (mem < (uint8_t)(is_pset ? PSET_STORE_LAST : HSET_LAST)) {
			plug = sdext_pset_core->pset_ops.find(mem, id, is_pset);
			aal_stream_format(stream, "    %*s : id = %u",
					  PSET_MEMBER_LEN, is_pset ? 
					  pset_name[mem] : hset_name[mem], id);
		} else {
			plug = NULL;
			aal_stream_format(stream, "%*sUNKN(0x%x) : id = %u", 
					  PSET_MEMBER_LEN - 5, "", mem, id);
		}
		
		if (plug && plug != INVAL_PTR) 
			aal_stream_format(stream, " (%s)\n", plug->label);
		else
			aal_stream_format(stream, "\n");
	}
}

#endif
