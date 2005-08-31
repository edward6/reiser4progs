/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40_repair.c -- reiser4 default symlink file plugin repair code. */

#include "sym40_repair.h"

#ifndef ENABLE_MINIMAL
#ifdef ENABLE_SYMLINKS

#define SYM40_EXTS_MUST ((uint64_t)1 << SDEXT_LW_ID | 1 << SDEXT_SYMLINK_ID)

object_entity_t *sym40_recognize(object_info_t *info) {
	sym40_t *sym;
	errno_t res;
	
	aal_assert("vpf-1124", info != NULL);
	
	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&sym->obj, info, sym40_core);
	
	if ((res = obj40_objkey_check(&sym->obj)))
		goto error;

	if ((res = obj40_check_stat(&sym->obj, SYM40_EXTS_MUST, 0)))
		goto error;

	return (object_entity_t *)sym;
 error:
	aal_free(sym);
	return res < 0 ? INVAL_PTR : NULL;
}

errno_t sym40_check_struct(object_entity_t *object,
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	sym40_t *sym = (sym40_t *)object;
	reiser4_place_t *place;
	obj40_stat_hint_t hint;
	obj40_stat_ops_t ops;
	errno_t res;
	char *path;
	
	aal_assert("vpf-1232", sym != NULL);
	aal_assert("vpf-1233", sym->obj.info.tree != NULL);
	aal_assert("vpf-1234", sym->obj.info.object.plug != NULL);

	place = STAT_PLACE(&sym->obj);
	
	aal_memset(&ops, 0, sizeof(ops));
	aal_memset(&hint, 0, sizeof(hint));
	
	if ((res = obj40_prepare_stat(&sym->obj, S_IFLNK, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(place, data))
		return -EINVAL;
	
	if (!(path = aal_calloc(place_blksize(place), 0)))
		return -ENOMEM;
	
	if ((res = obj40_read_ext(&sym->obj, SDEXT_SYMLINK_ID, path)))
		goto error;
	
	/* Fix the SD, if no fatal corruptions were found. */
	hint.mode = S_IFLNK;
	hint.size = aal_strlen(path);
	hint.must_exts = SYM40_EXTS_MUST;
	ops.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;
	
	if ((res = obj40_update_stat(&sym->obj, &ops, &hint, mode)))
		goto error;

	aal_free(path);
	return 0;

 error:
	aal_free(path);
	return res;
}
#endif
#endif
