/*
    stat40_repair.c -- reiser4 default stat data plugin.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include "stat40.h"

#ifndef ENABLE_COMPACT

errno_t stat40_check(item_entity_t *item, uint16_t options) {
    return 0;
}

#endif

