/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   master.h -- master super block functions. */

#ifndef REISER4_MASTER_H
#define REISER4_MASTER_H

#include <reiser4/types.h>

#define SUPER(master) (&master->ent)

#ifndef ENABLE_STAND_ALONE
extern int reiser4_master_confirm(aal_device_t *device);
extern errno_t reiser4_master_sync(reiser4_master_t *master);
extern errno_t reiser4_master_valid(reiser4_master_t *master);
extern errno_t reiser4_master_reopen(reiser4_master_t *master);
extern reiser4_plug_t *reiser4_master_guess(aal_device_t *device);

extern errno_t reiser4_master_pack(reiser4_master_t *master,
				   aal_stream_t *stream);

extern reiser4_master_t *reiser4_master_unpack(aal_device_t *device,
					       aal_stream_t *stream);

extern errno_t reiser4_master_print(reiser4_master_t *master,
				    aal_stream_t *stream,
				    uuid_unparse_t unparse);

extern errno_t reiser4_master_layout(reiser4_master_t *master, 
				     region_func_t region_func,
				     void *data);

extern errno_t reiser4_master_backup(reiser4_master_t *master, 
				     aal_stream_t *stream);

extern reiser4_master_t *reiser4_master_create(aal_device_t *device,
					       uint32_t blksize);

extern void reiser4_master_set_uuid(reiser4_master_t *master,
				    char *uuid);

extern void reiser4_master_set_label(reiser4_master_t *master,
				     char *label);

extern void reiser4_master_set_format(reiser4_master_t *master,
				      rid_t format);

extern void reiser4_master_set_blksize(reiser4_master_t *master,
				       uint32_t blksize);

extern char *reiser4_master_get_uuid(reiser4_master_t *master);
extern char *reiser4_master_get_label(reiser4_master_t *master);
extern char *reiser4_master_get_magic(reiser4_master_t *master);

extern bool_t reiser4_master_isdirty(reiser4_master_t *master);
extern void reiser4_master_mkdirty(reiser4_master_t *master);
extern void reiser4_master_mkclean(reiser4_master_t *master);
#endif

extern reiser4_master_t *reiser4_master_open(aal_device_t *device);

extern void reiser4_master_close(reiser4_master_t *master);
extern rid_t reiser4_master_get_format(reiser4_master_t *master);
extern uint32_t reiser4_master_get_blksize(reiser4_master_t *master);
#endif

