/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
 * reiser4progs/COPYING.
 */

#include "../node40/node40.h"
#include "../node40/node40_repair.h"
#include "node41.h"
#include "node41_repair.h"
#include <aux/crc32c.h>

uint32_t node41_get_csum(reiser4_node_t *entity) {
	aal_assert("edward-3", entity != NULL);
	return nh41_get_csum((reiser4_node_t *)entity);
}

uint32_t csum_node41(reiser4_node_t *entity, uint32_t check)
{
	uint32_t calc;

	calc = reiser4_crc32c(~0,
			      entity->block->data,
			      sizeof(node40_header_t));
	calc = reiser4_crc32c(calc,
			      entity->block->data + sizeof(node41_header_t),
			      entity->block->size - sizeof(node41_header_t));
	if (check)
		return calc == nh41_get_csum(entity);
	else {
		nh41_set_csum(entity, calc);
		return 1;
	}
}

reiser4_node_t *node41_prepare(aal_block_t *block, reiser4_key_plug_t *kplug) {
	reiser4_node_t *entity;

	aal_assert("edward-4", kplug != NULL);
	aal_assert("edward-5", block != NULL);

	if (!(entity = aal_calloc(sizeof(*entity), 0)))
		return NULL;

	entity->kplug = kplug;
	entity->block = block;
	entity->plug = &node41_plug;
	entity->keypol = plugcall(kplug, bodysize);

	return entity;
}

/* Open the node on the given @block with the given key plugin @kplug. Returns
   initialized node instance. */
reiser4_node_t *node41_open(aal_block_t *block, reiser4_key_plug_t *kplug)
{
	reiser4_node_t *entity;

	aal_assert("edward-6", kplug != NULL);
	aal_assert("edward-7", block != NULL);

	if (!(entity = node41_prepare(block, kplug)))
		return NULL;

	/* Verify checksum */
	if (csum_node41(entity, 1 /* check */) == 0) {
		aal_free(entity);
		return NULL;
	}
	/* Check the magic. */
	if (nh_get_magic(entity) != NODE41_MAGIC) {
		aal_free(entity);
		return NULL;
	}

	return entity;
}

errno_t node41_sync(reiser4_node_t *entity) {
	csum_node41(entity, 0 /* update */);
	return node40_sync(entity);
}

#ifndef ENABLE_MINIMAL

/* Returns maximal size of item possible for passed node instance */
static uint16_t node41_maxspace(reiser4_node_t *entity) {
	aal_assert("edward-8", entity != NULL);
	/*
	 * Maximal space is node size minus node header and minus item
	 * header
	 */
	return (entity->block->size - sizeof(node41_header_t) -
		ih_size(entity->keypol));
}

/*
 * Initializes node of the given @level on the @block with key plugin
 * @kplug. Returns initialized node instance
 */
static reiser4_node_t *node41_init(aal_block_t *block, uint8_t level,
				   reiser4_key_plug_t *kplug)
{
	return node40_init_common(block, level, kplug,
				  &node41_plug,
				  NODE41_MAGIC,
				  sizeof(node41_header_t),
				  node41_prepare);
}

#endif

reiser4_node_plug_t node41_plug = {
	.p = {
		.id    = {NODE_REISER41_ID, 0, NODE_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "node41",
		.desc  = "Protected node layout plugin.",
#endif
	},

	.open		= node41_open,
	.fini		= node40_fini,
	.lookup		= node40_lookup,
	.fetch          = node40_fetch,
	.items		= node40_items,

	.get_key	= node40_get_key,
	.get_level	= node40_get_level,

#ifndef ENABLE_MINIMAL
	.init		= node41_init,
	.sync           = node41_sync,
	.merge          = node40_merge,

	.pack           = node41_pack,
	.unpack         = node41_unpack,

	.insert		= node40_insert,
	.write		= node40_write,
	.trunc          = node40_trunc,
	.remove		= node40_remove,
	.print		= node41_print,
	.shift		= node40_shift,
	.shrink		= node40_shrink,
	.expand		= node40_expand,
	.insert_raw     = node40_insert_raw,
	.copy           = node40_copy,

	.overhead	= node40_overhead,
	.maxspace	= node41_maxspace,
	.space		= node40_space,

	.set_key	= node40_set_key,
	.set_level      = node40_set_level,

	.get_mstamp	= node40_get_mstamp,
	.get_fstamp     = node40_get_fstamp,

	.set_mstamp	= node40_set_mstamp,
	.set_fstamp     = node40_set_fstamp,

	.set_flags	= node40_set_flags,
	.get_flags	= node40_get_flags,

	.set_state      = node40_set_state,
	.get_state      = node40_get_state,
	.check_struct	= node41_check_struct
#endif
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
