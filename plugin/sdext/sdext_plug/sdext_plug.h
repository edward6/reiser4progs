/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_unix.h -- stat data exception plugin, that implements unix stat data
   fields. */

#ifndef SDEXT_PLUGID_H
#define SDEXT_PLUGID_H

#include <aal/libaal.h>
#include <reiser4/plugin.h>

/* stat-data extension for files with non-standard plugin. */
struct sdext_plug_slot {
        d16_t member;
        d16_t plug;
} __attribute__((packed));

typedef struct sdext_plug_slot sdext_plug_slot_t;

struct sdext_plug {
        d16_t count;
        sdext_plug_slot_t slot[0];
}  __attribute__((packed));

typedef struct sdext_plug sdext_plug_t;

extern reiser4_core_t *sdext_plug_core;

#define sdext_plug_get_count(ext)		aal_get_le16((ext), count)
#define sdext_plug_set_count(ext, val)		aal_set_le16((ext), count, (val))

#define sdext_plug_get_member(ext, n)		aal_get_le16(&((ext)->slot[n]), member)
#define sdext_plug_set_member(ext, n, val)	aal_set_le16(&((ext)->slot[n]), member, (val))

#define sdext_plug_get_pid(ext, n)		aal_get_le16(&((ext)->slot[n]), plug)
#define sdext_plug_set_pid(ext, n, val)		aal_set_le16(&((ext)->slot[n]), plug, (val))

extern uint32_t sdext_plug_length(stat_entity_t *stat, void *hint);

#endif
