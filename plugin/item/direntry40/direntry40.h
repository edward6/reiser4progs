/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   direntry40.h -- reiser4 default directory structures. */

#ifndef DIRENTRY40_H
#define DIRENTRY40_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

#define direntry40_body(item) ((direntry40_t *)item->body)

/* The direntry40 structure is as the following:
   +-------------------------------+-------------------------------------------------+
   |           Unit Headers        |                     Units.                      |
   +-------------------------------+-------------------------------------------------+
   |                               |                      |   |                      |
   |count|entry40[0]|...|entry40[N]|objid40[0]|name[0]'\0'|...|objid40[N]|name[N]'\0'|
   |                               |                      |   |                      |
   +-------------------------------+-------------------------------------------------+ */

/* Part of the key, the object, an entry points to */
struct objid {
	d8_t locality[8];
	d8_t objectid[8];
};

typedef struct objid objid_t;

#define ob40_get_locality(oid)		    LE64_TO_CPU(*((d64_t *)(oid)->locality))
#define ob40_set_locality(oid, val)	    (*(d64_t *)(oid)->locality) = CPU_TO_LE64(val)

#define ob40_get_objectid(oid)		    LE64_TO_CPU(*((d64_t *)(oid)->objectid))
#define ob40_set_objectid(oid, val)	    (*(d64_t *)(oid)->objectid) = CPU_TO_LE64(val)


/* Part of the key, describing the entry. */
struct hash {
	d8_t objectid[8];
	d8_t offset[8];
};

typedef struct hash hash_t;

#define ha40_get_objectid(eid)		    LE64_TO_CPU(*((d64_t *)(eid)->objectid))
#define ha40_set_objectid(eid, val)	    (*(d64_t *)(eid)->objectid) = CPU_TO_LE64(val)

#define ha40_get_offset(eid)		    LE64_TO_CPU(*((d64_t *)(eid)->offset))
#define ha40_set_offset(eid, val)	    (*(d64_t *)(eid)->offset) = CPU_TO_LE64(val)

struct entry40 {
	hash_t hash;
	d16_t offset;
};

typedef struct entry40 entry40_t;

struct direntry40 {
	d16_t units;
	entry40_t entry[0];
};

typedef struct direntry40 direntry40_t;

#define de40_get_units(de)		    aal_get_le16((de), units)
#define de40_set_units(de, val)		    aal_set_le16((de), units, val)

#define de40_inc_units(de, val) \
        de40_set_units(de, (de40_get_units(de) + val))

#define de40_dec_units(de, val) \
        de40_set_units(de, (de40_get_units(de) - val))

#define en40_get_offset(en)		    aal_get_le16((en), offset)
#define en40_set_offset(en, val)	    aal_set_le16((en), offset, val)

#define en40_inc_offset(de, val) \
        en40_set_offset(de, (en40_get_offset(de) + val))

#define en40_dec_offset(de, val) \
        en40_set_offset(de, (en40_get_offset(de) - val))

#endif
