/*
  reiser4.h -- the central libreiser4 header.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

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
#include "file.h"
#include "place.h"
#include "master.h"
#include "item.h"
#include "factory.h"

extern void libreiser4_done(void);
extern errno_t libreiser4_init(void);

extern const char *libreiser4_version(void);
extern int libreiser4_max_interface_version(void);
extern int libreiser4_min_interface_version(void);

extern reiser4_abort_t libreiser4_get_abort(void);
extern void libreiser4_set_abort(reiser4_abort_t func);

#ifdef __cplusplus
}
#endif

#endif

