/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde40.h -- reiser4 directory entry plugin. */

#ifndef CDE40_H
#define CDE40_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

/* The cde40 structure is as the following:
   +-------------------------------+-------------------------------------------------+
   |           Unit Headers        |                     Units.                      |
   +-------------------------------+-------------------------------------------------+
   |                               |                      |   |                      |
   |count|entry40[0]|...|entry40[N]|objid40[0]|name[0]'\0'|...|objid40[N]|name[N]'\0'|
   |                               |                      |   |                      |
   +-------------------------------+-------------------------------------------------+ */

struct cde40 {
	d16_t units;
};

typedef struct cde40 cde40_t;

#ifdef ENABLE_SHORT_KEYS
struct objid3 {
	d8_t locality[8];
	d8_t objectid[8];
};

typedef struct objid3 objid3_t;

struct hash3 {
	d8_t objectid[8];
	d8_t offset[8];
};

typedef struct hash3 hash3_t;

struct entry3 {
	hash3_t hash;
	d16_t offset;
};

typedef struct entry3 entry3_t;

struct cde403 {
	d16_t units;
	entry3_t entry[0];
};

typedef struct cde403  cde403_t;
#endif

#ifdef ENABLE_LARGE_KEYS
struct objid4 {
	d8_t locality[8];
	d8_t ordering[8];
	d8_t objectid[8];
};

typedef struct objid4 objid4_t;

struct hash4 {
	d8_t ordering[8];
	d8_t objectid[8];
	d8_t offset[8];
};

typedef struct hash4 hash4_t;

struct entry4 {
	hash4_t hash;
	d16_t offset;
};

typedef struct entry4 entry4_t;

struct cde404 {
	d16_t units;
	entry4_t entry[0];
};

typedef struct cde404  cde404_t;
#endif

extern reiser4_core_t *cde40_core;

extern uint32_t cde40_number_units(place_t *place);
extern inline uint32_t cde40_key_pol(place_t *place);

extern errno_t cde40_maxposs_key(place_t *place,
				 key_entity_t *key);

extern errno_t cde40_delete(place_t *place, uint32_t pos,
   	                    trans_hint_t *hint);

extern errno_t cde40_get_hash(place_t *place, uint32_t pos, 
			      key_entity_t *key);

extern errno_t cde40_copy(place_t *dst_place, uint32_t dst_pos,
			  place_t *src_place, uint32_t src_pos,
			  uint32_t count);

extern uint32_t cde40_expand(place_t *place, uint32_t pos,
			     uint32_t count, uint32_t len);

extern uint32_t cde40_regsize(place_t *place, uint32_t pos, 
			      uint32_t count);

extern lookup_t cde40_lookup(place_t *place, key_entity_t *key,
			     bias_t bias);

#if defined(ENABLE_SHORT_KEYS) && defined(ENABLE_LARGE_KEYS)
#define ob_get_locality(ob, pol)                                       \
        ((pol == 3) ?                                                  \
	 LE64_TO_CPU(*((d64_t *)((objid3_t *)(ob))->locality)) :       \
	 LE64_TO_CPU(*((d64_t *)((objid4_t *)(ob))->locality)))

#define ob_set_locality(ob, val, pol)	                               \
        ((pol == 3) ?                                                  \
	 (*(d64_t *)((objid3_t *)(ob))->locality) = CPU_TO_LE64(val) : \
	 (*(d64_t *)((objid4_t *)(ob))->locality) = CPU_TO_LE64(val))

#define ob_get_objectid(ob, pol)                                       \
        ((pol == 3) ?                                                  \
	 LE64_TO_CPU(*((d64_t *)((objid3_t *)(ob))->objectid)) :       \
	 LE64_TO_CPU(*((d64_t *)((objid4_t *)(ob))->objectid)))

#define ob_set_objectid(ob, val, pol)	                               \
        ({if (pol == 3)                                                \
	 (*(d64_t *)((objid3_t *)(ob))->objectid) = CPU_TO_LE64(val);  \
         else                                                          \
	 (*(d64_t *)((objid4_t *)(ob))->objectid) = CPU_TO_LE64(val);})

#define ob_get_ordering(ob, pol)                                       \
        ((pol == 3) ? 0 :                                              \
	 LE64_TO_CPU(*((d64_t *)((objid4_t *)(ob))->ordering)))

