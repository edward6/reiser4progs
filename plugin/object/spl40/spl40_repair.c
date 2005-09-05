/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   spl40_repair.c -- reiser4 special files plugin repair code. */

#ifndef ENABLE_MINIMAL
#ifdef ENABLE_SPECIAL

#include "spl40_repair.h"

/* Set of extentions that must present. */
#define SPL40_EXTS_MUST ((uint64_t)1 << SDEXT_LW_ID)

/* Set of unknown extentions. */
#define SPL40_EXTS_UNKN ((uint64_t)1 << SDEXT_SYMLINK_ID)

errno_t spl40_recognize(reiser4_object_t *spl) {
	errno_t res;
	
	aal_assert("vpf-1356", spl != NULL);
	
	/* Initializing file handle */
	obj40_init(spl);
	
	if ((res = obj40_objkey_check(spl)))
		return res;

	if ((res = obj40_check_stat(spl, SPL40_EXTS_MUST,
				    SPL40_EXTS_UNKN)))
	{
		return res;
	}

	return 0;
}

static int spl40_check_mode(reiser4_object_t *spl, 
			    uint16_t *mode, 
			    uint16_t correct) 
{
	if (S_ISCHR(*mode) || S_ISBLK(*mode) ||
	    S_ISFIFO(*mode) || S_ISSOCK(*mode))
	{
		return 0;
	}
		
	*mode &= ~S_IFMT;
	*mode |= S_IFBLK;
	return 1;
}

errno_t spl40_check_struct(reiser4_object_t *spl,
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	obj40_stat_hint_t hint;
	obj40_stat_ops_t ops;
	errno_t res;
	
	aal_assert("vpf-1357", spl != NULL);
	aal_assert("vpf-1358", spl->info.tree != NULL);
	aal_assert("vpf-1359", spl->info.object.plug != NULL);

	aal_memset(&ops, 0, sizeof(ops));
	aal_memset(&hint, 0, sizeof(hint));

	if ((res = obj40_prepare_stat(spl, S_IFBLK, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(&spl->info.start, data))
		return -EINVAL;
	
	hint.must_exts = SPL40_EXTS_MUST;
	hint.unkn_exts = SPL40_EXTS_UNKN;
	
	ops.check_mode = spl40_check_mode;
	ops.check_bytes = SKIP_METHOD;
	ops.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;
	
	/* Fix the SD, if no fatal corruptions were found. */
	return obj40_update_stat(spl, &ops, &hint, mode);
}

#endif
#endif
