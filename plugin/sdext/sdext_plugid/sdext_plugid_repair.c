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
