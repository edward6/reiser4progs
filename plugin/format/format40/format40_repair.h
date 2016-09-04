/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format40_repair.h -- reiser4 disk-format plugin repair functions. */

#ifndef FORMAT40_REPAIR_H
#define FORMAT40_REPAIR_H

#ifndef ENABLE_MINIMAL

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t format40_update(reiser4_format_ent_t *entity);
extern errno_t format40_pack(reiser4_format_ent_t *entity,
			     aal_stream_t *stream);
extern reiser4_format_ent_t *format40_unpack_common(aal_device_t *device,
						   uint32_t blksize,
						   aal_stream_t *stream,
						   reiser4_format_plug_t *plug);
extern reiser4_format_ent_t *format40_unpack(aal_device_t *device,
					     uint32_t blksize,
					     aal_stream_t *stream);
extern void format40_print_common(reiser4_format_ent_t *entity,
				  aal_stream_t *stream,
				  uint16_t options, reiser4_core_t *core);
extern void format40_print(reiser4_format_ent_t *entity,
			   aal_stream_t *stream,
			   uint16_t options);
extern errno_t format40_check_backup_common(backup_hint_t *hint,
					    reiser4_core_t *core);
extern errno_t format40_check_backup(backup_hint_t *hint);
extern reiser4_format_ent_t *format40_regenerate_common(aal_device_t *device,
						   backup_hint_t *hint,
						   reiser4_format_plug_t *plug);
extern reiser4_format_ent_t *format40_regenerate(aal_device_t *device, 
						 backup_hint_t *hint);
extern errno_t format40_check_struct_common(reiser4_format_ent_t *entity,
					    backup_hint_t *hint,
					    format_hint_t *desc,
					    uint8_t mode, reiser4_core_t *core,
					    errno_t (*is_mirror)(format40_super_t *s));
extern errno_t format40_check_struct(reiser4_format_ent_t *entity,
				     backup_hint_t *hint,
				     format_hint_t *desc,
				     uint8_t mode);

#endif
#endif
