/*
  malloc.h -- memory allocation functions. ctualy they are memory allocation
  handlers which may be set by caller. In the allone mode they will point to
  corresponding memory allocation functions, which are used in certain alone
  application (bootloaders, etc). In the standard mode they are pointed to libc
  memory allocation functions (malloc, free, etc).
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_MALLOC_H
#define AAL_MALLOC_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/types.h>

#ifndef ENABLE_MEMORY_MANAGER

extern void aal_mem_fini(void);

extern void aal_mem_init(void *start,
			 unsigned int len);

extern unsigned int aal_mem_free(void);

#endif

typedef void *(*aal_malloc_handler_t) (unsigned int);
typedef void *(*aal_realloc_handler_t) (void *, unsigned int);
typedef void (*aal_free_handler_t) (void *);

extern void aal_malloc_set_handler(aal_malloc_handler_t handler);
extern aal_malloc_handler_t aal_malloc_get_handler(void);

extern void *aal_malloc(unsigned int size);
extern void *aal_calloc(unsigned int size, char c);

extern void aal_realloc_set_handler(aal_realloc_handler_t handler);
extern aal_realloc_handler_t aal_realloc_get_handler(void);
extern int aal_realloc(void** old, unsigned int size);

extern void aal_free_set_handler(aal_free_handler_t handler);
extern aal_free_handler_t aal_free_get_handler(void);
extern void aal_free(void *ptr);

#endif

