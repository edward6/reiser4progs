/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   spl40_repair.c -- reiser4 special files plugin repair code. */

#ifndef ENABLE_STAND_ALONE

#include "spl40_repair.h"

/* Set of extentions that must present. */
#define SPL40_EXTS_MUST ((uint64_t)1 << SDEXT_LW_ID)

/* Set of unknown extentions. */
#define SPL40_EXTS_UNKN ((uint64_t)1 << SDEXT_SYMLINK_ID)

static errno_t spl40_extensions(reiser4_place_t *stat) {
	uint64_t extmask;
	
	extmask = obj40_extmask(stat);
	
	/* Check that there is no one unknown extension. */
	if (extmask & SPL40_EXTS_UNKN)
		return RE_FATAL;
	
	/* Check that LW and UNIX extensions exist. */
	return ((extmask & SPL40_EXTS_MUST) == SPL40_EXTS_MUST) ? 0 : RE_FATAL;

	/* FIXME: read object plug_id extention from sd. if present also. */
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

static int spl40_check_mode(obj40_t *obj, uint16_t *mode, uint16_t correct) {
	if (!S_ISCHR(*mode) && !S_ISBLK(*mode) && 
	    !S_ISFIFO(*mode) && !S_ISSOCK(*mode))
	{
		*mode &= ~S_IFMT;
        	*mode |= S_IFBLK;
	}
}

errno_t spl40_check_struct(object_entity_t *object,
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	spl40_t *spl = (spl40_t *)object;
	obj40_stat_methods_t methods;
	obj40_stat_params_t params;
	errno_t res;
	
	aal_assert("vpf-1357", spl != NULL);
	aal_assert("vpf-1358", spl->obj.info.tree != NULL);
	aal_assert("vpf-1359", spl->obj.info.object.plug != NULL);

	aal_memset(&methods, 0, sizeof(methods));
	aal_memset(&params, 0, sizeof(params));

	if ((res = obj40_prepare_stat(&spl->obj, S_IFBLK, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(&spl->obj.info.start, data))
		return -EINVAL;
	
	methods.check_mode = spl40_check_mode;
	methods.check_bytes = SKIP_METHOD;
	methods.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;
	
	/* Fix the SD, if no fatal corruptions were found. */
	return obj40_check_stat(&spl->obj, &methods,
				&params, mode);
}

#endif
