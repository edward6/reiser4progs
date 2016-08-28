/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.

   node41_repair.c -- reiser4 node with short keys. */

#ifndef ENABLE_MINIMAL
#include "../node40/node40.h"
#include "../node40/node40_repair.h"
#include "node41.h"
#include <repair/plugin.h>

#define MIN_ITEM_LEN	1

/*
 * Look through ih array looking for the last valid item location. This will
 * be the last valid item.
 */
uint32_t node41_estimate_count(reiser4_node_t *node)
{
	return node40_estimate_count_common(node, sizeof(node41_header_t));
}

/*
 * Checks the count of items written in node_header. If it is wrong, it tries
 * to estimate it on the base of free_space fields and recover if REBUILD mode.
 * Returns FATAL otherwise.
 */
static errno_t node41_count_check(reiser4_node_t *node, uint8_t mode)
{
	return node40_count_check_common(node, mode, node41_estimate_count);
}

/*
 * Count of items is correct. Free space fields and item locations should be
 * checked/recovered if broken
 */
static errno_t node41_ih_array_check(reiser4_node_t *node, uint8_t mode)
{
	return node40_ih_array_check_common(node, mode,
					    sizeof(node41_header_t));
}

errno_t node41_check_struct(reiser4_node_t *node, uint8_t mode) {
	errno_t res;

	aal_assert("edward-17", node != NULL);

	/* Check the content of the node40 header. */
	if ((res = node41_count_check(node, mode)))
		return res;

	if (nh_get_num_items(node) == 0) {
		uint32_t offset = sizeof(node41_header_t);
		return node40_space_check(node, offset, mode);
	}

	/* Count looks ok. Recover the item array. */
	res = node41_ih_array_check(node, mode);

	if (repair_error_fatal(res))
		return res;

	res |= node40_iplug_check(node, mode);

	return res;
}

/*
 * Pack node41 header w/out magic and padding
 */
void node41_header_pack(reiser4_node_t *entity, aal_stream_t *stream)
{
	node41_header_t *head41;

	node40_header_pack(entity, stream);

	head41 = nh41(entity->block);
	aal_stream_write(stream, &head41->csum,
			 sizeof(head41->csum));
}

errno_t node41_pack(reiser4_node_t *entity, aal_stream_t *stream)
{
	return node40_pack_common(entity, stream,
				  node41_header_pack, node40_items_pack);
}

static int32_t node41_header_unpack(reiser4_node_t *entity,
				    aal_stream_t *stream)
{
	int32_t ret;
	uint32_t read;
	node41_header_t *head41;

	head41 = nh41(entity->block);
	ret = node40_header_unpack(entity, stream);
	if (ret)
		return ret;
	read = aal_stream_read(stream, &head41->csum,
			       sizeof(head41->csum));
	if (read != sizeof(head41->csum))
		return -1;
	return 0;
}

reiser4_node_t *node41_unpack(aal_block_t *block,
			      reiser4_key_plug_t *kplug,
			      aal_stream_t *stream)
{
	return node40_unpack_common(block, kplug, stream,
				    &node41_plug, NODE41_MAGIC,
				    node41_prepare,
				    node41_header_unpack,
				    node40_items_unpack);
}

static void node41_header_print(reiser4_node_t *entity, aal_stream_t *stream)
{
	uint8_t level;
	uint32_t csum;

	level = node40_get_level(entity);
	csum = node41_get_csum(entity);

	aal_stream_format(stream, "NODE (%llu) CSUM=%u LEVEL=%u ITEMS=%u "
			  "SPACE=%u MKFS ID=0x%x FLUSH=0x%llx\n",
			  entity->block->nr, csum, level,
			  node40_items(entity), node40_space(entity),
			  nh_get_mkfs_id(entity), nh_get_flush_id(entity));
}

/*
 * Prepare text node description and push it into specified @stream
 */
void node41_print(reiser4_node_t *entity, aal_stream_t *stream,
		  uint32_t start, uint32_t count, uint16_t options)
{
	return node40_print_common(entity, stream, start, count, options,
				   node41_header_print);
}

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
