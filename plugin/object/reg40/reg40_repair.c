/* Copyright 2001-2003 by Hans Reiser, licensing governed by reiser4progs/COPYING.
   
   reg40_repair.c -- reiser4 default regular file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "reg40.h"

static errno_t callback_mode_reg(uint16_t mode) {
	return S_ISREG(mode) ? 0 : -EINVAL;
}

static errno_t callback_type_file(uint16_t type) {
	return type == KEY_FILEBODY_TYPE ? 0 : -EINVAL;
}

errno_t reg40_realize(object_info_t *info) {
	return obj40_realize(info, callback_mode_reg, callback_type_file);
}

object_entity_t *reg40_check_struct(object_info_t *info,
				    place_func_t func,
				    uint16_t mode, void *data) 
{
	reg40_t *reg;
	
	aal_assert("vpf-1126", info != NULL);
	
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;
	
	if (!info->object.plugin) {
		uint64_t locality, objectid;
		
		/* Build the SD key. */
		locality = plugin_call(info->start.item.key.plugin->o.key_ops,
				       get_locality, &info->object);
		objectid = plugin_call(info->start.item.plugin->o.key_ops,
				       get_objectid, &info->object);

		plugin_call(info->start.item.plugin->o.key_ops, build_generic,
			    &info->object, KEY_STATDATA_TYPE, locality, objectid, 0);
	}

#if 0
	if (info->start.item.plugin) {
		if (info->start.item.plugin->h.group != STATDATA_ITEM)
	}
#endif
	
 error_free_reg:
	aal_free(reg);
	return NULL;
}

#endif

