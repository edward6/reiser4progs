/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- methods for working with profiles in reiser4 programs. */

#include <stdio.h>
#include <reiser4/reiser4.h>

static reiser4_profile_t profiles[] = {
	[0] = {
		.name = "smart40",
		.desc = "Profile for reiser4 with smart tail policy",
		.plugin = {
			[0] = {
				.name  = "node",
				.type  = NODE_PLUGIN_TYPE,
				.value = NODE_REISER40_ID
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
				.name  = "direntry",
				.type  = ITEM_PLUGIN_TYPE,
				.value = ITEM_CDE40_ID
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
				.type  = TAIL_PLUGIN_TYPE,
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
				.value = KEY_REISER40_ID
			},
			[19] = {
				.name  = "sdext",
				.type  = SDEXT_PLUGIN_TYPE,
				.value = 1 << SDEXT_UNIX_ID
			}
		}
	},
	[1] = {
		.name = "extent40",
		.desc = "Profile for reiser4 with tails turned off",
		.plugin = {
			[0] = {
				.name  = "node",
				.type  = NODE_PLUGIN_TYPE,
				.value = NODE_REISER40_ID
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
				.name  = "direntry",
				.type  = ITEM_PLUGIN_TYPE,
				.value = ITEM_CDE40_ID
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
				.type  = TAIL_PLUGIN_TYPE,
				.value = TAIL_NEVER_ID
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
				.value = KEY_REISER40_ID
			},
			[19] = {
				.name  = "sdext",
				.type  = SDEXT_PLUGIN_TYPE,
				.value = 1 << SDEXT_UNIX_ID
			}
		}
	},
	[2] = {
		.name = "tail40",
		.desc = "Profile for reiser4 with extents turned off",     
		.plugin = {
			[0] = {
				.name  = "node",
				.type  = NODE_PLUGIN_TYPE,
				.value = NODE_REISER40_ID
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
				.name  = "direntry",
				.type  = ITEM_PLUGIN_TYPE,
				.value = ITEM_CDE40_ID
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
				.type  = TAIL_PLUGIN_TYPE,
				.value = TAIL_ALWAYS_ID
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
				.value = KEY_REISER40_ID
			},
			[19] = {
				.name  = "sdext",
				.type  = SDEXT_PLUGIN_TYPE,
				.value = 1 << SDEXT_UNIX_ID
			}
		}
	}
};

/* 0 profile is the default one. */
reiser4_profile_t *misc_profile_default(void) {
	return &profiles[0];
}

/* Finds profile by its name */
reiser4_profile_t *misc_profile_find(const char *name) {
	unsigned i;
    
	aal_assert("vpf-104", name != NULL);
    
	for (i = 0; i < (sizeof(profiles) / sizeof(reiser4_profile_t)); i++) {
		if (!aal_strncmp(profiles[i].name, name,
				 aal_strlen(profiles[i].name)))
		{
			return &profiles[i];
		}
	}

	return NULL;
}

errno_t misc_profile_override(reiser4_profile_t *profile,
			       char *override)
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
	
		if (reiser4_profile_override(profile, name, value))
			return -EINVAL;
	}

	return 0;
}

/* Shows all knows profiles */
void misc_profile_list(void) {
	unsigned i;
    
	for (i = 0; i < (sizeof(profiles) / sizeof(reiser4_profile_t)); i++)
		printf("%s:  \t%s.\n", profiles[i].name, profiles[i].desc);
    
	printf("\n");
}

void misc_profile_print(reiser4_profile_t *profile) {
	unsigned i;
	reiser4_plugin_t *plugin;

	aal_assert("umka-925", profile != NULL);
	
	printf("Profile %s:\n", profile->name);
	
	for (i = 0; i < (sizeof(profile->plugin) / sizeof(reiser4_pid_t)); i++) {
		reiser4_pid_t *pid = &profiles->plugin[i];

		if (!(plugin = libreiser4_factory_ifind(pid->type, pid->value)))
			continue;

		printf("%s:  \t%s(%s).\n", pid->name, plugin->h.label,
		       plugin->h.desc);
	}
	printf("\n");
}
