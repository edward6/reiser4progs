/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.h -- reiser4 file plugins common repair structures and methods. */

#ifndef OBJ40_REPAIR_H
#define OBJ40_REPAIR_H

#include "obj40.h"
#include "repair/plugin.h"

typedef errno_t (*stat_func_t) (reiser4_place_t *);

#define SKIP_METHOD	((void *)-1)

struct obj40_stat_methods {
	int (*check_mode) (obj40_t *obj, uint16_t *, uint16_t);
	int (*check_nlink) (obj40_t *obj, uint32_t *, uint32_t);
	int (*check_size) (obj40_t *obj, uint64_t *, uint64_t);
	int (*check_bytes) (obj40_t *obj, uint64_t *, uint64_t);
};

typedef struct obj40_stat_methods obj40_stat_methods_t;

struct obj40_stat_params {
	uint64_t size;
	uint64_t bytes;
	uint32_t nlink;
	uint16_t mode;
	uint64_t must_exts;
	uint64_t unkn_exts;
};

typedef struct obj40_stat_params obj40_stat_params_t;

extern errno_t obj40_objkey_check(obj40_t *obj);

extern errno_t obj40_save_stat(obj40_t *obj, statdata_hint_t *hint);

extern errno_t obj40_check_stat(obj40_t *obj, 
				uint64_t exts_must, 
				uint64_t exts_unkn);

extern errno_t obj40_update_stat(obj40_t *obj, 
				 obj40_stat_methods_t *methods,
				 obj40_stat_params_t *params,
				 uint8_t mode);

extern errno_t obj40_fix_key(obj40_t *obj, reiser4_place_t *place, 
			     reiser4_key_t *key, uint8_t mode);

extern errno_t obj40_prepare_stat(obj40_t *obj, uint16_t objmode, uint8_t mode);

#endif
