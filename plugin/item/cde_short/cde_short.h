/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde_short.h -- reiser4 directory entry with short keys. */

#ifndef CDE_SHORT_H
#define CDE_SHORT_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

#define cde_short_body(item) ((cde_short_t *)item->body)

/* The cde_short structure is as the following:
   +-------------------------------+-------------------------------------------------+
   |           Unit Headers        |                     Units.                      |
   +-------------------------------+-------------------------------------------------+
   |                               |                      |   |                      |
   |count|entry40[0]|...|entry40[N]|objid40[0]|name[0]'\0'|...|objid40[N]|name[N]'\0'|
   |                               |                      |   |                      |
   +-------------------------------+-------------------------------------------------+ */

struct objid {
	d8_t locality[8];
	d8_t objectid[8];
};

typedef struct objid objid_t;

#define ob_get_locality(ob)		    LE64_TO_CPU(*((d64_t *)(ob)->locality))
#define ob_set_locality(ob, val)	    (*(d64_t *)(ob)->locality) = CPU_TO_LE64(val)

#define ob_get_objectid(ob)		    LE64_TO_CPU(*((d64_t *)(ob)->objectid))
#define ob_set_objectid(ob, val)	    (*(d64_t *)(ob)->objectid) = CPU_TO_LE64(val)

struct hash {
	d8_t objectid[8];
	d8_t offset[8];
};

typedef struct hash hash_t;

#define ha_get_objectid(ha)		    LE64_TO_CPU(*((d64_t *)(ha)->objectid))
#define ha_set_objectid(ha, val)	    (*(d64_t *)(ha)->objectid) = CPU_TO_LE64(val)

#define ha_get_offset(ha)		    LE64_TO_CPU(*((d64_t *)(ha)->offset))
#define ha_set_offset(ha, val)              (*(d64_t *)(ha)->offset) = CPU_TO_LE64(val)

struct entry {
	hash_t hash;
	d16_t offset;
};

typedef struct entry entry_t;

struct cde_short {
	d16_t units;
	entry_t entry[0];
};

typedef struct cde_short cde_short_t;

#define de_get_units(de)		    \
        aal_get_le16((de), units)

#define de_set_units(de, val)		    \
        aal_set_le16((de), units, val)

#define de_inc_units(de, val)               \
        de_set_units(de, (de_get_units(de) + val))

#define de_dec_units(de, val)               \
        de_set_units(de, (de_get_units(de) - val))

#define en_get_offset(en)		    \
        aal_get_le16((en), offset)

#define en_set_offset(en, val)	            \
        aal_set_le16((en), offset, val)

#define en_inc_offset(en, val)              \
        en_set_offset(en, (en_get_offset(en) + val))

#define en_dec_offset(en, val)              \
        en_set_offset(en, (en_get_offset(en) - val))

#endif
