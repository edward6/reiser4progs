/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#include <reiser4/reiser4.h>

#ifndef ENABLE_STAND_ALONE
reiser4_profile_t default_profile = {
	.name = "default",
	.pid  = {
		[0] = {
			.name  = "node",
			.type  = NODE_PLUG_TYPE,
			.value = NODE_LARGE_ID
		},
		[1] = {
			.name  = "nodeptr",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_NODEPTR40_ID
		},
		[2] = {
			.name  = "statdata",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_STATDATA40_ID
		},
		[3] = {
			.name  = "tail",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_TAIL40_ID
		},
		[4] = {
			.name  = "extent",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_EXTENT40_ID
		},
		[5] = {
			.name  = "cde",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_CDE_LARGE_ID
		},
		[6] = {
			.name  = "acl",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_ACL40_ID
		},
		[7] = {
			.name  = "hash",
			.type  = HASH_PLUG_TYPE,
			.value = HASH_R5_ID
		},
		[8] = {
			.name  = "policy",
			.type  = POLICY_PLUG_TYPE,
			.value = TAIL_SMART_ID
		},
		[9] = {
			.name  = "perm",
			.type  = PERM_PLUG_TYPE,
			.value = PERM_RWX_ID
		},
		[10] = {
			.name  = "regular",
			.type  = OBJECT_PLUG_TYPE,
			.value = OBJECT_FILE40_ID
		},
		[11] = {
			.name  = "directory",
			.type  = OBJECT_PLUG_TYPE,
			.value = OBJECT_DIR40_ID
		},
		[12] = {
			.name  = "symlink",
			.type  = OBJECT_PLUG_TYPE,
			.value = OBJECT_SYM40_ID
		},
		[13] = {
			.name  = "special",
			.type  = OBJECT_PLUG_TYPE,
			.value = OBJECT_SPCL40_ID
		},
		[14] = {
			.name  = "format",
			.type  = FORMAT_PLUG_TYPE,
			.value = FORMAT_REISER40_ID
		},
		[15] = {
			.name  = "oid",
			.type  = OID_PLUG_TYPE,
			.value = OID_REISER40_ID
		},
		[16] = {
			.name  = "alloc",
			.type  = ALLOC_PLUG_TYPE,
			.value = ALLOC_REISER40_ID
		},
		[17] = {
			.name  = "journal",
			.type  = JOURNAL_PLUG_TYPE,
			.value = JOURNAL_REISER40_ID
		},
		[18] = {
			.name  = "key",
			.type  = KEY_PLUG_TYPE,
			.value = KEY_LARGE_ID
		}
	}
};
#endif

reiser4_pid_t *reiser4_profile_pid(const char *type) {
	unsigned i, pids;

	aal_assert("umka-1877", type != NULL);

	pids = sizeof(default_profile.pid) /
		sizeof(reiser4_pid_t);
	
	for (i = 0; i < pids; i++) {
		reiser4_pid_t *pid = &default_profile.pid[i];

		if (aal_strlen(pid->name) != aal_strlen(type))
			continue;

		if (!aal_strncmp(pid->name, type, aal_strlen(type)))
			return pid;
	}

	return NULL;
}

uint64_t reiser4_profile_value(const char *type) {
	reiser4_pid_t *pid;

	aal_assert("umka-1879", type != NULL);
	
	if (!(pid = reiser4_profile_pid(type)))
		return INVAL_PID;

	return pid->value;
}

#ifndef ENABLE_STAND_ALONE
errno_t reiser4_profile_override(const char *type,
				 const char *name)
{
	reiser4_pid_t *pid;
	reiser4_plug_t *plug;

	aal_assert("umka-923", type != NULL);
	aal_assert("umka-924", name != NULL);

	if (!(pid = reiser4_profile_pid(type)))
		return -EINVAL;

	if (!(plug = reiser4_factory_nfind((char *)name))) {
		aal_exception_error("Can't find plugin by name "
				    "\"%s\".", name);
		return -EINVAL;
	}

	if (pid->type != plug->id.type) {
		aal_exception_error("Can't override plugins of "
				    "different types.");
		return -EINVAL;
	}

	pid->value = plug->id.id;
	return 0;
}
#endif
