/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#include <reiser4/libreiser4.h>

#ifndef ENABLE_STAND_ALONE

/* All default plugin ids. This is used for getting plugin id if it cannot be
   obtained by usual way (get from disk structures, etc.). All these may be
   chnaged. */

struct reiser4_pid {
	char *name;
	rid_t type;
	rid_t id;
};

typedef struct reiser4_pid reiser4_pid_t;

reiser4_pid_t defpid[PROF_LAST] = {
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
	[PROF_STATDATA] = {
		.name  = "statdata",
		.type  = ITEM_PLUG_TYPE,
		.id = ITEM_STATDATA40_ID,
	},
	[PROF_NODEPTR] = {
		.name  = "nodeptr",
		.type  = ITEM_PLUG_TYPE,
		.id = ITEM_NODEPTR40_ID,
	},
	[PROF_DIRENTRY] = {
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
		.name  = "perm",
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
		.name  = "fibre",
		.type  = FIBRE_PLUG_TYPE,
		.id = FIBRE_DOT_O_ID,
	},
	[PROF_POLICY] = {
		.name  = "policy",
		.type  = POLICY_PLUG_TYPE,
		.id = TAIL_SMART_ID,
	}
};

static reiser4_profile_t defprof[PROF_LAST];

void reiser4_profile_init() {
	rid_t i;
	
	aal_memset(&defprof, 0, sizeof(defprof));

	for (i = 0; i < PROF_LAST; i++) {
		defprof[i].plug = reiser4_factory_ifind(defpid[i].type,
							defpid[i].id);
	}
}

void reiser4_profile_print(aal_stream_t *stream) {
	rid_t i;
	
	for (i = 0; i < PROF_LAST; i++) {
		uint32_t width;

		width = 12 - aal_strlen(defpid[i].name);

		if (defprof[i].plug) {
			aal_stream_format(stream, "%s:%*s\"%s\" (id:0x%x type:0x%x)"
					  "\n", defpid[i].name, width - 1, " ",
					  defprof[i].plug->label,
					  defprof[i].plug->id.id,
					  defprof[i].plug->id.type);
		} else {
			aal_stream_format(stream, "%s:%*s\"absent (id:0x%x type:0x%x)"
					  "\"\n", defpid[i].name, width - 1, " ",
					  defpid[i].id, defpid[i].type);
		}
	}
}

rid_t reiser4_profile_index(char *name) {
	rid_t i;
	
	aal_assert("vpf-1590", name != NULL);
	
	for (i = 0; i < PROF_LAST; i++) {
		if (aal_strlen(defpid[i].name) != aal_strlen(name))
			continue;

		if (!aal_strncmp(defpid[i].name, name, aal_strlen(name)))
			return i;
	}
	
	return -1;
}

/* Overrides plugin id by @id found by @name. */
errno_t reiser4_profile_override(rid_t index, const char *name) {
	reiser4_plug_t *plug;

	aal_assert("umka-924", index < PROF_LAST);
	aal_assert("umka-923", name != NULL);

	if (!(plug = reiser4_factory_nfind((char *)name))) {
		aal_error("Can't find plugin by name \"%s\".", name);
		return -EINVAL;
	}

	if (defpid[index].type != plug->id.type) {
		aal_error("Can't override plugin by name \"%s\": a "
			  "plugin of another type is found.", name);
		return -EINVAL;
	}

	defprof[index].plug = plug;

	return 0;
}

/* Overrides plugin id by @pid found by @name. */
errno_t reiser4_profile_set(rid_t index, rid_t id) {
	reiser4_plug_t *plug;

	aal_assert("vpf-1509", index < PROF_LAST);

	if (!(plug = reiser4_factory_ifind(defpid[index].type, id))) {
		aal_error("Can't find plugin of the type %d by "
			  "its id %u.", defpid[index].type, id);
		return -EINVAL;
	}

	defprof[index].plug = plug;
	
	return 0;
}

/* Overrides plugin id by @pid found by @name. */
inline reiser4_plug_t *reiser4_profile_plug(rid_t index) {
	aal_assert("vpf-1509", index < PROF_LAST);
	aal_assert("vpf-1591", defprof[index].plug != NULL);
	return defprof[index].plug;
}

void reiser4_profile_set_flag(rid_t index, uint8_t flag) {
	aal_assert("vpf-1558", index < PROF_LAST);
	defprof[index].flags |= (1 << flag);
}

bool_t reiser4_profile_get_flag(rid_t index, uint8_t flag) {
	aal_assert("vpf-1557", index < PROF_LAST);
	return (defprof[index].flags & (1 << flag));
}

#endif
