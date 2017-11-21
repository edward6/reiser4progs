/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   cde40.h -- reiser4 directory entry plugin. */

#ifndef CDE40_H
#define CDE40_H

#include <aal/libaal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

/* The cde40 structure is as the following:
   +-----------------------------+-----------------------------------------------+
   |           Unit Headers      |                   Units.                      |
   +-----------------------------+-----------------------------------------------+
   |                             |                     |   |                     |
   |units|entryX[0]|...|entryX[N]|objidX[0]|name[0]'\0'|...|objidX[N]|name[N]'\0'|
   |                             |                     |   |                     |
   +-----------------------------+-----------------------------------------------+ */

typedef struct cde40 {
	d16_t units;
} cde40_t;

#ifdef ENABLE_SHORT_KEYS
typedef struct objid3 {
	d8_t locality[8];
	d8_t objectid[8];
} objid3_t;


typedef struct hash3 {
	d8_t objectid[8];
	d8_t offset[8];
} hash3_t;

typedef struct entry3 {
	hash3_t hash;
	d16_t offset;
} entry3_t;

typedef struct cde403 {
	d16_t units;
	entry3_t entry[0];
} cde403_t;
#endif

#ifdef ENABLE_LARGE_KEYS
typedef struct objid4 {
	d8_t locality[8];
	d8_t ordering[8];
	d8_t objectid[8];
} objid4_t;

typedef struct hash4 {
	d8_t ordering[8];
	d8_t objectid[8];
	d8_t offset[8];
} hash4_t;

typedef struct entry4 {
	hash4_t hash;
	d16_t offset;
} entry4_t;

typedef struct cde404 {
	d16_t units;
	entry4_t entry[0];
} cde404_t;
#endif

extern reiser4_core_t *cde40_core;

extern uint32_t cde40_units(reiser4_place_t *place);

extern uint32_t cde40_key_pol(reiser4_place_t *place);

extern char *cde40_get_name(reiser4_place_t *place, uint32_t pos,
			    char *buff, uint32_t len);

extern void *cde40_entry(reiser4_place_t *place, uint32_t pos);
extern void *cde40_objid(reiser4_place_t *place, uint32_t pos);

extern errno_t cde40_maxposs_key(reiser4_place_t *place,
				 reiser4_key_t *key);

extern errno_t cde40_delete(reiser4_place_t *place, uint32_t pos,
   	                    trans_hint_t *hint);

extern errno_t cde40_get_hash(reiser4_place_t *place, uint32_t pos, 
			      reiser4_key_t *key);

extern errno_t cde40_copy(reiser4_place_t *dst_place, uint32_t dst_pos,
			  reiser4_place_t *src_place, uint32_t src_pos,
			  uint32_t count);

extern uint32_t cde40_expand(reiser4_place_t *place, uint32_t pos,
			     uint32_t count, uint32_t len);

extern uint32_t cde40_regsize(reiser4_place_t *place, uint32_t pos, 
			      uint32_t count);

extern lookup_t cde40_lookup(reiser4_place_t *place,
			     lookup_hint_t *hint, lookup_bias_t bias);

extern int cde40_comp_hash(reiser4_place_t *place, uint32_t pos, 
			   reiser4_key_t *key);

extern uint32_t cde40_cut(reiser4_place_t *place, uint32_t pos, 
			  uint32_t count, uint32_t len);

extern uint16_t cde40_overhead();

#if defined(ENABLE_SHORT_KEYS) && defined(ENABLE_LARGE_KEYS)

/* objidN_t macros. */
#define ob_loc(ob, pol)							\
	((pol == 3) ?							\
	 ((objid3_t *)(ob))->locality :					\
	 ((objid4_t *)(ob))->locality)
 
#define ob_get_locality(ob, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)ob_loc(ob, pol)))

#define ob_set_locality(ob, val, pol)					\
	put_unaligned(CPU_TO_LE64(val), (d64_t *)ob_loc(ob, pol))

