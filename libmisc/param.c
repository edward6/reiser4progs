/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   param.c -- methods for working with reiser4 params. */

#include <stdio.h>
#include <reiser4/reiser4.h>

errno_t misc_param_override(char *override) {
	while (1) {
		char *index;
		char *entry;
			
		char name[256];
		char value[256];

		if (!(entry = aal_strsep(&override, ",")))
			break;
		
		if (!aal_strlen(entry))
			continue;

		if (!(index = aal_strchr(entry, '='))) {
			aal_exception_error("Invalid params override "
					    "entry detected %s.", entry);
			return -EINVAL;
		}

		aal_memset(name, 0, sizeof(name));
		aal_memset(value, 0, sizeof(value));
		
		aal_strncpy(name, entry, index - entry);

		if (index + 1 == '\0') {
			aal_exception_error("Invalid params override "
					    "entry detected %s.", entry);
			return -EINVAL;
		}
		
		aal_strncpy(value, index + 1, entry + aal_strlen(entry) -
			    index);
	
		if (reiser4_param_override(name, value))
			return -EINVAL;
	}

	return 0;
}

void misc_param_print(void) {
	uint32_t i;
	reiser4_plug_t *plug;

	printf("Default params:\n");

	for (i = 0; i < PARAM_NR; i++) {
		uint32_t width;
		reiser4_pid_t *pid;

		pid = &default_param.pid[i];
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