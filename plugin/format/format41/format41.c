/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   format41.c -- disk-layout plugin for logical (compound) volumes */

#include "../format40/format40.h"
#include "../format40/format40_repair.h"
#include "format41.h"
#include "format41_repair.h"
#include <misc/misc.h>

reiser4_core_t *format41_core = NULL;

#ifndef ENABLE_MINIMAL

static void set_sb_format41(format40_super_t *super, format_hint_t *desc)
{
	set_sb_format40(super, desc);

	set_sb_subvol_id(super, desc->subvol_id);
	set_sb_num_subvols(super, desc->num_subvols);
	set_sb_num_sgs_bits(super, desc->num_sgs_bits);
	set_sb_data_room(super, desc->data_room_size);
}

reiser4_format_ent_t *format41_create(aal_device_t *device, format_hint_t *desc)
{
	return format40_create_common(device, desc, set_sb_format41);
}
#endif

errno_t check_super_format41(format40_super_t *super)
{
	errno_t ret;

	ret = check_super_format40(super);
	if (ret)
		return ret;
	return 0;
}

static errno_t format41_super_open(format40_t *format)
{
	return format40_super_open_common(format, check_super_format41);
}

static reiser4_format_ent_t *format41_open(aal_device_t *device,
					   uint32_t blksize)
{
	return format40_open_common(device, blksize, &format41_plug,
				    format41_super_open);
}

reiser4_format_plug_t format41_plug = {
	.p = {
		.id	= {FORMAT_REISER41_ID, 0, FORMAT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "format41",
		.desc  = "Standard layout for logical volumes.",
#endif
	},

#ifndef ENABLE_MINIMAL
	.valid		= format40_valid,
	.sync		= format40_sync,
	.create		= format41_create,
	.print		= format41_print,
	.layout	        = format40_layout,
	.update		= format40_update,
	.start		= format40_start,

	.pack           = format40_pack,
	.unpack         = format41_unpack,

	.get_len	= format40_get_len,
	.get_free	= format40_get_free,
	.get_stamp	= format40_get_stamp,
	.get_policy	= format40_get_policy,

	.set_root	= format40_set_root,
	.set_len	= format40_set_len,
	.set_free	= format40_set_free,
	.set_data_room  = format40_set_data_room,
	.set_height	= format40_set_height,
	.set_stamp	= format40_set_stamp,
	.set_policy	= format40_set_policy,
	.set_state      = format40_set_state,
	.get_state      = format40_get_state,
	.oid_pid	= format40_oid_pid,
	.oid_area       = format40_oid_area,
	.journal_pid	= format40_journal_pid,
	.alloc_pid	= format40_alloc_pid,
	.node_pid       = format40_node_pid,
	.backup		= format40_backup,
	.check_backup	= format41_check_backup,
	.regenerate     = format41_regenerate,
	.check_struct	= format41_check_struct,
	.version	= format40_version,
#endif
	.open		= format41_open,
	.close		= format40_close,

	.get_root	= format40_get_root,
	.get_height	= format40_get_height,
	.key_pid        = format40_get_key,
};

/*
  Local variables:
  c-indentation-style: "K&R"
  mode-name: "LC"
  c-basic-offset: 8
  tab-width: 8
  fill-column: 80
  scroll-step: 1
  End:
*/
