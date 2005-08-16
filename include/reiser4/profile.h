/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.h -- reiser4 profile functions. */

#ifndef REISER4_PROFILE_H
#define REISER4_PROFILE_H

#ifndef ENABLE_MINIMAL
#include <reiser4/types.h>

enum reiser4_profile_index {
	PROF_FORMAT	= 0x0,
	PROF_JOURNAL	= 0x1,
	PROF_OID	= 0x2,
	PROF_ALLOC	= 0x3,
	PROF_KEY	= 0x4,
	PROF_NODE	= 0x5,
	
	PROF_STAT	= 0x6,
	PROF_NODEPTR	= 0x7,
	PROF_DIRITEM	= 0x8,
	PROF_TAIL	= 0x9,
	PROF_EXTENT	= 0xa,
	PROF_CTAIL	= 0xb,
	
	PROF_CREATE	= 0xc,
	PROF_MKDIR	= 0xd,
	PROF_SYMLINK	= 0xe,
	PROF_MKNODE	= 0xf,
	
	PROF_COMPRESS   = 0x10,
	PROF_CRYPTO     = 0x11,
	
	PROF_HASH	= 0x12,
	PROF_FIBRE	= 0x13,
	PROF_POLICY	= 0x14,
	PROF_CLUSTER    = 0x15,
	PROF_LAST
};

extern errno_t reiser4_profile_override(const char *plug, const char *name);
extern bool_t reiser4_profile_overridden(rid_t id);

extern inline reiser4_plug_t *reiser4_profile_plug(rid_t index);
extern void reiser4_profile_print(aal_stream_t *stream);

typedef struct reiser4_profile {
	struct {
		char *name;
		plug_ident_t id;
		/* Do not show those parameters where is no alternatives. */
		uint8_t hidden;
	} pid[PROF_LAST];

	/* Overriden mask. */
	uint64_t mask;
} reiser4_profile_t;

#endif
#endif