#define ob_oid(ob, pol)							\
	((pol == 3) ?							\
	 ((objid3_t *)(ob))->objectid :					\
	 ((objid4_t *)(ob))->objectid)

#define ob_get_objectid(ob, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)ob_oid(ob, pol)))

#define ob_set_objectid(ob, val, pol)					\
	put_unaligned(CPU_TO_LE64(val), (d64_t *)ob_oid(ob, pol))

#define ob_ord(ob, pol) ((pol == 3) ? 0 : ((objid4_t *)(ob))->ordering)

#define ob_get_ordering(ob, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)ob_ord(ob, pol)))

#define ob_set_ordering(ob, val, pol)					\
	({if (pol == 3) do {} while(0); else				\
	 put_unaligned(CPU_TO_LE64(val), (d64_t *)ob_ord(ob, pol));})

#define ob_size(pol) ((pol == 3) ? sizeof(objid3_t) : sizeof(objid4_t))

/* hashN_t macros.  */
#define ha_oid(ha, pol)							\
	((pol == 3) ?							\
	 ((hash3_t *)(ha))->objectid :					\
	 ((hash4_t *)(ha))->objectid)

#define ha_get_objectid(ha, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)ha_oid(ha, pol)))

#define ha_set_objectid(ha, val, pol)					\
	put_unaligned(CPU_TO_LE64(val), (d64_t *)ha_oid(ha, pol))

#define ha_off(ha, pol)							\
	((pol == 3) ?							\
	 ((hash3_t *)(ha))->offset :					\
	 ((hash4_t *)(ha))->offset)

#define ha_get_offset(ha, pol)						\
	LE64_TO_CPU(get_unaligned((d64_t *)ha_off(ha, pol)))

#define ha_set_offset(ha, val, pol)					\
	put_unaligned(CPU_TO_LE64(val), (d64_t *)ha_off(ha, pol))

#define ha_ord(ha, pol) ((pol == 3) ? 0 : ((hash4_t *)(ha))->ordering)

#define ha_get_ordering(ha, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)ha_ord(ha, pol)))

#define ha_set_ordering(ha, val, pol)					\
	({if (pol == 3) do {} while(0); else				\
	 put_unaligned(CPU_TO_LE64(val), (d64_t *)ha_ord(ha, pol));})

#define ha_size(pol) ((pol == 3) ? sizeof(hash3_t) : sizeof(hash4_t))

/* entryN_t macros */
#define en_get_offset(en, pol)						\
        ((pol == 3) ?							\
	 aal_get_le16(((entry3_t *)(en)), offset) :			\
	 aal_get_le16(((entry4_t *)(en)), offset))

#define en_set_offset(en, val, pol)					\
        ({if (pol == 3)							\
         aal_set_le16(((entry3_t *)(en)), offset, val);			\
         else								\
	 aal_set_le16(((entry4_t *)(en)), offset, val);})

#define en_size(pol)							\
        ((pol == 3) ? sizeof(entry3_t) : sizeof(entry4_t))

#define cde_get_entry(pl, pos, pol)					\
        ((pol == 3) ?							\
	 (void *)(&((cde403_t *)(pl)->body)->entry[pos]) :		\
	 (void *)(&((cde404_t *)(pl)->body)->entry[pos]))

#elif defined(ENABLE_SHORT_KEYS)

/* objidN_t macros.  */
#define ob_get_locality(ob, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)((objid3_t *)(ob))->locality))

#define ob_set_locality(ob, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((objid3_t *)(ob))->locality)

#define ob_get_objectid(ob, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)((objid3_t *)(ob))->objectid))

#define ob_set_objectid(ob, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((objid3_t *)(ob))->objectid)

#define ob_get_ordering(ob, pol) (0)
#define ob_set_ordering(ob, val, pol) do {} while(0)

#define ob_size(pol) (sizeof(objid3_t))

