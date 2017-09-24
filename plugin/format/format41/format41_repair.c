/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   format41_repair.c -- repair methods of the disk-layout plugin for
   compound (logical) volumes
*/

#ifndef FORMAT41_REPAIR_H
#define FORMAT41_REPAIR_H

#ifndef ENABLE_MINIMAL

#include "format41.h"
#include "../format40/format40.h"
#include "../format40/format40_repair.h"

reiser4_format_ent_t *format41_unpack(aal_device_t *device,
				      uint32_t blksize,
				      aal_stream_t *stream)
{
	return format40_unpack_common(device, blksize, stream, &format41_plug);
}

void format41_print(reiser4_format_ent_t *entity,
		    aal_stream_t *stream, uint16_t options)
{
	format40_t *format = (format40_t *)entity;
	format40_super_t *super = &format->super;

	format40_print_common(entity, stream, options, format41_core);

	aal_stream_format(stream, "brick id:\t%u\n",
			  get_sb_subvol_id(super));

	aal_stream_format(stream, "data room:\t%u\n",
			  get_sb_data_room(super));

	aal_stream_format(stream, "volinfo loc:\t%u\n",
			  get_sb_volinfo_loc(super));

	aal_stream_format(stream, "max bricks:\t%u\n",
			  get_sb_num_sgs_bits(super) ?
			  1 << get_sb_num_sgs_bits(super) : 0);
}

reiser4_format_ent_t *format41_regenerate(aal_device_t *device,
					  backup_hint_t *hint)
{
	return format40_regenerate_common(device, hint, &format41_plug);
}


extern errno_t format41_check_struct(reiser4_format_ent_t *entity,
				     backup_hint_t *hint,
				     format_hint_t *desc,
				     uint8_t mode)
{
	return format40_check_struct_common(entity, hint, desc, mode,
					    format41_core);
}

errno_t format41_check_backup(backup_hint_t *hint)
{
	return format40_check_backup_common(hint, format41_core);
}

#endif
#endif /* FORMAT41_REPAIR_H */

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
