/*
    sdext_lw.h -- stat data plugin, that implements base stat data fields.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#ifndef SDEXT_LW_H
#define SDEXT_LW_H

#include <aal/aal.h>
#include <reiser4/plugin.h>

struct sdext_lw {
    d16_t mode;
    d32_t nlink;
    d64_t size;
};

typedef struct sdext_lw sdext_lw_t;

#define sdext_lw_get_mode(stat)		aal_get_le16(stat, mode)
#define sdext_lw_set_mode(stat, val)	aal_set_le16(stat, mode, val)

#define sdext_lw_get_nlink(stat)	aal_get_le32(stat, nlink)
#define sdext_lw_set_nlink(stat, val)	aal_set_le32(stat, nlink, val)

#define sdext_lw_get_size(stat)		aal_get_le64(stat, size)
#define sdext_lw_set_size(stat, val)	aal_set_le64(stat, size, val)

#endif

