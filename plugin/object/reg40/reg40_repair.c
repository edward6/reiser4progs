/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   reg40_repair.c -- reiser4 default regular file plugin repair code. */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef ENABLE_STAND_ALONE

#include "reg40.h"

extern reiser4_plugin_t reg40_plugin;

static errno_t callback_mode_reg(uint16_t mode) {
	return S_ISREG(mode) ? 0 : -EINVAL;
}

static errno_t callback_type_file(uint16_t type) {
	return type == KEY_FILEBODY_TYPE ? 0 : -EINVAL;
}

object_entity_t *reg40_realize(object_info_t *info) {
	reg40_t *reg;
	
	if (obj40_realize(info, callback_mode_reg, callback_type_file))
		return NULL;
	
	if (!(reg = aal_calloc(sizeof(*reg), 0)))
		return NULL;
	
	/* Initializing file handle */
	obj40_init(&reg->obj, &reg40_plugin, NULL, core, info->tree);
	
	return (object_entity_t *)reg;
}

errno_t reg40_check_struct(object_entity_t *object, object_info_t *info, 
			   place_func_t place_func, uint16_t mode, void *data) 
{
	sdext_lw_hint_t lw_hint;
	key_entity_t key;
	lookup_t lookup;
	reg40_t *reg;
	errno_t res = 0;
#if 0
	aal_assert("vpf-1126", info != NULL);
	aal_assert("vpf-1126", info->tree != NULL);
	
	/* Recovery on the base of an item. */
	if (info->start.item.plugin) {
		uint64_t locality, objectid;
		
		/* Build the SD key on the base of the start place. */
		locality = plugin_call(info->start.item.key.plugin->o.key_ops,
				       get_locality, &info->object);
		objectid = plugin_call(info->start.item.plugin->o.key_ops,
				       get_objectid, &info->object);

		plugin_call(info->start.item.plugin->o.key_ops, build_generic,
			    &info->object, KEY_STATDATA_TYPE, locality, objectid, 0);
		
		/* If the specified place is not the place of the SD, find SD. */
		if (info->start.item.plugin->h.group != STATDATA_ITEM) {
			lookup = core->tree_ops.lookup(info->tree, &info->object,
						       LEAF_LEVEL, &info->start);
			
			switch(lookup) {
			case PRESENT:
				/* If the SD was found, check that this is reg40 
				   SD. Relocate othewise. 
				   FIXME-VITALY: relocation is not ready yet. */
				if ((res = obj40_read_lw(&info->start.item, &lw_hint)))
					return res;
				
				aal_assert("vpf-1128: Relocation is not ready yet.", 
					callback_mode_reg(lw_hint.mode));
				
				if (reg40_open(info) == NULL)
					return -EINVAL;
				
				break;
			case ABSENT:
				/* No SD found, create a new one. */
				if ((reg = reg40_create(info, hint)) == NULL)
					return -EINVAL;
				break;
			case FAILED:
				return -EINVAL;
			}
		} else {
			/* We are on the SD, open the reg40 object. */
			if ((reg = reg40_open(info)) == NULL)
				return -EINVAL;
		}
	} else {
		/* Key of SD was specified but SD item has not been found. */
		if ((reg = reg40_create(info, hint)) == NULL)
			return -EINVAL;
	}
	
	/* Reg40 object (its SD item) has been openned or created. */
	while (TRUE) {
		plugin_call(info->start.item.plugin->o.key_ops, build_generic, 
			    &key, KEY_FILEBODY_TYPE, locality, objectid, reg->offset);

		lookup = obj40_lookup(&reg->obj, &key, LEAF_LEVEL, &reg->body);
		
		switch(lookup) {
		case PRESENT:
			/* Get the maxreal key of the foudn item and find next. */
			if ((res = core->tree_ops.realize(info->tree, &reg->body)))
				return res;
			
			if ((res = plugin_call(reg->body.item->plugin->o.item_ops, 
					       maxreal_key, reg->body.item, &key)))
				return res;
			
			reg->offset = plugin_call(key.plugin->key_ops, get_offset, 
						  &key) + 1;
			
			continue;
		case ABSENT:
			/* If place is invalid, then no more reg40 body items exists. */
			if (!core->tree_ops.valid(info->tree, &reg->body))
				break;
			
			/* Initializing item entity at @next place */
			if ((res = core->tree_ops.realize(info->tree, &reg->body)))
				return res;
			
			/* Check if this is an item of another object. */
			if (plugin_call(key.plugin->o.key_ops, compare_short, 
					&key, &reg->body.item.key))
				break;

			/* Insert the hole. */

		case FAILED:
			return -EINVAL;
		}

		break;
	}

	reg40_close(reg);
	return 0;

 error_free_reg:
	reg40_close(reg);
#endif
	return -EINVAL;
}

#endif

