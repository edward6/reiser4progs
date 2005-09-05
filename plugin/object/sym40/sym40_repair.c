/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40_repair.c -- reiser4 default symlink file plugin repair code. */

#include "sym40_repair.h"

#ifndef ENABLE_MINIMAL
#ifdef ENABLE_SYMLINKS

errno_t sym40_check_struct(reiser4_object_t *sym,
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	reiser4_place_t *place;
	obj40_stat_hint_t hint;
	obj40_stat_ops_t ops;
	errno_t res;
	char *path;
	
	aal_assert("vpf-1232", sym != NULL);
	aal_assert("vpf-1233", sym->info.tree != NULL);
	aal_assert("vpf-1234", sym->info.object.plug != NULL);

	place = STAT_PLACE(sym);
	
	aal_memset(&ops, 0, sizeof(ops));
	aal_memset(&hint, 0, sizeof(hint));
	
	if ((res = obj40_prepare_stat(sym, S_IFLNK, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(place, data))
		return -EINVAL;
	
	if (!(path = aal_calloc(place_blksize(place), 0)))
		return -ENOMEM;
	
	if ((res = obj40_read_ext(sym, SDEXT_SYMLINK_ID, path)))
		goto error;
	
	/* Fix the SD, if no fatal corruptions were found. */
	hint.mode = S_IFLNK;
	hint.size = aal_strlen(path);
	ops.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;
	
	if ((res = obj40_update_stat(sym, &ops, &hint, mode)))
		goto error;

	aal_free(path);
	return 0;

 error:
	aal_free(path);
	return res;
}
#endif
#endif
