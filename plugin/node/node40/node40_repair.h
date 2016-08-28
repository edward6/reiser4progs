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
extern void node40_header_pack(reiser4_node_t *entity, aal_stream_t *stream);
extern int32_t node40_items_pack(reiser4_node_t *entity, aal_stream_t *stream);
extern errno_t node40_pack_common(reiser4_node_t *entity, aal_stream_t *stream,
				  void (*pack_header_fn)(reiser4_node_t *,
							 aal_stream_t *),
				  int32_t (*pack_items_fn)(reiser4_node_t *,
							   aal_stream_t *));
extern errno_t node40_pack(reiser4_node_t *entity,
			   aal_stream_t *stream);
extern int32_t node40_header_unpack(reiser4_node_t *entity,
				    aal_stream_t *stream);
extern int32_t node40_items_unpack(reiser4_node_t *entity,
				   aal_stream_t *stream);
extern reiser4_node_t *
node40_unpack_common(aal_block_t *block,
		     reiser4_key_plug_t *kplug,
		     aal_stream_t *stream,
		     reiser4_node_plug_t *nplug,
		     const uint32_t magic,
		     reiser4_node_t* (*prepare_fn)(aal_block_t *,
						  reiser4_key_plug_t *),
		     int32_t (*unpack_header_fn)(reiser4_node_t *,
						 aal_stream_t *),
		     int32_t (*unpack_items_fn)(reiser4_node_t *,
						aal_stream_t *));
extern reiser4_node_t *node40_unpack(aal_block_t *block,
				    reiser4_key_plug_t *kplug,
				    aal_stream_t *stream);

extern void node40_print_common(reiser4_node_t *entity, aal_stream_t *stream,
				uint32_t start, uint32_t count,
				uint16_t options,
				void (*print_header_fn)(reiser4_node_t *,
							aal_stream_t *));
extern void node40_print(reiser4_node_t *entity, aal_stream_t *stream,
			 uint32_t start, uint32_t count, uint16_t options);

extern uint32_t node40_estimate_count_common(reiser4_node_t *entity,
					     uint32_t nh_size);
extern errno_t node40_count_check_common(reiser4_node_t *node,
				  uint8_t mode,
				  uint32_t (*estimate_count)(reiser4_node_t *));
extern errno_t node40_ih_array_check_common(reiser4_node_t *node,
					    uint8_t mode,
					    uint32_t node_header_size);
#endif
