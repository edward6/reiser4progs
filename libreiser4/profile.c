/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#include <reiser4/reiser4.h>

uint64_t reiser4_profile_value(reiser4_profile_t *profile, 
			       const char *name) 
{
	reiser4_pid_t *pid;

	aal_assert("umka-1878", profile != NULL);
	aal_assert("umka-1879", name != NULL);
	
	if (!(pid = reiser4_profile_pid(profile, name)))
		return INVAL_PID;

	return pid->value;
}

reiser4_pid_t *reiser4_profile_pid(reiser4_profile_t *profile,
				   const char *name)
{
	unsigned i;

	aal_assert("umka-1876", profile != NULL);
	aal_assert("umka-1877", name != NULL);
	
	for (i = 0; i < (sizeof(profile->plugin) / sizeof(reiser4_pid_t)); i++) {
		reiser4_pid_t *pid = &profile->plugin[i];

		if (!aal_strncmp(pid->name, name, aal_strlen(name)))
			return pid;
	}

	return NULL;
}

#ifndef ENABLE_STAND_ALONE
errno_t reiser4_profile_override(reiser4_profile_t *profile, 
				 const char *type, const char *name) 
{
	reiser4_pid_t *pid;
	reiser4_plugin_t *plugin;

	aal_assert("umka-923", type != NULL);
	aal_assert("umka-924", name != NULL);
	aal_assert("umka-922", profile != NULL);

	if (!(pid = reiser4_profile_pid(profile, type)))
		return -EINVAL;

	if (!(plugin = libreiser4_factory_nfind((char *)name))) {
		aal_exception_error("Can't find plugin by name \"%s\".",
				    name);
		return -EINVAL;
	}

	if (pid->type != plugin->id.type) {
		aal_exception_error("Can't override plugins of "
				    "different types.");
		return -EINVAL;
	}

	pid->value = plugin->id.id;
	return 0;
}
#endif