/* hashN_t macros.  */
#define ha_get_objectid(ha, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)((hash3_t *)(ha))->objectid))

#define ha_set_objectid(ha, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((hash3_t *)(ha))->objectid)

#define ha_get_offset(ha, pol)						\
	LE64_TO_CPU(get_unaligned((d64_t *)((hash3_t *)(ha))->offset))

#define ha_set_offset(ha, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((hash3_t *)(ha))->offset)

#define ha_get_ordering(ha, pol) (0)
#define ha_set_ordering(ha, val, pol) do {} while (0)

#define ha_size(pol) (sizeof(hash3_t))

/* entryN_t macros */
#define en_get_offset(en, pol)						\
	 aal_get_le16(((entry3_t *)(en)), offset)

#define en_set_offset(en, val, pol)					\
         aal_set_le16(((entry3_t *)(en)), offset, val)

#define en_size(pol) (sizeof(entry3_t))

#define cde_get_entry(pl, pos, pol)					\
	 ((void *)(&((cde403_t *)pl->body)->entry[pos]))

#elif defined(ENABLE_LARGE_KEYS)
/* objidN_t macros. */
#define ob_get_locality(ob, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)((objid4_t *)(ob))->locality))

#define ob_set_locality(ob, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((objid4_t *)(ob))->locality)

#define ob_get_objectid(ob, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)((objid4_t *)(ob))->objectid))

#define ob_set_objectid(ob, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((objid4_t *)(ob))->objectid)

#define ob_get_ordering(ob, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)((objid4_t *)(ob))->ordering))

#define ob_set_ordering(ob, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((objid4_t *)(ob))->ordering)

#define ob_size(pol) (sizeof(objid4_t))

/* hashN_t macros.  */
#define ha_get_objectid(ha, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)((hash4_t *)(ha))->objectid))

#define ha_set_objectid(ha, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((hash4_t *)(ha))->objectid)

#define ha_get_offset(ha, pol)						\
	LE64_TO_CPU(get_unaligned((d64_t *)((hash4_t *)(ha))->offset))
	 
#define ha_set_offset(ha, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((hash4_t *)(ha))->offset)

#define ha_get_ordering(ha, pol)					\
	LE64_TO_CPU(get_unaligned((d64_t *)((hash4_t *)(ha))->ordering))

#define ha_set_ordering(ha, val, pol)					\
	put_unaligned(CPU_TO_LE64(val),					\
		      (d64_t *)((hash4_t *)(ha))->ordering)

#define ha_size(pol) (sizeof(hash4_t))

#define en_get_offset(en, pol)						\
	 aal_get_le16(((entry4_t *)(en)), offset)

#define en_set_offset(en, val, pol)					\
         aal_set_le16(((entry4_t *)(en)), offset, val)

#define en_size(pol) (sizeof(entry4_t))

#define cde_get_entry(pl, pos, pol)					\
	 ((void *)(&((cde404_t *)pl->body)->entry[pos]))
#endif

#define cde_get_units(pl)						\
        aal_get_le16(((cde40_t *)pl->body), units)

#define cde_set_units(pl, val)						\
        aal_set_le16(((cde40_t *)pl->body), units, val)

#define cde_inc_units(pl, val)						\
        (cde_set_units(pl, (cde_get_units(pl) + val)))

#define cde_dec_units(pl, val)						\
        (cde_set_units(pl, (cde_get_units(pl) - val)))

#define cde_get_offset(pl, n, pol)					\
	((uint32_t)en_get_offset(cde_get_entry(pl, n, pol), pol))

#define cde_set_offset(pl, n, offset, pol)				\
	(en_set_offset(cde_get_entry(pl, n, pol), offset, pol))

#define en_inc_offset(en, val, pol)					\
        (en_set_offset(en, (en_get_offset(en, pol) + val), pol))

#define en_dec_offset(en, val, pol)					\
        (en_set_offset(en, (en_get_offset(en, pol) - val), pol))

#endif
