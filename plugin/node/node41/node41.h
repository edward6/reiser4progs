/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   node41.h -- reiser4 node41 layouts.
   The same as node40, but with 32-bit reference counter
*/

#ifndef NODE41_H
#define NODE41_H

#include <aal/libaal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>

#define NODE41_MAGIC 0x19051966

extern reiser4_node_plug_t node41_plug;

typedef struct node41_header {
	node40_header_t head;
	d32_t csum;
}  __attribute__((packed)) node41_header_t;

#define	nh41(block) ((node41_header_t *)block->data)

#define nh41_get_csum(node)			\
	aal_get_le32(nh41((node)->block), csum)

#define nh41_set_csum(node, val)			\
	aal_set_le32(nh41((node)->block), csum, val)


extern uint32_t node41_get_csum(reiser4_node_t *entity);
extern reiser4_node_t *node41_prepare(aal_block_t *block,
				      reiser4_key_plug_t *kplug);

#endif
/*
   Local variables:
   c-indentation-style: "K&R"
   mode-name: "LC"
   c-basic-offset: 8
   tab-width: 8
   fill-column: 120
   scroll-step: 1
   End:
*/
