/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   master.h -- master super block functions. */

#ifndef REISER4_MASTER_H
#define REISER4_MASTER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

#define SUPER(master) (&master->super)

#ifndef ENABLE_STAND_ALONE

extern errno_t reiser4_master_print(reiser4_master_t *master,
				    aal_stream_t *stream);

extern errno_t reiser4_master_clobber(aal_device_t *device);

extern reiser4_master_t *reiser4_master_create(aal_device_t *device, 
					       rid_t format_pid,
					       uint32_t blocksize,
					       const char *uuid, 
					       const char *label);

extern int reiser4_master_confirm(aal_device_t *device);
extern errno_t reiser4_master_sync(reiser4_master_t *master);
extern errno_t reiser4_master_valid(reiser4_master_t *master);
extern errno_t reiser4_master_reopen(reiser4_master_t *master);
extern reiser4_plugin_t *reiser4_master_guess(aal_device_t *device);

extern char *reiser4_master_uuid(reiser4_master_t *master);
extern char *reiser4_master_label(reiser4_master_t *master);
extern char *reiser4_master_magic(reiser4_master_t *master);

extern bool_t reiser4_master_isdirty(reiser4_master_t *master);
extern void reiser4_master_mkdirty(reiser4_master_t *master);
extern void reiser4_master_mkclean(reiser4_master_t *master);
#endif

extern reiser4_master_t *reiser4_master_open(aal_device_t *device);

extern void reiser4_master_close(reiser4_master_t *master);
extern rid_t reiser4_master_format(reiser4_master_t *master);
extern uint32_t reiser4_master_blksize(reiser4_master_t *master);

#endif

