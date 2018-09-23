/*
  Copyright (c) 2018 Eduard O. Shishkin

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "../extent40/extent40.h"
#include "../extent40/extent40_repair.h"

reiser4_core_t *extent41_core = NULL;

static item_balance_ops_t balance_ops = {
#ifndef ENABLE_MINIMAL
	.merge		  = NULL,
	.update_key	  = NULL,
	.mergeable	  = extent40_mergeable,
	.prep_shift	  = extent40_prep_shift,
	.shift_units	  = extent40_shift_units,
	.maxreal_key	  = extent40_maxreal_key,
	.collision	  = NULL,
	.overhead	  = NULL,
#endif
	.init		  = NULL,
	.units		  = extent40_units,
	.lookup		  = extent40_lookup,
	.fetch_key	  = extent40_fetch_key,
	.maxposs_key      = extent40_maxposs_key
};

static item_object_ops_t object_ops = {
#ifndef ENABLE_MINIMAL
	.remove_units	  = extent40_remove_units,
	.update_units	  = extent40_update_units,
	.prep_insert	  = extent40_prep_insert,
	.insert_units	  = extent40_insert_units,
	.prep_write	  = extent40_prep_write,
	.write_units	  = extent40_write_units,
	.trunc_units	  = extent40_trunc_units,
	.layout		  = extent40_layout,
	.size		  = extent40_size,
	.bytes		  = extent40_bytes,
#endif
	.read_units	  = extent40_read_units,
	.fetch_units	  = extent40_fetch_units
};

#ifndef ENABLE_MINIMAL
static item_repair_ops_t repair_ops = {
	.check_layout	  = extent40_check_layout,
	.check_struct	  = extent40_check_struct,

	.prep_insert_raw  = extent40_prep_insert_raw,
	.insert_raw	  = extent40_insert_raw,

	.pack		  = NULL,
	.unpack		  = NULL
};

static item_debug_ops_t debug_ops = {
	.print		  = extent40_print,
};
#endif

reiser4_item_plug_t extent41_plug = {
	.p = {
		.id	= {ITEM_EXTENT41_ID, EXTENT_ITEM, ITEM_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "extent41",
		.desc  = "Extent striped file body item plugin.",
#endif
	},

	.object		  = &object_ops,
	.balance	  = &balance_ops,
#ifndef ENABLE_MINIMAL
	.repair		  = &repair_ops,
	.debug		  = &debug_ops,
#endif
};
