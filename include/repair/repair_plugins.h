/*
    repair/repair_plugin.h - reiser4 plugins repair code known types and macros.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_PLUGIN_H
#define REPAIR_PLUGIN_H

enum repair_flag {
    REPAIR_CHECK    = 0,
    REPAIR_FIX	    = 1,
    REPAIR_ROLLBACK = 2
};

typedef enum repair_flag repair_flag_t;

struct repair_plugin_info {
    uint64_t fatal;
    uint64_t fixable;
    uint64_t fixed;
};

typedef struct repair_plugin_info repair_plugin_info_t;

#endif
