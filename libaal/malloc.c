/*
  malloc.c -- hanlders for memory allocation functions.
    
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#include <aal/aal.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/*
  Checking whether allone mode is in use. If so, initializes memory working
  handlers as NULL, because application that is use libreiser4 and libaal must
  set it up.
*/
#ifndef ENABLE_COMPACT

#include <sys/vfs.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

static aal_malloc_handler_t malloc_handler = malloc;
static aal_realloc_handler_t realloc_handler = realloc;
static aal_free_handler_t free_handler = free;

#else

static aal_malloc_handler_t malloc_handler = NULL;
static aal_realloc_handler_t realloc_handler = NULL;
static aal_free_handler_t free_handler = NULL;

#endif

struct mpressure {
	void *data;
	int enabled;
	char name[256];
	
	aal_mpressure_handler_t handler;
};

static int mpressure_active = 0;
static aal_list_t *mpressure_handler = NULL;
static aal_mpressure_detect_t mpressure_detect = NULL;

#ifndef ENABLE_COMPACT

static int check_mpressure = 0;

static void callback_check_mpressure(int dummy) {
	check_mpressure = 1;
	alarm(1);
}

#endif

errno_t aal_mpressure_check(void) {
	int result;
	aal_list_t *walk;
	struct mpressure *mpressure;
	
	if (check_mpressure) {
		check_mpressure = 0;
		result = mpressure_detect();

		if (mpressure_handler != NULL) {
			aal_list_foreach_forward(walk, mpressure_handler) {
				mpressure = (struct mpressure *)walk->data;

				if (!mpressure->enabled)
					continue;
					
				if (mpressure->handler(mpressure->data, result)) {
					aal_exception_warn("Memory pressure handler "
							   "\"%s\" failed.", mpressure->name);
				}
			}
		}
	}

	return 0;
}

void aal_mpressure_enable(void *handler) {
	((struct mpressure *)handler)->enabled = 1;
}

void aal_mpressure_disable(void *handler) {
	((struct mpressure *)handler)->enabled = 0;
}

int aal_mpressure_active(void) {
	return mpressure_active;
}

int aal_mpressure_detect(void) {
	return mpressure_detect && mpressure_detect();
}

errno_t aal_mpressure_init(aal_mpressure_detect_t handler) {
#ifndef ENABLE_COMPACT
	struct statfs fs_st;
	struct sigaction new, old;
#endif

	if (!(mpressure_detect = handler))
		return -1;

#ifndef ENABLE_COMPACT
	mpressure_detect();
	
	mpressure_active = statfs("/proc", &fs_st) != -1 &&
		fs_st.f_type == 0x9fa0;

	if (mpressure_active) {
		new.sa_flags = 0;
		new.sa_handler = callback_check_mpressure;

		sigaction(SIGALRM, &new, &old);
		alarm(1);
	}

	return 0;
#else
	mpressure_active = 0;
	return -1;
#endif
}

void *aal_mpressure_handler_create(aal_mpressure_handler_t handler,
				   char *name, void *data)
{
	struct mpressure *mpressure;
	
	aal_assert("umka-1557", handler != NULL, return NULL);
	aal_assert("umka-1558", name != NULL, return NULL );

	if (!(mpressure = aal_calloc(sizeof(*mpressure), 0)))
		return NULL;

	mpressure->handler = handler;
	mpressure->data = data;
	mpressure->enabled = 1;

	aal_strncpy(mpressure->name, name,
		    sizeof(mpressure->name));

	mpressure_handler = aal_list_append(mpressure_handler, mpressure);

	return mpressure;
}

void aal_mpressure_handler_free(void *handler) {
	aal_assert("umka-1559", handler != NULL, return);
	
	mpressure_handler = aal_list_remove(mpressure_handler, handler);
	aal_free(handler);
}

/* 
   Sets new handler for malloc function. This is useful for alone mode, because
   all application which are working in alone mode (without libc, probably in
   real mode of processor, etc) have own memory allocation factory. That factory
   usualy operates on some static memory heap. And all allocation function just
   mark some piece of heap as used. And all deallocation function marks
   corresponding piece as unused.
*/
void aal_malloc_set_handler(
	aal_malloc_handler_t handler)  /* new handler to be set */
{
	malloc_handler = handler;
}

/* Returns allocation handler */
aal_malloc_handler_t aal_malloc_get_handler(void) {
	return malloc_handler;
}

/*
  The wrapper for malloc function. It checks for result memory allocation and if
  it failed then reports about this.
*/
void *aal_malloc(
	size_t size)		    /* size of memory piece to be allocated */
{
	void *mem;

	/* 
	   We are using simple printf function instead of exception, because
	   exception initialization is needed correctly worked memory allocation
	   handler.
	*/
	if (!malloc_handler)
		return NULL;

	if (!(mem = malloc_handler(size)))
		return NULL;

	return mem;
}

/* Allocates memory piese and fills it by specified byte */
void *aal_calloc(
	size_t size,		    /* size of memory piece to be allocated */
	char c)
{
	void *mem;

	if (!(mem = aal_malloc(size)))
		return NULL;

	aal_memset(mem, c, size);
	return mem;
}

/* 
   Sets new handler for "realloc" operation. The same as in malloc case. See
   above for details.
*/
void aal_realloc_set_handler(
	aal_realloc_handler_t handler)   /* new handler for realloc */
{
	realloc_handler = handler;
}

/* Returns realloc handler */
aal_realloc_handler_t aal_realloc_get_handler(void) {
	return realloc_handler;
}

/*
  The wrapper for realloc function. It checks for result memory allocation and
  if it failed then reports about this.
*/
errno_t aal_realloc(
	void **old,		    /* pointer to previously allocated piece */
	size_t size)		    /* new size */
{
	void *mem;

	if (!realloc_handler)
		return -1;

	if (!(mem = (void *)realloc_handler(*old, size)))
		return -1;
    
	*old = mem;
	return 0;
}

/* Sets new handle for "free" operation */
void aal_free_set_handler(
	aal_free_handler_t handler)    /* new "free" operation handler */
{
	free_handler = handler;
}

/* Returns hanlder for "free" opetration */
aal_free_handler_t aal_free_get_handler(void) {
	return free_handler;
}

/*
  The wrapper for free function. It checks for passed memory pointer and if it
  is invalid then reports about this.
*/
void aal_free(
	void *ptr)		    /* pointer onto memory to be released */
{
	if (!free_handler)
		return;

	free_handler(ptr);
}
