/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   format41.h -- reiser4 disk-format plugin. */

#ifndef FORMAT41_H
#define FORMAT41_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>
#include <../format40/format40.h>

extern reiser4_format_plug_t format41_plug;
extern reiser4_core_t *format41_core;



#define get_sb_fiber_len(sb)		aal_get_le64(sb, sb_fiber_len)
#define set_sb_fiber_len(sb, val)	aal_set_le64(sb, sb_fiber_len, val)

#define get_sb_fiber_loc(sb)		aal_get_le64(sb, sb_fiber_loc)
#define set_sb_fiber_loc(sb, val)	aal_set_le64(sb, sb_fiber_loc, val)

#define get_sb_subvol_id(sb)		aal_get_le64(sb, sb_subvol_id)
#define set_sb_subvol_id(sb, val)	aal_set_le64(sb, sb_subvol_id, val)

#define get_sb_num_subvols(sb)		aal_get_le64(sb, sb_num_subvols)
#define set_sb_num_subvols(sb, val)	aal_set_le64(sb, sb_num_subvols, val)

#ifndef ENABLE_MINIMAL

extern reiser4_format_ent_t *format41_unpack(aal_device_t *device,
					     uint32_t blksize,
					     aal_stream_t *stream);
extern void format41_print(reiser4_format_ent_t *entity,
			   aal_stream_t *stream, uint16_t options);
extern reiser4_format_ent_t *format41_regenerate(aal_device_t *device,
						 backup_hint_t *hint);
extern errno_t format41_check_struct(reiser4_format_ent_t *entity,
				     backup_hint_t *hint,
				     format_hint_t *desc,
				     uint8_t mode);
errno_t format41_check_backup(backup_hint_t *hint);

#endif
#endif

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
