/*
  types.h -- libaal types declaration.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_TYPES_H
#define AAL_TYPES_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#if !defined(__GNUC__) && (defined(__sparc__) || defined(__sparcv9))
#  include <sys/int_types.h>
#else
#  include <stdint.h>
#endif

/*
  As libaal may be used without any standard headers, we need to declare NULL
  macro here in order to avoid compilation errors.
*/
#undef NULL

#if defined(__cplusplus)
#  define NULL 0
#else
#  define NULL ((void *)0)
#endif

/*
  Here we define FALSE and TRUE macros in order to make sources more clean for
  understanding. I mean, that there where we need some boolean value, we will
  use these two macro.
*/
#if !defined(FALSE)
#  define FALSE 0
#endif

#if !defined(TRUE)
#  define TRUE 1
#endif

/* 
  Macro for checking the format string in situations like this:

  aal_exception_throw(EXCEPTION_ERROR, EXCEPTION_OK, "Operation %d failed.",
                      "open");

  As aal_exception_throw is declared with this macro, compiller in the comile
  time will make warning about incorrect format string.
*/
#ifdef __GNUC__
#  define __aal_check_format__(style, format, start) \
       __attribute__((__format__(style, format, start)))
#else
#  define __aal_check_format__(style, format, start)
#endif

/* Simple type for direction denoting */
enum aal_direction {
	D_TOP    = 1 << 0,
	D_BOTTOM = 1 << 1,
	D_LEFT   = 1 << 2,
	D_RIGHT  = 1 << 3
};

typedef enum aal_direction aal_direction_t;

typedef int bool_t;

/* 
  This type is used for return of result of execution some function.
    
  Success - 0 (not errors),
  Failure - negative error code
*/
typedef int errno_t;

typedef struct aal_list aal_list_t;

/* 
   This is the struct that describes one list element. It contains: pointer to
   data assosiated with this item of list, pointer to next element of list and
   pointer to prev element of list.
*/
struct aal_list {
	void *data;
    
	aal_list_t *next;
	aal_list_t *prev;
};

/*
  Type for callback compare function. It is used in list functions and in 
  other places.
*/
typedef int (*comp_func_t) (const void *, const void *, void *);

/* 
  Type for callback function that is called for each element of list. Usage is 
  the same as previous one.
*/
typedef int (*foreach_func_t) (const void *, const void *);

struct lru_ops {
	int (*free) (void *);
	int (*sync) (void *);

	aal_list_t *(*get_next) (void *);
	void (*set_next) (void *, aal_list_t *);
	
	aal_list_t *(*get_prev) (void *);
	void (*set_prev) (void *, aal_list_t *);
};

typedef struct lru_ops lru_ops_t;

struct aal_lru {
	uint32_t adjust;
	aal_list_t *list;
	lru_ops_t *ops;
};

typedef struct aal_lru aal_lru_t;

/* 
   This types is used for keeping the block number and block count value. They
   are needed to be increase source code maintainability.

   For instance, there is some function:

   blk_t some_func(void);
    
   It is clear to any reader, that this function is working with block number, 
   it returns block number.

   Yet another variant of this function:

   uint64_t some_func(void);
    
   This function may return anything. This is may be bytes, blocks, etc.
*/
#define INVAL_BLK (~0ull)

typedef uint64_t blk_t;
typedef uint64_t count_t;

struct aal_device_ops;

/*
  Abstract device structure. It consists of flags device opened with, user
  specified data, some opaque entity (for standard file it is file descriptor),
  name of device (for instance, /dev/hda2), block size of device and device
  operations.
*/
struct aal_device {
	int flags;

	void *data;
	void *entity;
	void *personality;

	uint32_t blocksize;
	char name[256], error[256];
	struct aal_device_ops *ops;
};

typedef struct aal_device aal_device_t;

/* 
   Operations which may be performed on the device. Some of them may not
   be implemented.
*/
struct aal_device_ops {
	errno_t (*open) (aal_device_t *, void *,
			 uint32_t, int);
	
	errno_t (*read) (aal_device_t *, 
			 void *, blk_t, count_t);
    
	errno_t (*write) (aal_device_t *, 
			 void *, blk_t, count_t);
    
	errno_t (*sync) (aal_device_t *);
    
	errno_t (*equals) (aal_device_t *, 
			   aal_device_t *);
    
	count_t (*len) (aal_device_t *);
	void (*close) (aal_device_t *);
};

/*
  Disk block structure. It is a replica of struct buffer_head from the linux
  kernel. It consists of flags (dirty, clean, etc), data (pointer to data of
  block), block size, offset (offset in bytes where block is placed on device),
  and pointer to device, block opened on.
*/
struct aal_block {
	int flags;
	void *data;

	blk_t blk;
	aal_device_t *device;
};

typedef struct aal_block aal_block_t;

/* This is the type of exception */
enum aal_exception_type {
	EXCEPTION_INFORMATION   = 1,
	EXCEPTION_WARNING       = 2,
	EXCEPTION_ERROR	        = 3,
	EXCEPTION_FATAL	        = 4,
	EXCEPTION_BUG	        = 5
};

typedef enum aal_exception_type aal_exception_type_t;

enum aal_exception_option {
	EXCEPTION_UNHANDLED     = 1 << 0,
	EXCEPTION_YES	        = 1 << 1,
	EXCEPTION_NO	        = 1 << 2,
	EXCEPTION_OK	        = 1 << 3,
	EXCEPTION_RETRY	        = 1 << 4,
	EXCEPTION_IGNORE        = 1 << 5,
	EXCEPTION_CANCEL        = 1 << 6,
	EXCEPTION_LAST
};

typedef enum aal_exception_option aal_exception_option_t;

#define EXCEPTION_YESNO		(EXCEPTION_YES | EXCEPTION_NO)
#define EXCEPTION_OKCANCEL	(EXCEPTION_OK | EXCEPTION_CANCEL)
#define EXCEPTION_RETRYIGNORE	(EXCEPTION_RETRY | EXCEPTION_IGNORE)

/* 
   This is exception structure. It contains: exception message, exception type,
   exception options. Usualy, the life cycle of exception is very
   short. Exception instance created by aal_exception_throw function and passed
   t exception handler. After exception processed, it is destroyed by exception
   factory.
*/
struct aal_exception {
	char *message;
	aal_exception_type_t type;
	aal_exception_option_t options;
};

typedef struct aal_exception aal_exception_t;

typedef aal_exception_option_t (*aal_exception_handler_t) (aal_exception_t *ex);

typedef struct aal_gauge aal_gauge_t;

enum aal_gauge_type {
	GAUGE_PERCENTAGE,
	GAUGE_INDICATOR,
	GAUGE_SILENT
};

typedef enum aal_gauge_type aal_gauge_type_t;

enum aal_gauge_state {
	GAUGE_STARTED,
	GAUGE_RUNNING,
	GAUGE_PAUSED,
	GAUGE_DONE,
};

typedef enum aal_gauge_state aal_gauge_state_t;

typedef void (*aal_gauge_handler_t)(aal_gauge_t *);

struct aal_gauge {
	aal_gauge_type_t type;
	aal_gauge_state_t state;
	aal_gauge_handler_t handler;

	void *data;
    
	char name[256];
	uint32_t value;
};

struct aal_stream {
	int size;
	int offset;
	void *data;
};

typedef struct aal_stream aal_stream_t;

typedef void (*assert_handler_t) (char *, int, char *, char *, int, char *);

#endif
