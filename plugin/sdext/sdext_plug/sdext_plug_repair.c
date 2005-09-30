/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt_repair.c -- large time stat data extension plugin recovery code. */

#ifndef ENABLE_MINIMAL
#include "sdext_plug.h"
#include <repair/plugin.h>

#include "sdext_plug.h"

#define PSET_MEMBER_LEN 10

char *opset_name[OPSET_STORE_LAST] = {
	[OPSET_OBJ]	= "object",
	[OPSET_DIR]	= "directory",
	[OPSET_PERM]	= "permission",
	[OPSET_POLICY]	= "formatting",
	[OPSET_HASH]	= "hash",
	[OPSET_FIBRE]	= "fibration",
	[OPSET_STAT]	= "statdata",
	[OPSET_DIRITEM] = "diritem",
	[OPSET_CRYPTO]	= "crypto",
	[OPSET_DIGEST]	= "digest",
	[OPSET_COMPRESS]= "compress",
	[OPSET_CMODE]	= "compressMode",
	[OPSET_CLUSTER] = "cluster",
	[OPSET_CREATE]	= "create"
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
	void *dst;
	
	ext = (sdext_plug_t *)stat_body(stat);
	count = sdext_plug_get_count(ext);
	place = stat->place;
	
	if (count > OPSET_STORE_LAST) {
		fsck_mess("Node (%llu), item (%u), [%s]: does not look "
			  "like a valid SD plugin set extention: wrong "
			  "pset member count detected (%u).", 
			  place_blknr(place), place->pos.item,
			  print_key(sdext_plug_core, &place->key), count);

		return RE_FATAL;
	}
	
	len = sdext_plug_length(stat, NULL);

	if (len == 0 || stat->offset + len > place->len) {
		fsck_mess("Node (%llu), item (%u), [%s]: does not look like "
			  "a valid SD plugin set extention: wrong pset member "
			  "count detected (%u).", 
			  place_blknr(place), place->pos.item,
			  print_key(sdext_plug_core, &place->key), count);
		return RE_FATAL;
	}
	    
	aal_memset(&plugh, 0, sizeof(plugh));
	remove = 0;
	
	for (i = 0; i < count; i++) {
		rid_t mem, id;

		mem = sdext_plug_get_member(ext, i);
		id = sdext_plug_get_pid(ext, i);

		if (mem >= OPSET_STORE_LAST) {
			/* Unknown member. */
			fsck_mess("Node (%llu), item (%u), [%s]: the slot (%u) "
				  "contains the invalid opset member (%u).",
				  place_blknr(place), place->pos.item,
				  print_key(sdext_plug_core, &place->key),
				  i, mem);

			rmmask |= (1 << i);
			remove++;
		} else if (metmask & (1 << mem)) {
			/* Was met already. */
			fsck_mess("Node (%llu), item (%u), [%s]: the slot (%u) "
				  "contains the opset member (%s) that was met "
				  "already.",place_blknr(place),place->pos.item,
				  print_key(sdext_plug_core, &place->key),
				  i, opset_name[mem]);

			rmmask |= (1 << i);
			remove++;
		} else {
			metmask |= (1 << i);
			
			/* Obtain the plugin. */
			plugh.plug[mem] = 
				sdext_plug_core->pset_ops.find(mem, id);

			/* Check if the member is valid. */
			if (plugh.plug[mem] == INVAL_PTR) {
				fsck_mess("Node (%llu), item (%u), [%s]: the "
					  "slot (%u) contains the invalid "
					  "opset member (%s), id (%u).",
					  place_blknr(place), place->pos.item, 
					  print_key(sdext_plug_core, &place->key),
					  i, opset_name[mem], id);
				
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
			  print_key(sdext_plug_core, &place->key),
			  stat->ext_plug->label);
		return RE_FATAL;
	}
	
	/* Removing broken slots. */
	fsck_mess("Node (%llu), item (%u), [%s]: removing broken slots.",
		  place_blknr(place), place->pos.item, 
		  print_key(sdext_plug_core, &place->key));
	
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

	aal_assert("vpf-1603", stat != NULL);
	aal_assert("vpf-1604", stream != NULL);
	
	ext = (sdext_plug_t *)stat_body(stat);

	if (sizeof(sdext_plug_t) + sizeof(sdext_plug_slot_t) > 
	    stat->place->len - stat->offset)
	{
		aal_stream_format(stream, "No enough space (%u bytes) "
				  "for the pset extention body.\n", 
				  stat->place->len - stat->offset);
		return;
	}

	count = (stat->place->len - stat->offset - sizeof(sdext_plug_t)) / 
		sizeof(sdext_plug_slot_t);
	
	if (count >= sdext_plug_get_count(ext)) {
		count = sdext_plug_get_count(ext);
		aal_stream_format(stream, "Pset count: \t%u\n", count);
	} else {
		aal_stream_format(stream, "Pset count: \t%u (fit to place length "
				  "%u)\n", sdext_plug_get_count(ext), count);
	}


	for (i = 0; i < count; i++) {
		rid_t mem, id;

		mem = sdext_plug_get_member(ext, i);
		id = sdext_plug_get_pid(ext, i);
		
		if (mem < OPSET_STORE_LAST) {
			plug = sdext_plug_core->pset_ops.find(mem, id);
			aal_stream_format(stream, "    %*s : id = %u",
					  PSET_MEMBER_LEN, opset_name[mem], id);
		} else {
			plug = NULL;
			aal_stream_format(stream, "    UNKN(0x%*x):"
					  " id = %u", 4, mem, id);
		}
		
		if (plug && plug != INVAL_PTR) 
			aal_stream_format(stream, " (%s)\n", plug->label);
		else
			aal_stream_format(stream, "\n");
	}
}

#endif
