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

extern char *aux_strncat(char *dest, uint32_t n, const char *src, ...) 
__check_format__(printf, 3, 4);
    
#endif

typedef void *(*reiser4_elem_func_t) (void *, uint32_t, void *);
typedef int (*reiser4_comp_func_t) (void *, void *, void *);

extern int aux_binsearch(void *array, uint32_t count, 
			 void *needle, reiser4_elem_func_t elem_func, 
			 reiser4_comp_func_t comp_func, void *, uint64_t *pos);

#endif

