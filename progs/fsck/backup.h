/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   backup.h -- backup and rollback fsck declarations. */

#ifndef BACKUP_H
#define BACKUP_H

#include <stdio.h>

#include <aal/libaal.h>
#include <reiser4/bitmap.h>

#define BACKUP_MAGIC "_RollBackFileForReiser4FSCK"

typedef struct backup {
	errno_t (*write) (aal_device_t *, void *, blk_t, count_t);
	reiser4_bitmap_t *bitmap;
	FILE *file;
} backup_t;

extern errno_t backup_init(FILE *file, aal_device_t *device, count_t len);
extern errno_t backup_rollback(FILE *file, aal_device_t *device);
extern void backup_fini();

#endif
