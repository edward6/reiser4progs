/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   param.c -- reiser4 parameter functions. */

#include <reiser4/libreiser4.h>

#ifndef ENABLE_STAND_ALONE
/* All default plugin ids. This is used for getting plugin id if it cannot be
   obtained by usual way (get from disk structures, etc.). All these may be
   chnaged. */
reiser4_param_t default_param = {
	.name = "default",
	.pid  = {
		[0] = {
			.name  = "node",
			.type  = NODE_PLUG_TYPE,
			.value = NODE_REISER40_ID,
			.flags = 0
		},
		[1] = {
			.name  = "nodeptr",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_NODEPTR40_ID,
			.flags = 0
		},
		[2] = {
			.name  = "statdata",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_STATDATA40_ID,
			.flags = 0
		},
		[3] = {
			.name  = "tail",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_TAIL40_ID,
			.flags = 0
		},
		[4] = {
			.name  = "extent",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_EXTENT40_ID,
			.flags = 0
		},
		[5] = {
			.name  = "direntry",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_CDE40_ID,
			.flags = 0
		},
		[6] = {
			.name  = "acl",
			.type  = ITEM_PLUG_TYPE,
			.value = ITEM_ACL40_ID,
			.flags = 0
		},
		[7] = {
			.name  = "hash",
			.type  = HASH_PLUG_TYPE,
			.value = HASH_R5_ID,
			.flags = 0
		},
		[8] = {
			.name  = "policy",
			.type  = POLICY_PLUG_TYPE,
			.value = TAIL_SMART_ID,
			.flags = 0
		},
		[9] = {
			.name  = "perm",
			.type  = PERM_PLUG_TYPE,
			.value = PERM_RWX_ID,
			.flags = 0
		},
		[10] = {
			.name  = "regular",
			.type  = OBJECT_PLUG_TYPE,
			.value = OBJECT_REG40_ID,
			.flags = 0
		},
		[11] = {
			.name  = "directory",
			.type  = OBJECT_PLUG_TYPE,
			.value = OBJECT_DIR40_ID,
			.flags = 0
		},
		[12] = {
			.name  = "symlink",
			.type  = OBJECT_PLUG_TYPE,
			.value = OBJECT_SYM40_ID,
			.flags = 0
		},
		[13] = {
			.name  = "special",
			.type  = OBJECT_PLUG_TYPE,
			.value = OBJECT_SPL40_ID,
			.flags = 0
		},
		[14] = {
			.name  = "format",
			.type  = FORMAT_PLUG_TYPE,
			.value = FORMAT_REISER40_ID,
			.flags = 0
		},
		[15] = {
			.name  = "oid",
			.type  = OID_PLUG_TYPE,
			.value = OID_REISER40_ID,
			.flags = 0
		},
		[16] = {
			.name  = "alloc",
			.type  = ALLOC_PLUG_TYPE,
			.value = ALLOC_REISER40_ID,
			.flags = 0
		},
		[17] = {
			.name  = "journal",
			.type  = JOURNAL_PLUG_TYPE,
			.value = JOURNAL_REISER40_ID,
			.flags = 0
		},
		[18] = {
			.name  = "key",
			.type  = KEY_PLUG_TYPE,
			.value = KEY_LARGE_ID,
			.flags = 0
		}
	}
};

/* Returns plugin param instnace by its name. */
reiser4_pid_t *reiser4_param_pid(const char *name) {
	uint32_t i;

	aal_assert("umka-1877", name != NULL);

	for (i = 0; i < PARAM_NR; i++) {
		reiser4_pid_t *pid = &default_param.pid[i];

		if (aal_strlen(pid->name) != aal_strlen(name))
			continue;

		if (!aal_strncmp(pid->name, name,
				 aal_strlen(name)))
		{
			return pid;
		}
	}

	return NULL;
}

/* Return plugin id by param name. */
uint64_t reiser4_param_value(const char *name) {
	reiser4_pid_t *pid;

	aal_assert("umka-1879", name != NULL);
	
	if (!(pid = reiser4_param_pid(name)))
		return INVAL_PID;

	return pid->value;
}

/* Overrides plugin id by @value found by @name. */
errno_t reiser4_param_override(const char *name, const char *value) {
	reiser4_pid_t *pid;
	reiser4_plug_t *plug;

	aal_assert("umka-924", name != NULL);
	aal_assert("umka-923", value != NULL);

	if (!(pid = reiser4_param_pid(name))) {
		aal_error("Can't find param \"%s\".", name);
		return -EINVAL;
	}

	if (!(plug = reiser4_factory_nfind((char *)value))) {
		aal_error("Can't find plugin by name \"%s\".", value);
		return -EINVAL;
	}

	if (pid->type != plug->id.type) {
		aal_error("Can't override plugins of different types.");
		return -EINVAL;
	}

	pid->value = plug->id.id;
	return 0;
}

/* Overrides plugin id by @pid found by @name. */
errno_t reiser4_param_set(const char *name, rid_t id) {
	reiser4_pid_t *pid;
	reiser4_plug_t *plug;

	aal_assert("vpf-1509", name != NULL);

	if (!(pid = reiser4_param_pid(name))) {
		aal_error("Can't find param \"%s\".", name);
		return -EINVAL;
	}

	if (!(plug = reiser4_factory_ifind(pid->type, id))) {
		aal_error("Can't find plugin of the type %d by "
			  "its id %u.", pid->type, id);
		return -EINVAL;
	}

	pid->value = id;
	return 0;
}

void reiser4_param_set_flag(const char *name, uint8_t flag) {
	reiser4_pid_t *pid;
	
	aal_assert("vpf-1558", name != NULL);
	
	if (!(pid = reiser4_param_pid(name))) {
		aal_error("Can't find param \"%s\".", name);
		return;
	}
	
	pid->flags |= (1 << flag);
}

bool_t reiser4_param_get_flag(const char *name, uint8_t flag) {
	reiser4_pid_t *pid;
	
	aal_assert("vpf-1557", name != NULL);
	
	if (!(pid = reiser4_param_pid(name))) {
		aal_error("Can't find param \"%s\".", name);
		return -EINVAL;
	}
	
	return (pid->flags & (1 << flag));
}

#endif
