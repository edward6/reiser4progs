/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   key.h -- reiser4 key defines and functions. */

#ifndef REISER4_KEY_H
#define REISER4_KEY_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <reiser4/types.h>

#ifndef ENABLE_STAND_ALONE
extern errno_t reiser4_key_string(reiser4_key_t *key,
				  char *buff);

extern errno_t reiser4_key_print(reiser4_key_t *key,
				 aal_stream_t *stream);

extern uint64_t reiser4_key_get_hash(reiser4_key_t *key);

extern errno_t reiser4_key_set_hash(reiser4_key_t *key,
				    uint64_t hash);
#endif

extern int reiser4_key_compare(reiser4_key_t *key1,
			       reiser4_key_t *key2);

extern errno_t reiser4_key_assign(reiser4_key_t *dst,
				  reiser4_key_t *src);

extern errno_t reiser4_key_build_gener(reiser4_key_t *key,
				       uint32_t type,
				       oid_t locality,
				       uint64_t ordering,
				       oid_t objectid,
				       uint64_t offset);

extern errno_t reiser4_key_build_entry(reiser4_key_t *key,
				       reiser4_plugin_t *plugin,
				       oid_t locality,
				       oid_t objectid,
				       char *name);

extern errno_t reiser4_key_set_type(reiser4_key_t *key,
				    uint32_t type);

extern errno_t reiser4_key_set_offset(reiser4_key_t *key,
				      uint64_t offset);

extern errno_t reiser4_key_set_objectid(reiser4_key_t *key,
					oid_t objectid);

extern errno_t reiser4_key_set_locality(reiser4_key_t *key,
					oid_t locality);

extern errno_t reiser4_key_set_ordering(reiser4_key_t *key,
					uint64_t ordering);

#ifndef ENABLE_STAND_ALONE
extern errno_t reiser4_key_valid(reiser4_key_t *key);
extern uint32_t reiser4_key_get_type(reiser4_key_t *key);
extern uint64_t reiser4_key_get_offset(reiser4_key_t *key);
extern oid_t reiser4_key_get_objectid(reiser4_key_t *key);
extern oid_t reiser4_key_get_locality(reiser4_key_t *key);
extern uint64_t reiser4_key_get_ordering(reiser4_key_t *key);
#endif

extern void reiser4_key_maximal(reiser4_key_t *key);
extern void reiser4_key_minimal(reiser4_key_t *key);
extern void reiser4_key_clean(reiser4_key_t *key);

#endif
