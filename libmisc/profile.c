/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- default profile and methods for working with it. */

#include <stdio.h>
#include <reiser4/reiser4.h>

static reiser4_profile_t profile = {
	.name = "default",
	.plugin = {
		[0] = {
			.name  = "node",
			.type  = NODE_PLUGIN_TYPE,
			.value = NODE_LARGE_ID
		},
		[1] = {
			.name  = "nodeptr",
			.type  = ITEM_PLUGIN_TYPE,
			.value = ITEM_NODEPTR40_ID
		},
		[2] = {
			.name  = "statdata",
			.type  = ITEM_PLUGIN_TYPE,
			.value = ITEM_STATDATA40_ID
		},
		[3] = {
			.name  = "tail",
			.type  = ITEM_PLUGIN_TYPE,
			.value = ITEM_TAIL40_ID
		},
		[4] = {
			.name  = "extent",
			.type  = ITEM_PLUGIN_TYPE,
			.value = ITEM_EXTENT40_ID
		},
		[5] = {
			.name  = "cde",
			.type  = ITEM_PLUGIN_TYPE,
			.value = ITEM_CDE_LARGE_ID
		},
		[6] = {
			.name  = "acl",
			.type  = ITEM_PLUGIN_TYPE,
			.value = ITEM_ACL40_ID
		},
		[7] = {
			.name  = "hash",
			.type  = HASH_PLUGIN_TYPE,
			.value = HASH_R5_ID
		},
		[8] = {
			.name  = "policy",
			.type  = POLICY_PLUGIN_TYPE,
			.value = TAIL_SMART_ID
		},
		[9] = {
			.name  = "perm",
			.type  = PERM_PLUGIN_TYPE,
			.value = PERM_RWX_ID
		},
		[10] = {
			.name  = "regular",
			.type  = OBJECT_PLUGIN_TYPE,
			.value = OBJECT_FILE40_ID
		},
		[11] = {
			.name  = "directory",
			.type  = OBJECT_PLUGIN_TYPE,
			.value = OBJECT_DIRTORY40_ID
		},
		[12] = {
			.name  = "symlink",
			.type  = OBJECT_PLUGIN_TYPE,
			.value = OBJECT_SYMLINK40_ID
		},
		[13] = {
			.name  = "special",
			.type  = OBJECT_PLUGIN_TYPE,
			.value = OBJECT_SPECIAL40_ID
		},
		[14] = {
			.name  = "format",
			.type  = FORMAT_PLUGIN_TYPE,
			.value = FORMAT_REISER40_ID
		},
		[15] = {
			.name  = "oid",
			.type  = OID_PLUGIN_TYPE,
			.value = OID_REISER40_ID
		},
		[16] = {
			.name  = "alloc",
			.type  = ALLOC_PLUGIN_TYPE,
			.value = ALLOC_REISER40_ID
		},
		[17] = {
			.name  = "journal",
			.type  = JOURNAL_PLUGIN_TYPE,
			.value = JOURNAL_REISER40_ID
		},
		[18] = {
			.name  = "key",
			.type  = KEY_PLUGIN_TYPE,
			.value = KEY_LARGE_ID
		}
	}
};

#define PROFILE_SIZE 19

reiser4_profile_t *misc_profile_default(void) {
	return &profile;
}

errno_t misc_profile_override(char *override)
{
	while (1) {
		char *index;
		char *entry;
			
		char name[255];
		char value[255];
			
		if (!(entry = aal_strsep(&override, ",")))
			break;
		
		if (!aal_strlen(entry))
			continue;

		if (!(index = aal_strchr(entry, '='))) {
			aal_exception_error("Invalid profile override "
					    "entry detected %s.", entry);
			return -EINVAL;
		}

		aal_memset(name, 0, sizeof(name));
		aal_memset(value, 0, sizeof(value));
		
		aal_strncpy(name, entry, (index - entry));

		if (index + 1 == '\0') {
			aal_exception_error("Invalid profile override "
					    "entry detected %s.", entry);
			return -EINVAL;
		}
		
		aal_strncpy(value, index + 1, (entry + aal_strlen(entry)) -
			    index);
	
		if (reiser4_profile_override(&profile, name, value))
			return -EINVAL;
	}

	return 0;
}

void misc_profile_print(void) {
	uint32_t i;
	reiser4_plugin_t *plugin;

	printf("Profile %s:\n", profile.name);
	
	for (i = 0; i < PROFILE_SIZE; i++) {
		uint32_t width;
		reiser4_pid_t *pid = &profile.plugin[i];

		plugin = libreiser4_factory_ifind(pid->type,
						  pid->value);

		width = 12 - aal_strlen(pid->name);

		if (plugin) {
			printf("%s:%*s%s (%s).\n", pid->name, width - 1, " ",
			       plugin->label, plugin->desc);
		} else {
			printf("%s:%*s0x%llx.\n", pid->name, width - 1, " ",
			       pid->value);
		}
	}
	printf("\n");
}
