/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40_repair.h -- reiser4 node plugin repair functions. */

#ifndef NODE40_REPAIR_H
#define NODE40_REPAIR_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

extern errno_t node40_insert_raw(reiser4_node_t *entity, pos_t *pos,
				 trans_hint_t *hint);

extern errno_t node40_check_struct(reiser4_node_t *entity,
				   uint8_t mode);

extern errno_t node40_pack(reiser4_node_t *entity,
			   aal_stream_t *stream);

extern reiser4_node_t *node40_unpack(aal_block_t *block,
				    reiser4_key_plug_t *kplug,
				    aal_stream_t *stream);

extern void node40_print(reiser4_node_t *entity, aal_stream_t *stream,
			 uint32_t start, uint32_t count, uint16_t options);

#endif
