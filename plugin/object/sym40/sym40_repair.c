/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40_repair.c -- reiser4 default symlink file plugin repair code. */

#ifndef ENABLE_STAND_ALONE

#include "sym40.h"
#include "repair/plugin.h"

#define sym40_exts ((uint64_t)1 << SDEXT_UNIX_ID |	\
			      1 << SDEXT_LW_ID |	\
			      1 << SDEXT_SYMLINK_ID)

static errno_t sym40_extensions(place_t *stat) {
	uint64_t extmask;
	
	extmask = obj40_extmask(stat);
	
	/* Check that there is no one unknown extension. */
	/*
	if (extmask & ~(sym40_exts | 1 << SDEXT_PLUG_ID))
		return RE_FATAL;
	*/
	/* Check that LW, UNIX and SYMLINK extensions exist. */
	return ((extmask & sym40_exts) == sym40_exts) ? 0 : RE_FATAL;
}

/* Check SD extensions and that mode in LW extension is DIRFILE. */
static errno_t callback_stat(place_t *stat) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = sym40_extensions(stat)))
		return res;

	/* Check the mode in the LW extension. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &lw_hint)))
		return res;
	
	return S_ISLNK(lw_hint.mode) ? 0 : RE_FATAL;
}

object_entity_t *sym40_recognize(object_info_t *info) {
	sym40_t *sym;
	errno_t res;
	
	aal_assert("vpf-1124", info != NULL);
	
	if (!(sym = aal_calloc(sizeof(*sym), 0)))
		return INVAL_PTR;
	
	/* Initializing file handle */
	obj40_init(&sym->obj, &sym40_plug, sym40_core, info);
	
	if ((res = obj40_recognize(&sym->obj, callback_stat)))
		goto error;
	
	return (object_entity_t *)sym;
 error:
	aal_free(sym);
	return res < 0 ? INVAL_PTR : NULL;
}

static void sym40_zero_nlink(obj40_t *obj, uint32_t *nlink) {
	*nlink = 0;
}

static void sym40_check_mode(obj40_t *obj, uint16_t *mode) {
	if (!S_ISDIR(*mode)) {
		*mode &= ~S_IFMT;
        	*mode |= S_IFLNK;
	}
}

static void sym40_check_size(obj40_t *obj, uint64_t *sd_size, uint64_t size) {
	if (*sd_size != size)
		*sd_size = size;
}

errno_t sym40_check_struct(object_entity_t *object,
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	sym40_t *sym = (sym40_t *)object;
	char path[_SYMLINK_LEN];
	errno_t res;
	
	aal_assert("vpf-1232", sym != NULL);
	aal_assert("vpf-1233", sym->obj.info.tree != NULL);
	aal_assert("vpf-1234", sym->obj.info.object.plug != NULL);

	if ((res = obj40_launch_stat(&sym->obj, sym40_extensions, 
				     sym40_exts, 1, S_IFLNK, mode)))
	{
		return res;
	}
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(&sym->obj.info.start, data))
		return -EINVAL;
	
	/* Fix SD's key if differs. */
	if ((res = obj40_fix_key(&sym->obj, &sym->obj.info.start,
				 &sym->obj.info.object, mode)))
	{
		return res;
	}
	
	if ((res = obj40_read_ext(STAT_PLACE(&sym->obj),
				  SDEXT_SYMLINK_ID, path)))
	{
		return res;
	}
	
	/* Fix the SD, if no fatal corruptions were found. */
	return obj40_check_stat(&sym->obj, mode == RM_BUILD ? 
				sym40_zero_nlink : NULL,
				sym40_check_mode, 
				sym40_check_size, 
				aal_strlen(path), 0, mode);
}
#endif
