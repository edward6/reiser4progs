/*
    repair/repair_plugin.h - reiser4 plugins repair code known types and macros.
    
    Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
    reiser4progs/COPYING.
*/

#ifndef REPAIR_PLUGIN_H
#define REPAIR_PLUGIN_H

enum repair_flag {
    REPAIR_CHECK    = 1,
    REPAIR_FIX	    = 2,
    REPAIR_REBUILD  = 3,
    REPAIR_ROLLBACK = 4
};

typedef enum repair_flag repair_flag_t;

enum repair_error_codes {
    /* No error were detected. */
    REPAIR_OK	    = 0,
    /* All errors were fixed. */
    REPAIR_FIXED    = 1,
    /* Fixable errors were detected. */
    REPAIR_FIXABLE  = 2,
    /* Fatal errors were detected. */
    REPAIR_FATAL    = 3,
};

typedef enum repair_error_codes repair_error_codes_t;

#define repair_error_exists(result)	(result > REPAIR_FIXED || result < 0)
#define repair_error(result, kind)	(result = result < kind ? kind : result)

struct repair_plugin_info {
    uint32_t fatal;
    uint32_t fixable;
    uint32_t fixed;
    uint8_t  mode;
};

//typedef struct repair_plugin_info repair_plugin_info_t;

#endif
