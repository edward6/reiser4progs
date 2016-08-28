/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   obj40_repair.h -- reiser4 file plugins common repair structures and methods. */

#ifndef OBJ40_REPAIR_H
#define OBJ40_REPAIR_H

#include "obj40.h"
#include "repair/plugin.h"

typedef errno_t (*stat_func_t) (reiser4_place_t *);

#define SKIP_METHOD	((void *)-1)

typedef struct obj40_stat_ops {
	int (*check_mode) (reiser4_object_t *obj, uint16_t *, uint16_t);
	int (*check_nlink) (reiser4_object_t *obj, uint32_t *, uint32_t);
	int (*check_size) (reiser4_object_t *obj, uint64_t *, uint64_t);
	int (*check_bytes) (reiser4_object_t *obj, uint64_t *, uint64_t);
	int (*check_bytes_report) (reiser4_place_t *start,
				   reiser4_core_t *core,
				   uint64_t found_bytes,
				   uint8_t mode,
				   uint64_t correct_bytes);
} obj40_stat_ops_t;

typedef struct obj40_stat_hint {
	uint64_t size;
	uint64_t bytes;
	uint32_t nlink;
	uint16_t mode;
} obj40_stat_hint_t;

extern uint64_t obj40_place_maxreal(reiser4_place_t *place);

extern errno_t obj40_update_stat(reiser4_object_t *obj, 
				 obj40_stat_ops_t *ops,
				 obj40_stat_hint_t *hint, 
				 uint8_t mode);

extern errno_t obj40_prepare_stat(reiser4_object_t *obj, 
				  uint16_t objmode, 
				  uint8_t mode);

extern errno_t obj40_recognize(reiser4_object_t *obj);

extern lookup_t obj40_check_item(reiser4_object_t *obj, 
				 obj_func_t item_func,
				 obj_func_t update_func,
				 void *data);

#endif

/*
  Local variables:
  c-indentation-style: "K&R"
  mode-name: "LC"
  c-basic-offset: 8
  tab-width: 8
  fill-column: 80
  scroll-step: 1
  End:
*/
