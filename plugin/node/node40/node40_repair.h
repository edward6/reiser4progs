/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node40_repair.h -- reiser4 node plugin repair functions. */

#ifndef NODE40_REPAIR_H
#define NODE40_REPAIR_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

extern errno_t node40_pack(node_entity_t *entity,
			   aal_stream_t *stream);

extern errno_t node40_unpack(node_entity_t *entity,
			     aal_stream_t *stream);

extern void node40_set_flag(node_entity_t *entity, 
			    uint32_t pos, uint16_t flag);

extern void node40_clear_flag(node_entity_t *entity, 
			      uint32_t pos, uint16_t flag);

extern bool_t node40_test_flag(node_entity_t *entity, 
			       uint32_t pos, uint16_t flag);

extern errno_t node40_check_struct(node_entity_t *entity,
				   uint8_t mode);

extern errno_t node40_merge(node_entity_t *dst_entity, pos_t *dst_pos, 
			    node_entity_t *src_entity, pos_t *src_pos, 
			    merge_hint_t *hint);
#endif
