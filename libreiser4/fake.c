/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   fake.c -- functions for working with fake block numbers. */

#ifndef ENABLE_STAND_ALONE
#include <reiser4/reiser4.h>

static blk_t fake_gen = 0;

inline blk_t reiser4_fake_get(void) {
	return (++fake_gen | REISER4_FAKE_BLOCKNR_VAL);
}

inline int reiser4_fake_ack(blk_t blk) {
	return (blk & REISER4_FAKE_BLOCKNR_MASK) ? 1 : 0;
}
#endif
