/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40_repair.h -- reiser4 node plugin repair functions. */

#ifndef NODE40_REPAIR_H
#define NODE40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t node40_pack(reiser4_node_t *entity,
			   aal_stream_t *stream,
			   int mode);

extern reiser4_node_t *node40_unpack(aal_block_t *block,
				    reiser4_plug_t *kplug,
				    aal_stream_t *stream,
				    int mode);

extern void node40_set_flag(reiser4_node_t *entity, 
			    uint32_t pos, uint16_t flag);

extern void node40_clear_flag(reiser4_node_t *entity, 
			      uint32_t pos, uint16_t flag);

extern bool_t node40_test_flag(reiser4_node_t *entity, 
			       uint32_t pos, uint16_t flag);

extern errno_t node40_check_struct(reiser4_node_t *entity,
				   uint8_t mode);

extern errno_t node40_merge(reiser4_node_t *entity, pos_t *pos,
			    trans_hint_t *hint);

extern void node40_print(reiser4_node_t *entity, aal_stream_t *stream,
			    uint32_t start, uint32_t count, uint16_t options);

#endif
