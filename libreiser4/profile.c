/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#ifndef ENABLE_STAND_ALONE

#include <reiser4/libreiser4.h>

/* All default plugin ids. This is used for getting plugin id if it cannot be
   obtained by usual way (get from disk structures, etc.). All these may be
   chnaged. */

reiser4_profile_t defprof = {
	.pid = {
		[PROF_FORMAT] = {
			.name  = "format",
			.type  = FORMAT_PLUG_TYPE,
			.id = FORMAT_REISER40_ID,
		},
		[PROF_JOURNAL] = {
			.name  = "journal",
			.type  = JOURNAL_PLUG_TYPE,
			.id = JOURNAL_REISER40_ID,
		},
		[PROF_OID] = {
			.name  = "oid",
			.type  = OID_PLUG_TYPE,
			.id = OID_REISER40_ID,
		},
		[PROF_ALLOC] = {
			.name  = "alloc",
			.type  = ALLOC_PLUG_TYPE,
			.id = ALLOC_REISER40_ID,
		},
		[PROF_KEY] = {
			.name  = "key",
			.type  = KEY_PLUG_TYPE,
			.id = KEY_LARGE_ID,
		},
		[PROF_NODE] = {
			.name  = "node",
			.type  = NODE_PLUG_TYPE,
			.id = NODE_REISER40_ID,
		},
		[PROF_STAT] = {
			.name  = "statdata",
			.type  = ITEM_PLUG_TYPE,
			.id = ITEM_STAT40_ID,
		},
		[PROF_NODEPTR] = {
			.name  = "nodeptr",
			.type  = ITEM_PLUG_TYPE,
			.id = ITEM_NODEPTR40_ID,
		},
		[PROF_DIRITEM] = {
			.name  = "direntry",
			.type  = ITEM_PLUG_TYPE,
			.id = ITEM_CDE40_ID,
		},
		[PROF_TAIL] = {
			.name  = "tail",
			.type  = ITEM_PLUG_TYPE,
			.id = ITEM_TAIL40_ID,
		},
		[PROF_EXTENT] = {
			.name  = "extent",
			.type  = ITEM_PLUG_TYPE,
			.id = ITEM_EXTENT40_ID,
		},
		[PROF_ACL] = {
			.name  = "acl",
			.type  = ITEM_PLUG_TYPE,
			.id = ITEM_ACL40_ID,
		},
		[PROF_PERM] = {
			.name  = "permission",
			.type  = PERM_PLUG_TYPE,
			.id = PERM_RWX_ID,
		},
		[PROF_REG] = {
			.name  = "regular",
			.type  = OBJECT_PLUG_TYPE,
			.id = OBJECT_REG40_ID,
		},
		[PROF_DIR] = {
			.name  = "directory",
			.type  = OBJECT_PLUG_TYPE,
			.id = OBJECT_DIR40_ID,
		},
		[PROF_SYM] = {
			.name  = "symlink",
			.type  = OBJECT_PLUG_TYPE,
			.id = OBJECT_SYM40_ID,
		},
		[PROF_SPL] = {
			.name  = "special",
			.type  = OBJECT_PLUG_TYPE,
			.id = OBJECT_SPL40_ID,
		},
		[PROF_HASH] = {
			.name  = "hash",
			.type  = HASH_PLUG_TYPE,
			.id = HASH_R5_ID,
		},
		[PROF_FIBRE] = {
			.name  = "fibration",
			.type  = FIBRE_PLUG_TYPE,
			.id = FIBRE_EXT_1_ID,
		},
		[PROF_POLICY] = {
			.name  = "formatting",
			.type  = POLICY_PLUG_TYPE,
			.id = TAIL_SMART_ID,
		}
	},
	.mask = 0
};

void reiser4_profile_print(aal_stream_t *stream) {
	rid_t i;
	
	for (i = 0; i < PROF_LAST; i++) {
		reiser4_plug_t *plug;
		uint32_t width;

		width = 12 - aal_strlen(defprof.pid[i].name);

		plug = reiser4_factory_ifind(defprof.pid[i].type, 
					     defprof.pid[i].id);
		
		if (plug) {
			aal_stream_format(stream, "%s:%*s\"%s\" (id:0x%x "
					  "type:0x%x)\n", defprof.pid[i].name,
					  width - 1, " ", plug->label, 
					  plug->id.id, plug->id.type);
		} else {
			aal_stream_format(stream, "%s:%*s\"absent (id:0x%x "
					  "type:0x%x)\"\n", defprof.pid[i].name,
					  width - 1, " ", defprof.pid[i].id,
					  defprof.pid[i].type);
		}
	}
}

/* Overrides plugin id by @id found by @name. */
errno_t reiser4_profile_override(const char *slot, const char *name) {
	reiser4_plug_t *plug;
	uint8_t i;
	
	aal_assert("umka-924", slot != NULL);
	aal_assert("umka-923", name != NULL);

	for (i = 0; i < PROF_LAST; i++) {
		if (aal_strlen(defprof.pid[i].name) != aal_strlen(slot))
			continue;

		if (!aal_strncmp(defprof.pid[i].name, slot, aal_strlen(slot)))
			break;
	}
	
	if (i == PROF_LAST) {
		aal_error("Can't find a profile slot for the \"%s\".", slot);
		return -EINVAL;
	}
	
	if (!(plug = reiser4_factory_nfind((char *)name))) {
		aal_error("Can't find a plugin by the name \"%s\".", name);
		return -EINVAL;
	}

	if (defprof.pid[i].type != plug->id.type) {
		aal_error("Can't override profile slot \"%s\" by the found "
			  "plugin \"%s\": a plugin of another type is found.",
			  slot, name);
		return -EINVAL;
	}

	defprof.pid[i].id = plug->id.id;
	aal_set_bit(&defprof.mask, i);
	
	return 0;
}

bool_t reiser4_profile_overridden(rid_t id) {
	aal_assert("vpf-1509", id < PROF_LAST);
	return aal_test_bit(&defprof.mask, id);
}

/* Find the plugin from the profile slot pointer by @id. */
inline reiser4_plug_t *reiser4_profile_plug(rid_t id) {
	reiser4_plug_t *plug;
	
	aal_assert("vpf-1591", id < PROF_LAST);
	
	if (!(plug = reiser4_factory_ifind(defprof.pid[id].type, 
					   defprof.pid[id].id)))
	{
		aal_bug("vpf-1607", "Failed to find a plugin from the "
			"reiser4porgs profile, type (%s), id(%u)",
			defprof.pid[id].name, defprof.pid[id].id);
	}
	
	return plug;
	
}
#endif
