/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   spl40_repair.c -- reiser4 special files plugin repair code. */

#ifndef ENABLE_STAND_ALONE
#include "spl40.h"
#include "repair/plugin.h"

#define spl40_exts ((uint64_t)1 << SDEXT_LW_ID)

static errno_t spl40_extensions(reiser4_place_t *stat) {
	uint64_t extmask;
	
	extmask = obj40_extmask(stat);
	
	/* Check that there is no one unknown extension. */
	/*
	if (extmask & ~(sym40_exts | 1 << SDEXT_PLUG_ID))
		return RE_FATAL;
	*/
	/* Check that LW and UNIX extensions exist. */
	return ((extmask & spl40_exts) == spl40_exts) ? 0 : RE_FATAL;
}

/* Check SD extensions and that mode in LW extension is DIRFILE. */
static errno_t callback_stat(reiser4_place_t *stat) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = spl40_extensions(stat)))
		return res;

	/* Check the mode in the LW extension. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &lw_hint)))
		return res;
	
	return  S_ISCHR(lw_hint.mode) || S_ISBLK(lw_hint.mode) || 
		S_ISFIFO(lw_hint.mode) || S_ISSOCK(lw_hint.mode) ? 
		0 : RE_FATAL;
}

object_entity_t *spl40_recognize(object_info_t *info) {
	spl40_t *spl;
	errno_t res;
	
	aal_assert("vpf-1356", info != NULL);
	
	if (!(spl = aal_calloc(sizeof(*spl), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&spl->obj, &spl40_plug, spl40_core, info);
	
	if ((res = obj40_recognize(&spl->obj, callback_stat)))
		goto error;
	
	return (object_entity_t *)spl;
 error:
	aal_free(spl);
	return res < 0 ? INVAL_PTR : NULL;
}

static void spl40_zero_nlink(obj40_t *obj, uint32_t *nlink) {
	*nlink = 0;
}

static void spl40_check_mode(obj40_t *obj, uint16_t *mode) {
	if (!S_ISCHR(*mode) && !S_ISBLK(*mode) && 
	    !S_ISFIFO(*mode) && !S_ISSOCK(*mode))
	{
		*mode &= ~S_IFMT;
        	*mode |= S_IFBLK;
	}
}

static void spl40_check_size(obj40_t *obj, uint64_t *sd_size, uint64_t size) {
	if (*sd_size != size)
		*sd_size = size;
}

errno_t spl40_check_struct(object_entity_t *object,
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	spl40_t *spl = (spl40_t *)object;
	errno_t res;
	
	aal_assert("vpf-1357", spl != NULL);
	aal_assert("vpf-1358", spl->obj.info.tree != NULL);
	aal_assert("vpf-1359", spl->obj.info.object.plug != NULL);

	if ((res = obj40_launch_stat(&spl->obj, spl40_extensions, 
				     0, 1, 0, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(&spl->obj.info.start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((res = obj40_fix_key(&spl->obj, &spl->obj.info.start,
				 &spl->obj.info.object, mode)))
	{
		return res;
	}
	
	/* Fix the SD, if no fatal corruptions were found. */
	return obj40_check_stat(&spl->obj, mode == RM_BUILD ? 
				spl40_zero_nlink : NULL,
				spl40_check_mode, spl40_check_size,
				0, MAX_UINT64, mode);
}

#endif
