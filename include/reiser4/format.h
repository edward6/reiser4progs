/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   format.h -- public format functions. */

#ifndef REISER4_FORMAT_H
#define REISER4_FORMAT_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <reiser4/types.h>

extern reiser4_format_t *reiser4_format_open(reiser4_fs_t *fs);

#ifndef ENABLE_STAND_ALONE
extern int reiser4_format_confirm(reiser4_format_t *format);
extern errno_t reiser4_format_sync(reiser4_format_t *format);
extern errno_t reiser4_format_reopen(reiser4_format_t *format);

extern bool_t reiser4_format_isdirty(reiser4_format_t *format);
extern void reiser4_format_mkdirty(reiser4_format_t *format);
extern void reiser4_format_mkclean(reiser4_format_t *format);

extern reiser4_format_t *reiser4_format_create(reiser4_fs_t *fs,
					       count_t len,
					       uint16_t tail,
					       rid_t pid);

extern errno_t reiser4_format_skipped(reiser4_format_t *format, 
				      block_func_t func,
				      void *data);

extern errno_t reiser4_format_layout(reiser4_format_t *format, 
				     block_func_t func,
				     void *data);

extern void reiser4_format_set_root(reiser4_format_t *format, 
				    blk_t root);

extern void reiser4_format_set_len(reiser4_format_t *format, 
				   count_t blocks);

extern void reiser4_format_set_free(reiser4_format_t *format, 
				    count_t blocks);

extern void reiser4_format_set_height(reiser4_format_t *format, 
				      uint8_t height);

extern void reiser4_format_set_stamp(reiser4_format_t *format, 
				     uint32_t stamp);

extern void reiser4_format_set_policy(reiser4_format_t *format, 
				      uint16_t policy);

extern errno_t reiser4_format_mark(reiser4_format_t *format, 
				   reiser4_alloc_t *alloc);

extern errno_t reiser4_format_print(reiser4_format_t *format,
				    aal_stream_t *stream);

extern errno_t reiser4_format_valid(reiser4_format_t *format);
extern rid_t reiser4_format_alloc_pid(reiser4_format_t *format);
extern rid_t reiser4_format_journal_pid(reiser4_format_t *format);

extern blk_t reiser4_format_start(reiser4_format_t *format);
extern count_t reiser4_format_get_len(reiser4_format_t *format);
extern count_t reiser4_format_get_free(reiser4_format_t *format);
extern const char *reiser4_format_name(reiser4_format_t *format);
extern uint32_t reiser4_format_get_stamp(reiser4_format_t *format);
extern uint16_t reiser4_format_get_policy(reiser4_format_t *format);
extern rid_t reiser4_format_oid_pid(reiser4_format_t *format);
#endif

extern void reiser4_format_close(reiser4_format_t *format);
extern rid_t reiser4_format_key_pid(reiser4_format_t *format);
extern rid_t reiser4_format_node_pid(reiser4_format_t *format);
extern blk_t reiser4_format_get_root(reiser4_format_t *format);
extern uint16_t reiser4_format_get_height(reiser4_format_t *format);
#endif

