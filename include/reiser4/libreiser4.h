/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   libreiser4.h -- the central libreiser4 header file. */

#ifndef REISER4_LIBREISER4_H
#define REISER4_LIBREISER4_H

#ifdef __cplusplus
extern "C" {
#endif

#include <aal/aal.h>
	
#include <reiser4/types.h>
#include <reiser4/filesystem.h>
#include <reiser4/format.h>
#include <reiser4/journal.h>
#include <reiser4/alloc.h>
#include <reiser4/oid.h>
#include <reiser4/backup.h>
#include <reiser4/plugin.h>
#include <reiser4/tree.h>
#include <reiser4/flow.h>
#include <reiser4/node.h>
#include <reiser4/key.h>
#include <reiser4/semantic.h>
#include <reiser4/object.h>
#include <reiser4/place.h>
#include <reiser4/master.h>
#include <reiser4/status.h>
#include <reiser4/item.h>
#include <reiser4/factory.h>
#include <reiser4/param.h>
#include <reiser4/print.h>
#include <reiser4/fake.h>
#include <reiser4/debug.h>
	
extern void libreiser4_fini(void);
extern errno_t libreiser4_init(void);

extern const char *libreiser4_version(void);
extern int libreiser4_max_interface_version(void);
extern int libreiser4_min_interface_version(void);

#ifdef __cplusplus
}
#endif

#endif

