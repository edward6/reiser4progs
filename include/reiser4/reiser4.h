/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reiser4.h -- the central libreiser4 header. */

#ifndef REISER4_H
#define REISER4_H

#ifdef __cplusplus
extern "C" {
#endif

#include <aal/aal.h>

#include "types.h"
#include "filesystem.h"
#include "format.h"
#include "journal.h"
#include "alloc.h"
#include "oid.h"
#include "plugin.h"
#include "tree.h"
#include "node.h"
#include "key.h"
#include "object.h"
#include "place.h"
#include "master.h"
#include "item.h"
#include "factory.h"
#include "profile.h"
#include "print.h"

extern void libreiser4_fini(void);
extern errno_t libreiser4_init(void);

extern const char *libreiser4_version(void);
extern int libreiser4_max_interface_version(void);
extern int libreiser4_min_interface_version(void);

#ifdef __cplusplus
}
#endif

#endif

