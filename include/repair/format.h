/*  repair/format.h -- reiserfs filesystem recovery structures and macros.
    
    Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING. */

#ifndef REPAIR_FORMAT_H
#define REPAIR_FORMAT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <reiser4/filesystem.h>

extern errno_t repair_format_open(reiser4_fs_t *fs, uint8_t mode);
extern errno_t repair_format_update(reiser4_format_t *format);
extern void repair_format_print(reiser4_fs_t *fs, FILE *file, 
				uint16_t options);
#endif
