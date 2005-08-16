/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#ifndef ENABLE_MINIMAL

#include <reiser4/libreiser4.h>

/* All default plugin ids. This is used for getting plugin id if it cannot be
   obtained by usual way (get from disk structures, etc.). All these may be
   chnaged. */

reiser4_profile_t defprof = {
	.pid = {
		[PROF_FORMAT] = {
			.name  = "format",
			.id = {FORMAT_REISER40_ID, 0, FORMAT_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_JOURNAL] = {
			.name  = "journal",
			.id = {JOURNAL_REISER40_ID, 0, JOURNAL_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_OID] = {
			.name  = "oid",
			.id = {OID_REISER40_ID, 0, OID_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_ALLOC] = {
			.name  = "alloc",
			.id = {ALLOC_REISER40_ID, 0, ALLOC_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_KEY] = {
			.name  = "key",
			.id = {KEY_LARGE_ID, 0, KEY_PLUG_TYPE},
			.hidden = 0,
		},
		[PROF_NODE] = {
			.name  = "node",
			.id = {NODE_REISER40_ID, 0, NODE_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_STAT] = {
			.name  = "statdata",
			.id = {ITEM_STAT40_ID, STAT_ITEM, ITEM_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_NODEPTR] = {
			.name  = "nodeptr",
			.id = {ITEM_NODEPTR40_ID, PTR_ITEM, ITEM_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_DIRITEM] = {
			.name  = "direntry",
			.id = {ITEM_CDE40_ID, DIR_ITEM, ITEM_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_TAIL] = {
			.name  = "tail",
			.id = {ITEM_PLAIN40_ID, TAIL_ITEM, ITEM_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_EXTENT] = {
			.name  = "extent",
			.id = {ITEM_EXTENT40_ID, EXTENT_ITEM, ITEM_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_CTAIL] = {
			.name = "compression tail",
			.id = {ITEM_CTAIL40_ID, CTAIL_ITEM, ITEM_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_CREATE] = {
			.name  = "create",
			.id = {OBJECT_REG40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
			.hidden = 0,
		},
		[PROF_MKDIR] = {
			.name  = "mkdir",
			.id = {OBJECT_DIR40_ID, DIR_OBJECT, OBJECT_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_SYMLINK] = {
			.name  = "mksym",
			.id = {OBJECT_SYM40_ID, SYM_OBJECT, OBJECT_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_MKNODE] = {
			.name  = "mknode",
			.id = {OBJECT_SPL40_ID, SPL_OBJECT, OBJECT_PLUG_TYPE},
			.hidden = 1,
		},
		[PROF_COMPRESS] = {
			.name = "compression",
			.id = {COMPRESS_NONE_ID, 0, COMPRESS_PLUG_TYPE},
			.hidden = 0,
		},
		[PROF_CRYPTO] = {
			.name = "crypto",
			.id = {CRYPTO_NONE_ID, 0, CRYPTO_PLUG_TYPE},
			.hidden = 0,
		},
		[PROF_HASH] = {
			.name  = "hash",
			.id = {HASH_R5_ID, 0, HASH_PLUG_TYPE},
			.hidden = 0,
		},
		[PROF_FIBRE] = {
			.name  = "fibration",
			.id = {FIBRE_EXT_1_ID, 0, FIBRE_PLUG_TYPE},
			.hidden = 0,
		},
		[PROF_POLICY] = {
			.name  = "formatting",
			.id = {TAIL_SMART_ID, 0, POLICY_PLUG_TYPE},
			.hidden = 0,
		},
		[PROF_CLUSTER] = {
			.name = "cluster",
			.id = {CLUSTER_64K_ID, CLUSTER_PLUG_TYPE, PARAM_PLUG_TYPE},
			.hidden = 0,
		}
	},
	.mask = 0
};

void reiser4_profile_print(aal_stream_t *stream) {
	rid_t i;
	
	for (i = 0; i < PROF_LAST; i++) {
		reiser4_plug_t *plug;
		uint32_t width;

		/* skip hidden ones. */
		if (defprof.pid[i].hidden)
			continue;
		
		width = 12 - aal_strlen(defprof.pid[i].name);

		plug = reiser4_factory_ifind(defprof.pid[i].id.type, 
					     defprof.pid[i].id.id);
		
		if (plug) {
			aal_stream_format(stream, "%s:%*s\"%s\" (id:0x%x "
					  "type:0x%x)\n", defprof.pid[i].name,
					  width - 1, " ", plug->label, 
					  plug->id.id, plug->id.type);
		} else {
			aal_stream_format(stream, "%s:%*s\"absent (id:0x%x "
					  "type:0x%x)\"\n", defprof.pid[i].name,
					  width - 1, " ", defprof.pid[i].id.id,
					  defprof.pid[i].id.type);
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
		/* skip hidden ones. */
		if (defprof.pid[i].hidden)
			continue;
		
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

	if (defprof.pid[i].id.type != plug->id.type) {
		aal_error("Can't override profile slot \"%s\" by the found "
			  "plugin \"%s\": a plugin of another type is found.",
			  slot, name);
		return -EINVAL;
	}

	if (defprof.pid[i].id.group != plug->id.group) {
		aal_error("Can't override profile slot \"%s\" by the found "
			  "plugin \"%s\": a plugin of the same type but of "
			  "another group is found.", slot, name);
		return -EINVAL;
	}

	defprof.pid[i].id.id = plug->id.id;
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
	
	if (!(plug = reiser4_factory_ifind(defprof.pid[id].id.type, 
					   defprof.pid[id].id.id)))
	{
		aal_bug("vpf-1607", "Failed to find a plugin from the "
			"reiser4porgs profile, type (%s), id(%u)",
			defprof.pid[id].name, defprof.pid[id].id.id);
	}
	
	return plug;
	
}
#endif