#define ob_set_ordering(ob, val, pol)	                               \
        ({if (pol == 3) do {} while(0); else                           \
	 (*(d64_t *)((objid4_t *)(ob))->ordering) = CPU_TO_LE64(val);})

#define ob_size(pol)                                                   \
        ((pol == 3) ? sizeof(objid3_t) : sizeof(objid4_t))

#define ha_get_objectid(ha, pol)		                       \
        ((pol == 3) ?                                                  \
	 LE64_TO_CPU(*((d64_t *)((hash3_t *)(ha))->objectid)) :        \
	 LE64_TO_CPU(*((d64_t *)((hash4_t *)(ha))->objectid)))

#define ha_set_objectid(ha, val, pol)	                               \
        ({if (pol == 3)                                                \
         (*(d64_t *)((hash3_t *)(ha))->objectid) = CPU_TO_LE64(val);   \
	 else                                                          \
	 (*(d64_t *)((hash4_t *)(ha))->objectid) = CPU_TO_LE64(val);})

#define ha_get_offset(ha, pol)		                               \
        ((pol == 3) ?                                                  \
         LE64_TO_CPU(*((d64_t *)((hash3_t *)(ha))->offset)) :          \
	 LE64_TO_CPU(*((d64_t *)((hash4_t *)(ha))->offset)))
	 
#define ha_set_offset(ha, val, pol)                                    \
        ({if (pol == 3)                                                \
        (*(d64_t *)((hash3_t *)(ha))->offset) = CPU_TO_LE64(val);      \
        else                                                           \
        (*(d64_t *)((hash4_t *)(ha))->offset) = CPU_TO_LE64(val);})

#define ha_get_ordering(ha, pol)		                       \
        ((pol == 3) ? 0 :                                              \
	 LE64_TO_CPU(*((d64_t *)((hash4_t *)(ha))->ordering)))
	 
#define ha_set_ordering(ha, val, pol)                                  \
        ({if (pol == 3) do {} while(0); else                           \
        (*(d64_t *)((hash4_t *)(ha))->ordering) = CPU_TO_LE64(val);})

#define ha_size(pol)                                                   \
        ((pol == 3) ? sizeof(hash3_t) : sizeof(hash4_t))

#define en_get_offset(en, pol)		                               \
        ((pol == 3) ?                                                  \
	 aal_get_le16(((entry3_t *)(en)), offset) :                    \
	 aal_get_le16(((entry4_t *)(en)), offset))

#define en_set_offset(en, val, pol)	                               \
        ({if (pol == 3)                                                \
         aal_set_le16(((entry3_t *)(en)), offset, val);                \
         else                                                          \
	 aal_set_le16(((entry4_t *)(en)), offset, val);})

#define en_size(pol)                                                   \
        ((pol == 3) ? sizeof(entry3_t) : sizeof(entry4_t))

#define cde_get_entry(pl, pos, pol)                                    \
        ((pol == 3) ?                                                  \
	 (void *)(&((cde403_t *)(pl)->body)->entry[pos]) :             \
	 (void *)(&((cde404_t *)(pl)->body)->entry[pos]))

#else
#if defined(ENABLE_SHORT_KEYS)
#define ob_get_locality(ob, pol)                                       \
	 LE64_TO_CPU(*((d64_t *)((objid3_t *)(ob))->locality))

#define ob_set_locality(ob, val, pol)	                               \
	 (*(d64_t *)((objid3_t *)(ob))->locality) = CPU_TO_LE64(val)

#define ob_get_objectid(ob, pol)                                       \
	 LE64_TO_CPU(*((d64_t *)((objid3_t *)(ob))->objectid))

#define ob_set_objectid(ob, val, pol)	                               \
	 (*(d64_t *)((objid3_t *)(ob))->objectid) = CPU_TO_LE64(val)

#define ob_get_ordering(ob, pol) (0)
#define ob_set_ordering(ob, val, pol) do {} while(0)

#define ob_size(pol)                                                   \
        (sizeof(objid3_t))

#define ha_get_objectid(ha, pol)		                       \
	 LE64_TO_CPU(*((d64_t *)((hash3_t *)(ha))->objectid))

#define ha_set_objectid(ha, val, pol)	                               \
         (*(d64_t *)((hash3_t *)(ha))->objectid) = CPU_TO_LE64(val)

