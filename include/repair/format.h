/*
    repair/format.h -- reiserfs filesystem recovery structures and macros.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#ifndef REPAIR_FORMAT_H
#define REPAIR_FORMAT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/filesystem.h>

extern reiser4_format_t *repair_format_open(reiser4_master_t *master, 
    reiser4_profile_t *profile);
extern void repair_format_print(reiser4_fs_t *fs, FILE *file, 
    uint16_t options);
extern errno_t callback_mark_format_block(object_entity_t *format, blk_t blk, 
    void *data);

#endif
