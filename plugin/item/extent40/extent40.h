/*
  extent40 -- resier4 default extent plugin.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef EXTENT40
#define EXTENT40

#include <aal/aal.h>
#include <reiser4/plugin.h>
#include <plugin/item/body40/body40.h>

struct extent40 {
	blk_t start;
	count_t width;
};

typedef struct extent40 extent40_t;

#define extent40_body(item)	    ((extent40_t *)item->body)

#define et40_get_start(et)	    aal_get_le64((et), start)
#define et40_set_start(et, val)	    aal_set_le64((et), start, val)

#define et40_get_width(et)	    aal_get_le64((et), width)
#define et40_set_width(et, val)	    aal_set_le64((et), width, val)

#endif
