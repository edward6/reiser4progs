/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.h -- reiser4 profile functions. */

#ifndef REISER4_PROFILE_H
#define REISER4_PROFILE_H

#include <reiser4/types.h>

enum reiser4_profile_index {
	PROF_OBJ	= 0x0,
	PROF_DIR	= 0x1,
	PROF_REGFILE	= 0x2,
	PROF_DIRFILE	= 0x3,
	PROF_SYMFILE	= 0x4,
	PROF_SPLFILE	= 0x5,
	PROF_CREATE	= 0x6,
	
	PROF_FORMAT	= 0x7,
	PROF_JOURNAL	= 0x8,
	PROF_OID	= 0x9,
	PROF_ALLOC	= 0xa,
	PROF_KEY	= 0xb,
	PROF_NODE	= 0xc,
	
	PROF_STAT	= 0xd,
	PROF_DIRITEM	= 0xe,
	PROF_NODEPTR	= 0xf,
	PROF_TAIL	= 0x10,
	PROF_EXTENT	= 0x11,
	PROF_CTAIL	= 0x12,
	
	
	PROF_COMPRESS	= 0x13,
	PROF_CMODE	= 0x14,
	PROF_CRYPTO	= 0x15,
	PROF_DIGEST	= 0x16,
	PROF_CLUSTER	= 0x17,
	
	PROF_HASH	= 0x18,
	PROF_FIBRE	= 0x19,
	PROF_POLICY	= 0x1a,
	PROF_PERM	= 0x1b,
	PROF_LAST
};

typedef struct reiser4_profile {
	struct {
#ifndef ENABLE_MINIMAL
		/* The name of the profile slot. */
		char *name;

		/* Hide those slots where are no alternatives. */
		uint8_t hidden;
		
		/* The maximum allowed value. */
		rid_t max;
#endif
		/* The default plugin id and the plugin type of profile slot. */
		plug_ident_t id;		
	} pid[PROF_LAST];

	/* Overriden mask. */
	uint64_t mask;
} reiser4_profile_t;

extern reiser4_plug_t *reiser4_profile_plug(rid_t index);

#ifndef ENABLE_MINIMAL
extern errno_t reiser4_profile_override(const char *plug, const char *name);

extern bool_t reiser4_profile_overridden(rid_t id);

extern void reiser4_profile_print(aal_stream_t *stream);
#endif
#endif
