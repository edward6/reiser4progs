/* Copyright 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40_repair.c -- reiser4 default symlink file plugin repair code. */

#ifndef ENABLE_STAND_ALONE

#include "sym40_repair.h"

#define SYM40_EXTS_MUST ((uint64_t)1 << SDEXT_LW_ID | 1 << SDEXT_SYMLINK_ID)

static errno_t sym40_extensions(reiser4_place_t *stat) {
	uint64_t extmask;
	
	extmask = obj40_extmask(stat);
	
	/* Check that LW, UNIX and SYMLINK extensions exist. */
	return ((extmask & SYM40_EXTS_MUST) == SYM40_EXTS_MUST) ? 0 : RE_FATAL;
}

/* Check SD extensions and that mode in LW extension is DIRFILE. */
static errno_t callback_stat(reiser4_place_t *stat) {
	sdext_lw_hint_t lw_hint;
	errno_t res;
	
	if ((res = sym40_extensions(stat)))
		return res;

	/* Check the mode in the LW extension. */
	if ((res = obj40_read_ext(stat, SDEXT_LW_ID, &lw_hint)))
		return res;
	
	return S_ISLNK(lw_hint.mode) ? 0 : RE_FATAL;

	/* FIXME: read object plug_id extention from sd. if present also. */
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

errno_t sym40_check_struct(object_entity_t *object,
			   place_func_t place_func,
			   void *data, uint8_t mode)
{
	sym40_t *sym = (sym40_t *)object;
	obj40_stat_methods_t methods;
	obj40_stat_params_t params;
	reiser4_place_t *place;
	errno_t res;
	char *path;
	
	aal_assert("vpf-1232", sym != NULL);
	aal_assert("vpf-1233", sym->obj.info.tree != NULL);
	aal_assert("vpf-1234", sym->obj.info.object.plug != NULL);

	place = STAT_PLACE(&sym->obj);
	
	aal_memset(&methods, 0, sizeof(methods));
	aal_memset(&params, 0, sizeof(params));
	
	if ((res = obj40_prepare_stat(&sym->obj, S_IFLNK, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (place_func && place_func(place, data))
		return -EINVAL;
	
	if (!(path = aal_calloc(place->node->block->size, 0)))
		return -ENOMEM;
		
	if ((res = obj40_read_ext(place, SDEXT_SYMLINK_ID, path)))
		goto error;
	
	/* Fix the SD, if no fatal corruptions were found. */
	params.mode = S_IFLNK;
	params.size = aal_strlen(path);
	
	methods.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;
	
	if ((res = obj40_check_stat(&sym->obj, &methods, 
				    &params, mode)))
		goto error;

	aal_free(path);
	return 0;

 error:
	aal_free(path);
	return res;
}
#endif
