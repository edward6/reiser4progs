/*
  aux.h -- miscellaneous useful code.

  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AUX_H
#define AUX_H

#include <stdint.h>
#include <aal/aal.h>

#ifndef ENABLE_COMPACT

#include <stdlib.h>
#include <errno.h>

extern long int aux_strtol(const char *str, int *error);

#endif

typedef int (*aux_comp_func_t) (void *, uint32_t, void *, void *);

typedef errno_t (*aux_pre_parse_t) (char *, char *, void *);
typedef errno_t (*aux_post_parse_t) (char *, char *, void *);

extern errno_t aux_parse_path(char *path, aux_pre_parse_t pre_func,
			      aux_post_parse_t post_func, void *data);

extern int aux_bin_search(void *array, uint32_t count, void *needle,
			 aux_comp_func_t comp_func, void *, uint64_t *pos);

#endif

