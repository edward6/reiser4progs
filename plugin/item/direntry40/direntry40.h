/*
  direntry40.h -- reiser4 default directory structures.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef DIRENTRY40_H
#define DIRENTRY40_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

/*
  The direntry40 structure is as the following:
  +-------------------------------+-------------------------------------------------+
  |           Unit Headers        |                     Units.                      |
  +-------------------------------+-------------------------------------------------+
  |                               |                      |   |                      |
  |count|entry40[0]|...|entry40[N]|objid40[0]|name[0]'\0'|...|objid40[N]|name[N]'\0'|
  |                               |                      |   |                      |
  +-------------------------------+-------------------------------------------------+

*/

/* Part of the key, the object, an entry points to. */
struct objid40 {
	d8_t locality[sizeof(d64_t)];
	d8_t objectid[sizeof(d64_t)];
};

typedef struct objid40 objid40_t;

#define oid_get_locality(oid)		    LE64_TO_CPU(*((d64_t *)(oid)->locality))
#define oid_set_locality(oid, val)	    (*(d64_t *)(oid)->locality) = CPU_TO_LE64(val)

#define oid_get_objectid(oid)		    LE64_TO_CPU(*((d64_t *)(oid)->objectid))
#define oid_set_objectid(oid, val)	    (*(d64_t *)(oid)->objectid) = CPU_TO_LE64(val)


/* Part of the key, describing the entry. */
struct entryid40 {
	d8_t objectid[sizeof(d64_t)];	    
	d8_t offset[sizeof(d64_t)];
};

typedef struct entryid40 entryid40_t;

#define eid_get_objectid(eid)		    LE64_TO_CPU(*((d64_t *)(eid)->objectid))
#define eid_set_objectid(eid, val)	    (*(d64_t *)(eid)->objectid) = CPU_TO_LE64(val)

#define eid_get_offset(eid)		    LE64_TO_CPU(*((d64_t *)(eid)->offset))
#define eid_set_offset(eid, val)	    (*(d64_t *)(eid)->offset) = CPU_TO_LE64(val)

struct entry40 {
	entryid40_t entryid;		    /* unit's part of key - hash for key40 */
	d16_t offset;			    /* offset within the direntry40 item. */
};

typedef struct entry40 entry40_t;

struct direntry40 {
	d16_t count;			    /* unit count. */
	entry40_t entry[0];		    /* unit headers. */
};

typedef struct direntry40 direntry40_t;

#define de40_get_count(de)		    aal_get_le16(de, count)
#define de40_set_count(de, val)		    aal_set_le16(de, count, val)

#define en40_get_offset(en)		    aal_get_le16(en, offset)
#define en40_set_offset(en, val)	    aal_set_le16(en, offset, val)

#define de40_inc_count(de, val) \
        de40_set_count(de, (de40_get_count(de) + val))

#define de40_dec_count(de, val) \
        de40_set_count(de, (de40_get_count(de) - val))

#define en40_inc_offset(de, val) \
        en40_set_offset(de, (en40_get_offset(de) + val))

#define en40_dec_offset(de, val) \
        en40_set_offset(de, (en40_get_offset(de) - val))

#endif
