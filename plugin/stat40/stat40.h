/*
    stat40.h -- reiser4 default stat data structures.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef STAT40_H
#define STAT40_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct stat40_sdext {
    reiser4_body_t *body;
    reiser4_plugin_t *plugin;
};

typedef struct stat40_sdext stat40_sdext_t;

struct stat40 {
    d16_t extmask;
};

typedef struct stat40 stat40_t;  

#define st40_get_extmask(stat)		aal_get_le16(stat, extmask)
#define st40_set_extmask(stat, val)	aal_set_le16(stat, extmask, val)

#endif

