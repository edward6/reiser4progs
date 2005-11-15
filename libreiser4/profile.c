/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */


#include <reiser4/libreiser4.h>
#include <misc/misc.h>

#define INVAL_TYPE ((uint8_t)~0)

/* All default plugin ids. This is used for getting plugin id if it cannot be
   obtained by usual way (get from disk structures, etc.). All these may be
   chnaged. */
reiser4_profile_t defprof = {
	.pid = {
		[PROF_OBJ] = {
#ifndef ENABLE_MINIMAL
			.name  = "file",
			.hidden = 1,
			.max = OBJECT_LAST_ID,
#endif
			.id = {INVAL_PID, INVAL_TYPE, OBJECT_PLUG_TYPE},
		},
		[PROF_DIR] = {
#ifndef ENABLE_MINIMAL
			.name  = "dir",
			.hidden = 1,
			.max = 1,
#endif
			.id = {INVAL_PID, 0, PARAM_PLUG_TYPE},
		},
		[PROF_REGFILE] = {
#ifndef ENABLE_MINIMAL
			.name  = "regfile",
			.hidden = 1,
			.max = OBJECT_LAST_ID,
#endif
			.id = {OBJECT_REG40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
		},
		[PROF_DIRFILE] = {
#ifndef ENABLE_MINIMAL
			.name  = "dirfile",
			.hidden = 1,
			.max = OBJECT_LAST_ID,
#endif
			.id = {OBJECT_DIR40_ID, DIR_OBJECT, OBJECT_PLUG_TYPE},
		},
		[PROF_SYMFILE] = {
#ifndef ENABLE_MINIMAL
			.name  = "symfile",
			.hidden = 1,
			.max = OBJECT_LAST_ID,
#endif
			.id = {OBJECT_SYM40_ID, SYM_OBJECT, OBJECT_PLUG_TYPE},
		},
		[PROF_SPLFILE] = {
#ifndef ENABLE_MINIMAL
			.name  = "splfile",
			.hidden = 1,
			.max = OBJECT_LAST_ID,
#endif
			.id = {OBJECT_SPL40_ID, SPL_OBJECT, OBJECT_PLUG_TYPE},
		},
		[PROF_CREATE] = {
#ifndef ENABLE_MINIMAL
			.name  = "create",
			.hidden = 0,
			.max = OBJECT_LAST_ID,
#endif
			.id = {OBJECT_REG40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
		},
		[PROF_FORMAT] = {
#ifndef ENABLE_MINIMAL
			.name  = "format",
			.hidden = 1,
			.max = FORMAT_LAST_ID,
#endif
			.id = {FORMAT_REISER40_ID, 0, FORMAT_PLUG_TYPE},
		},
		[PROF_JOURNAL] = {
#ifndef ENABLE_MINIMAL
			.name  = "journal",
			.hidden = 1,
			.max = JOURNAL_LAST_ID,
#endif
			.id = {JOURNAL_REISER40_ID, 0, JOURNAL_PLUG_TYPE},
		},
		[PROF_OID] = {
#ifndef ENABLE_MINIMAL
			.name  = "oid",
			.hidden = 1,
			.max = OID_LAST_ID,
#endif
			.id = {OID_REISER40_ID, 0, OID_PLUG_TYPE},
		},
		[PROF_ALLOC] = {
#ifndef ENABLE_MINIMAL
			.name  = "alloc",
			.hidden = 1,
			.max = ALLOC_LAST_ID,
#endif
			.id = {ALLOC_REISER40_ID, 0, ALLOC_PLUG_TYPE},
		},
		[PROF_KEY] = {
#ifndef ENABLE_MINIMAL
			.name  = "key",
			.hidden = 0,
			.max = KEY_LAST_ID,
#endif
			.id = {KEY_LARGE_ID, 0, KEY_PLUG_TYPE},
		},
		[PROF_NODE] = {
#ifndef ENABLE_MINIMAL
			.name  = "node",
			.hidden = 1,
			.max = NODE_LAST_ID,
#endif
			.id = {NODE_REISER40_ID, 0, NODE_PLUG_TYPE},
		},
		[PROF_COMPRESS] = {
#ifndef ENABLE_MINIMAL
			.name = "compress",
			.hidden = 0,
			.max = COMPRESS_LAST_ID,
#endif
			.id = {COMPRESS_NONE_ID, COMPRESS_PARAM, PARAM_PLUG_TYPE},
		},
		[PROF_CMODE] = {
#ifndef ENABLE_MINIMAL
			.name = "compressMode",
			.hidden = 0,
			.max = CMODE_LAST_ID,
#endif
			.id = {CMODE_SMART_ID, CMODE_PARAM, PARAM_PLUG_TYPE},
		},
		[PROF_CRYPTO] = {
#ifndef ENABLE_MINIMAL
			.name = "crypto",
			.hidden = 0,
			.max = CRYPTO_LAST_ID,
#endif
			.id = {CRYPTO_NONE_ID, CRYPTO_PARAM, PARAM_PLUG_TYPE},
		},
		[PROF_DIGEST] = {
#ifndef ENABLE_MINIMAL
			.name = "digest",
			.hidden = 0,
			.max = DIGEST_LAST_ID,
#endif
			.id = {DIGEST_NONE_ID, DIGEST_PARAM, PARAM_PLUG_TYPE},
		},
		[PROF_CLUSTER] = {
#ifndef ENABLE_MINIMAL
			.name = "cluster",
			.hidden = 0,
			.max = CLUSTER_LAST_ID,
#endif
			.id = {CLUSTER_64K_ID, CLUSTER_PARAM, PARAM_PLUG_TYPE},
		},
		[PROF_HASH] = {
#ifndef ENABLE_MINIMAL
			.name  = "hash",
			.hidden = 0,
			.max = HASH_LAST_ID,
#endif
			.id = {HASH_R5_ID, 0, HASH_PLUG_TYPE},
		},
		[PROF_FIBRE] = {
#ifndef ENABLE_MINIMAL
			.name  = "fibration",
			.hidden = 0,
			.max = FIBRE_LAST_ID,
#endif
			.id = {FIBRE_EXT_1_ID, 0, FIBRE_PLUG_TYPE},
		},
		[PROF_PERM] = {
#ifndef ENABLE_MINIMAL
			.name = "permission",
			.hidden = 1,
			.max = PERM_LAST_ID,
#endif
			.id = {PERM_RWX_ID, 0, PARAM_PLUG_TYPE},
		},
		[PROF_POLICY] = {
#ifndef ENABLE_MINIMAL
			.name  = "formatting",
			.hidden = 0,
			.max = TAIL_LAST_ID,
#endif
			.id = {TAIL_SMART_ID, 0, POLICY_PLUG_TYPE},
		},
		[PROF_STAT] = {
#ifndef ENABLE_MINIMAL
			.name = "statdata",
			.hidden = 1,
			.max = ITEM_LAST_ID,
#endif
			.id = {ITEM_STAT40_ID, STAT_ITEM, ITEM_PLUG_TYPE},
		},
		[PROF_DIRITEM] = {
#ifndef ENABLE_MINIMAL
			.name  = "direntry",
			.hidden = 1,
			.max = ITEM_LAST_ID,
#endif
			.id = {ITEM_CDE40_ID, DIR_ITEM, ITEM_PLUG_TYPE},
		},
		[PROF_NODEPTR] = {
#ifndef ENABLE_MINIMAL
			.name  = "nodeptr",
			.hidden = 1,
			.max = ITEM_LAST_ID,
#endif
			.id = {ITEM_NODEPTR40_ID, PTR_ITEM, ITEM_PLUG_TYPE},
		},
#ifndef ENABLE_MINIMAL
		[PROF_TAIL] = {
			.name  = "tail",
			.hidden = 1,
			.max = ITEM_LAST_ID,
			.id = {ITEM_PLAIN40_ID, TAIL_ITEM, ITEM_PLUG_TYPE},
		},
		[PROF_EXTENT] = {
			.name  = "extent",
			.hidden = 1,
			.max = ITEM_LAST_ID,
			.id = {ITEM_EXTENT40_ID, EXTENT_ITEM, ITEM_PLUG_TYPE},
		},
		[PROF_CTAIL] = {
			.name = "compressTail",
			.hidden = 1,
			.max = ITEM_LAST_ID,
			.id = {ITEM_CTAIL40_ID, CTAIL_ITEM, ITEM_PLUG_TYPE},
		},
#endif

	},
	.mask = 0
};

#ifndef ENABLE_MINIMAL
void reiser4_profile_print(aal_stream_t *stream) {
	rid_t i;
	
	for (i = 0; i < PROF_LAST; i++) {
		reiser4_plug_t *plug;
		uint32_t width;

		/* skip hidden ones. */
		if (defprof.pid[i].hidden)
			continue;
		
		width = 12 - aal_strlen(defprof.pid[i].name);

		if (defprof.pid[i].id.type != PARAM_PLUG_TYPE) {
			plug = reiser4_factory_ifind(defprof.pid[i].id.type,
						     defprof.pid[i].id.id);

			if (plug) {
				aal_stream_format(stream, "%s:%*s\"%s\" "
						  "(id:0x%x type:0x%x)\n", 
						  defprof.pid[i].name,
						  width - 1, " ", plug->label,
						  plug->id.id, plug->id.type);
			} else {
				aal_stream_format(stream, "%s:%*s\"absent\" "
						  "(id:0x%x type:0x%x)\n", 
						  defprof.pid[i].name, width - 1, 
						  " ", defprof.pid[i].id.id,
						  defprof.pid[i].id.type);
			}
		} else {
			aal_stream_format(stream, "%s:%*s 0x%x, max value 0x%x\n", 
					  defprof.pid[i].name, width - 1, 
					  " ", defprof.pid[i].id.id,
					  defprof.pid[i].max - 1);
		}
	}
}

/* Overrides plugin id by @id found by @name. */
errno_t reiser4_profile_override(const char *slot, const char *name) {
	reiser4_plug_t *plug;
	long long int val;
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
	
	if (defprof.pid[i].id.type == PARAM_PLUG_TYPE)  {
		if ((val = misc_str2long((char *)name, 10)) == INVAL_DIG) {
			aal_error("Invalid value \"%s\" is provided for the "
				  "profile slot \"%s\".", (char *)name,
				  defprof.pid[i].name);
			return -EINVAL;
		}

		if (val >= defprof.pid[i].max) {
			aal_error("Invalid value (%s) is provided for the "
				  "profile slot \"%s\". Maximum value is %u",
				  (char *)name, defprof.pid[i].name,
				   defprof.pid[i].max - 1);
			return -EINVAL;
		}

		defprof.pid[i].id.id = val;
	} else {	
		if (!(plug = reiser4_factory_nfind((char *)name))) {
			aal_error("Can't find a plugin by "
				  "the name \"%s\".", name);
			
			return -EINVAL;
		}

		if (defprof.pid[i].id.type != plug->id.type) {
			aal_error("Can't override profile slot \"%s\" by "
				  "the found plugin \"%s\": a plugin of "
				  "another type is found.", slot, name);
			return -EINVAL;
		}

		if (defprof.pid[i].id.group != plug->id.group) {
			aal_error("Can't override profile slot \"%s\" by "
				  "the found plugin \"%s\": a plugin of "
				  "the same type but of another group is "
				  "found.", slot, name);
			return -EINVAL;
		}
		
		defprof.pid[i].id.id = plug->id.id;
	}
	
	aal_set_bit(&defprof.mask, i);
	
	return 0;
}

bool_t reiser4_profile_overridden(rid_t id) {
	aal_assert("vpf-1509", id < PROF_LAST);
	return aal_test_bit(&defprof.mask, id);
}
#endif

/* Find the plugin from the profile slot pointer by @id. */
reiser4_plug_t *reiser4_profile_plug(rid_t id) {
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
