/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   version.h -- version information for reiser4progs. */

static inline unsigned int get_release_number_major(void)
{
        return FORMAT_LAST_ID - 1;
}

static inline unsigned int get_release_number_minor(void)
{
        return PLUGIN_LIBRARY_VERSION;
}

#define BANNER						     \
    "Copyright (C) 2001-2005 by Hans Reiser, "  \
    "licensing governed by reiser4progs/COPYING."