#define ha_get_offset(ha, pol)		                               \
         LE64_TO_CPU(*((d64_t *)((hash3_t *)(ha))->offset))
	 
#define ha_set_offset(ha, val, pol)                                    \
        (*(d64_t *)((hash3_t *)(ha))->offset) = CPU_TO_LE64(val)

#define ha_get_ordering(ha, pol) (0)
#define ha_set_ordering(ha, val, pol) do {} while (0)

#define ha_size(pol)                                                   \
        (sizeof(hash3_t))

#define en_get_offset(en, pol)		                               \
	 aal_get_le16(((entry3_t *)(en)), offset)

#define en_set_offset(en, val, pol)	                               \
         aal_set_le16(((entry3_t *)(en)), offset, val)

#define en_size(pol)                                                   \
        (sizeof(entry3_t))

#define cde_get_entry(pl, pos, pol)                                    \
	 ((void *)(&((cde403_t *)pl->body)->entry[pos]))
#else
#define ob_get_locality(ob, pol)                                       \
	 LE64_TO_CPU(*((d64_t *)((objid4_t *)(ob))->locality))

#define ob_set_locality(ob, val, pol)	                               \
	 (*(d64_t *)((objid4_t *)(ob))->locality) = CPU_TO_LE64(val)

#define ob_get_objectid(ob, pol)                                       \
	 LE64_TO_CPU(*((d64_t *)((objid4_t *)(ob))->objectid))

#define ob_set_objectid(ob, val, pol)	                               \
	 (*(d64_t *)((objid4_t *)(ob))->objectid) = CPU_TO_LE64(val)

#define ob_get_ordering(ob, pol)                                       \
	 LE64_TO_CPU(*((d64_t *)((objid4_t *)(ob))->ordering))

#define ob_set_ordering(ob, val, pol)	                               \
	 (*(d64_t *)((objid4_t *)(ob))->ordering) = CPU_TO_LE64(val))

#define ob_size(pol)                                                   \
        (sizeof(objid4_t))

#define ha_get_objectid(ha, pol)		                       \
	 LE64_TO_CPU(*((d64_t *)((hash4_t *)(ha))->objectid))

#define ha_set_objectid(ha, val, pol)	                               \
         (*(d64_t *)((hash4_t *)(ha))->objectid) = CPU_TO_LE64(val)

#define ha_get_offset(ha, pol)		                               \
         LE64_TO_CPU(*((d64_t *)((hash4_t *)(ha))->offset))
	 
#define ha_set_offset(ha, val, pol)                                    \
        (*(d64_t *)((hash4_t *)(ha))->offset) = CPU_TO_LE64(val)

#define ha_get_ordering(ha, pol)                                       \
         LE64_TO_CPU(*((d64_t *)((hash4_t *)(ha))->ordering))

#define ha_set_ordering(ha, val, pol)                                  \
        (*(d64_t *)((hash4_t *)(ha))->ordering) = CPU_TO_LE64(val)

#define ha_size(pol)                                                   \
        (sizeof(hash4_t))

#define en_get_offset(en, pol)		                               \
	 aal_get_le16(((entry4_t *)(en)), offset)

#define en_set_offset(en, val, pol)	                               \
         aal_set_le16(((entry4_t *)(en)), offset, val)

#define en_size(pol)                                                   \
        (sizeof(entry4_t))

#define cde_get_entry(pl, pos, pol)                                    \
	 ((void *)(&((cde404_t *)pl->body)->entry[pos]))
#endif
#endif

#define cde_get_units(pl)		                               \
        aal_get_le16(((cde40_t *)pl->body), units)

#define cde_set_units(pl, val)		                               \
        aal_set_le16(((cde40_t *)pl->body), units, val)

#define cde_inc_units(pl, val)                                         \
        (cde_set_units(pl, (cde_get_units(pl) + val)))

#define cde_dec_units(pl, val)                                         \
        (cde_set_units(pl, (cde_get_units(pl) - val)))

#define en_inc_offset(en, val, pol)                                    \
        (en_set_offset(en, (en_get_offset(en, pol) + val), pol))

#define en_dec_offset(en, val, pol)                                    \
        (en_set_offset(en, (en_get_offset(en, pol) - val), pol))

#endif
