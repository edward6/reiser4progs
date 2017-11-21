/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   fake.h -- functions for working with fake block numbers. */

#ifndef REISER4_FAKE_H
#define REISER4_FAKE_H

#ifndef ENABLE_MINIMAL
#include <reiser4/types.h>

#define REISER4_FAKE_BLOCKNR_VAL   0xf000000000000000ull
#define REISER4_FAKE_BLOCKNR_MASK  0x8000000000000000ull

extern blk_t reiser4_fake_get(void);
extern int reiser4_fake_ack(blk_t blk);

#endif

#endif
