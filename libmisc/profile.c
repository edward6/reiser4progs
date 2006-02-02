/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   pprofile.c -- methods for working with reiser4 profile. */

#include <stdio.h>
#include <reiser4/libreiser4.h>

errno_t misc_profile_override(char *override) {
	while (1) {
		char *entry, *c;
		char name[256];
		char value[256];

		if (!(entry = aal_strsep(&override, ",")))
			break;
		
		if (!aal_strlen(entry))
			continue;

		if (!(c = aal_strchr(entry, '='))) {
			aal_error("Invalid profile override "
				  "entry detected %s.", entry);
			return -EINVAL;
		}

		aal_memset(name, 0, sizeof(name));
		aal_memset(value, 0, sizeof(value));
		
		aal_strncpy(name, entry, c - entry);

		if (c + 1 == '\0') {
			aal_error("Invalid profile override "
				  "entry detected %s.", entry);
			return -EINVAL;
		}
		
		aal_strncpy(value, c + 1, entry + aal_strlen(entry) - c);
		
		if (reiser4_profile_override(name, value))
			return -EINVAL;
	}

	return 0;
}

/* Prints default plugin profiles. */
void misc_profile_print(void) {
	aal_stream_t stream;

	aal_stream_init(&stream, stdout, &file_stream);
	
	aal_stream_format(&stream, "Default profile:\n");

	reiser4_profile_print(&stream);
	
	aal_stream_format(&stream, "\n");

	aal_stream_fini(&stream);
}
