/*
    stat40_repair.c -- reiser4 default stat data plugin.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_ALONE

#include "stat40.h"

errno_t stat40_check(item_entity_t *item) {
    return 0;
}

#endif

