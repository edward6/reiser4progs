/*
    stat40_repair.c -- reiser4 default stat data plugin.
    
    Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#include "stat40.h"

#ifndef ENABLE_COMPACT

errno_t stat40_check(item_entity_t *item) {
    return 0;
}

#endif

