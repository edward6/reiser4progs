/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   node_large.h -- reiser4 node with large keys. */

#ifndef NODE_LARGE_H
#define NODE_LARGE_H

#include <aal/aal.h>
#include <aux/aux.h>
#include <reiser4/plugin.h>
#include <plugin/node/node_common/node_common.h>

union key {
	d64_t el[4];
	int pad;
};

typedef union key key_t;

struct item_header {
	key_t key;
    
	d16_t offset;
	d16_t flags;
	d16_t pid;
};

typedef struct item_header item_header_t;

#define ih_get_offset(ih)	        \
        aal_get_le16(ih, offset)

#define ih_set_offset(ih, val)          \
        aal_set_le16(ih, offset, val)

#define ih_inc_offset(ih, val)          \
        ih_set_offset(ih, (ih_get_offset(ih) + (val)))

#define ih_dec_offset(ih, val)          \
        ih_set_offset(ih, (ih_get_offset(ih) - (val)))

#define ih_get_flags(ih)               \
        aal_get_le16(ih, flags)

#define ih_set_flags(ih, val)          \
        aal_set_le16(ih, flags, val)

#define ih_clear_flag(ih, flag)         \
        aal_clear_bit(ih->flags, flag)

#define ih_test_flag(ih, flag)          \
        aal_test_bit(ih->flags, flag)

#define ih_set_flag(ih, flag)	        \
        aal_set_bit(ih->flags, flag)

#define ih_get_pid(ih)	                \
        aal_get_le16(ih, pid)

#define ih_set_pid(ih, val)	        \
        aal_set_le16(ih, pid, (val))

extern item_header_t *node_large_ih_at(node_t *node,
				       uint32_t pos);

extern uint16_t node_large_free_space_end(node_t *node);
extern void *node_large_ib_at(node_t *node, uint32_t pos);
#endif
