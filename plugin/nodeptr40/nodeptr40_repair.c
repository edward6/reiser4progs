/*
    nodeptr40_repair.c -- reiser4 default node pointer item plugin.  

    Copyright (C) 2001, 2002 by Hans Reiser, licencing governed by
    reiser4progs/COPYING.
*/

#include "nodeptr40.h"

#ifndef ENABLE_COMPACT

errno_t nodeptr40_check(item_entity_t *item, uint16_t options) {
    /* Block numbers are checked in setup_func from reiser4_node_traverse */
    return 0;
}

#endif
