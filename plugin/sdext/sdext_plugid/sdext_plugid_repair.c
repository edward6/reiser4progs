/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_lt_repair.c -- large time stat data extension plugin recovery code. */

#ifndef ENABLE_STAND_ALONE
#include "sdext_plugid.h"
#include <repair/plugin.h>

char *opset_name[OPSET_STORE_LAST] = {
	[OPSET_OBJ] =	  "object    ",
	[OPSET_DIR] =	  "directory ",
	[OPSET_PERM] =	  "permission",
	[OPSET_POLICY] =  "formatting",
	[OPSET_HASH] =	  "hash      ",
	[OPSET_FIBRE] =   "fibre     ",
	[OPSET_STAT] =	  "statdata  ",
	[OPSET_DENTRY] =  "dir entry ",
	[OPSET_CRYPTO] =  "crypto    ",
	[OPSET_DIGEST] =  "digest    ",
	[OPSET_COMPRES] = "compress  "
};

errno_t sdext_plugid_check_struct(stat_entity_t *stat, uint8_t mode) {
	sdext_plugid_hint_t plugs;
	sdext_plugid_t *ext;
	uint64_t mask = 0;
	uint8_t i, count;
//	errno_t res = 0;
	
	ext = (sdext_plugid_t *)stat_body(stat);
	count = sdext_plugid_get_count(ext);
	
	if (stat->offset + sdext_plugid_length(stat, NULL) < stat->place->len) {
		aal_error("Node (%llu), item (%u): does not look like a "
			  "valid plugid extention: wrong count of plugins "
			  "detected (%u).", place_blknr(stat->place),
			  stat->place->pos.item, count);
		return RE_FATAL;
	}
	    
	aal_memset(&plugs, 0, sizeof(plugs));

	for (i = 0; i < count; i++) {
		rid_t mem, id;

		mem = sdext_plugid_get_member(ext, i);
		id = sdext_plugid_get_pid(ext, i);

		if (mem >= OPSET_STORE_LAST) {
			/* Unknown member. */
			aal_error("Node (%llu), item (%u): the slot (%u) "
				  "contains the invalid opset member (%u).",
				  place_blknr(stat->place), 
				  stat->place->pos.item, i, mem);

			aal_set_bit(&mask, mem);
		} else if (plugs.pset[i]) {
			/* Was met already. */
			aal_error("Node (%llu), item (%u): the opset member "
				  "(%u) is encountered more then once.",
				  place_blknr(stat->place), 
				  stat->place->pos.item, mem);

			aal_set_bit(&mask, mem);
		} else {
			/* Obtain the plugin. */
			plugs.pset[mem] = 
				sdext_plugid_core->pset_ops.find(mem, id);

			if (!plugs.pset[mem]) {
				
			}
		}
	}

	if (!mask) 
		return 0;
	
	/* Some broken slots are found. */
	if (mode != RM_BUILD)
		return RE_FATAL;

	/* Removing broken slots. */
	aal_warn("Node (%llu), item (%u): removing broken slots.",
		 place_blknr(stat->place), stat->place->pos.item);
	
	return 0;
}

void sdext_plugid_print(stat_entity_t *stat, 
			aal_stream_t *stream, 
			uint16_t options) 
{
	reiser4_plug_t *plug;
	sdext_plugid_t *ext;
	uint16_t i;

	aal_assert("vpf-1603", ext != NULL);
	aal_assert("vpf-1604", stream != NULL);
	
	ext = (sdext_plugid_t *)stat_body(stat);

	aal_stream_format(stream, "Pset count: \t%u\n",
			  sdext_plugid_get_count(ext));

	for (i = 0; i < sdext_plugid_get_count(ext); i++) {
		rid_t mem, id;

		mem = sdext_plugid_get_member(ext, i);
		id = sdext_plugid_get_pid(ext, i);
		
		plug = sdext_plugid_core->pset_ops.find(mem, id);

		aal_stream_format(stream, "    %s : id = %u",
				  opset_name[mem], id);

		if (plug) 
			aal_stream_format(stream, " (%s)\n", plug->label);
		else
			aal_stream_format(stream, "\n");
	}
}

#endif
