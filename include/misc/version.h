/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   version.h -- version information for reiser4progs. */

/*
 * principal release number gets incremented if there are
 * changes in master super-block
 */
#define RELEASE_NUMBER_PRINCIPAL (5)

static inline unsigned int get_release_number_principal(void)
{
        return RELEASE_NUMBER_PRINCIPAL;
}

static inline unsigned int get_release_number_major(void)
{
        return FORMAT_LAST_ID - 1;
}

static inline unsigned int get_release_number_minor(void)
{
        return PLUGIN_LIBRARY_VERSION;
}

#define COPYRIGHT_REISER			\
    "Copyright (C) 2001-2005 by Hans Reiser, "  \
    "licensing governed by reiser4progs/COPYING."
