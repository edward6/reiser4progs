/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- default profile and methods for working with it. */

#include <stdio.h>
#include <reiser4/reiser4.h>

errno_t misc_profile_override(char *override) {
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
		
		aal_strncpy(name, entry, index - entry);

		if (index + 1 == '\0') {
			aal_exception_error("Invalid profile override "
					    "entry detected %s.", entry);
			return -EINVAL;
		}
		
		aal_strncpy(value, index + 1, entry + aal_strlen(entry) -
			    index);
	
		if (reiser4_profile_override(name, value))
			return -EINVAL;
	}

	return 0;
}

void misc_profile_print(void) {
	uint32_t i;
	reiser4_plug_t *plug;

	printf("Default profile\n");

	for (i = 0; i < PROFILE_PLUGS; i++) {
		uint32_t width;
		reiser4_pid_t *pid;

		pid = &default_profile.pid[i];
		width = 12 - aal_strlen(pid->name);
		
		if ((plug = reiser4_factory_ifind(pid->type,
						  pid->value)))
		{
			printf("%s:%*s\"%s\"\n", pid->name, width - 1,
			       " ", plug->label);
		} else {
			printf("%s:%*s\"absent (id: 0x%llx)\"\n", pid->name,
			       width - 1, " ", pid->value);
		}
	}
	
	printf("\n");
}
