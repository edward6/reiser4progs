/*
  format.h -- format's functions.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef FORMAT_H
#define FORMAT_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <aal/aal.h>
#include <reiser4/plugin.h>
#include <reiser4/filesystem.h>

extern reiser4_format_t *reiser4_format_reopen(reiser4_format_t *format);
extern reiser4_format_t *reiser4_format_open(reiser4_fs_t *fs);

#ifndef ENABLE_COMPACT

extern errno_t reiser4_format_sync(reiser4_format_t *format);

extern reiser4_format_t *reiser4_format_create(reiser4_fs_t *fs, count_t len,
					       uint16_t tail, rpid_t pid);

extern void reiser4_format_set_root(reiser4_format_t *format, 
				    blk_t root);

extern void reiser4_format_set_len(reiser4_format_t *format, 
				   count_t blocks);

extern void reiser4_format_set_free(reiser4_format_t *format, 
				    count_t blocks);

extern void reiser4_format_set_height(reiser4_format_t *format, 
				      uint8_t height);

extern void reiser4_format_set_make_stamp(reiser4_format_t *format, 
					  uint32_t stamp);

extern errno_t reiser4_format_mark(reiser4_format_t *format, 
				   reiser4_alloc_t *alloc);

extern errno_t reiser4_format_print(reiser4_format_t *format,
				    aal_stream_t *stream);

#endif

extern blk_t reiser4_format_start(reiser4_format_t *format);
extern errno_t reiser4_format_valid(reiser4_format_t *format);
extern void reiser4_format_close(reiser4_format_t *format);
extern int reiser4_format_confirm(reiser4_format_t *format);

extern blk_t reiser4_format_get_root(reiser4_format_t *format);
extern count_t reiser4_format_get_len(reiser4_format_t *format);
extern count_t reiser4_format_get_free(reiser4_format_t *format);
extern uint16_t reiser4_format_get_height(reiser4_format_t *format);
extern uint32_t reiser4_format_get_make_stamp(reiser4_format_t *format);

extern const char *reiser4_format_name(reiser4_format_t *format);
extern rpid_t reiser4_format_journal_pid(reiser4_format_t *format);
extern rpid_t reiser4_format_alloc_pid(reiser4_format_t *format);
extern rpid_t reiser4_format_oid_pid(reiser4_format_t *format);

extern errno_t reiser4_format_skipped(reiser4_format_t *format, 
				      block_func_t func,
				      void *data);

extern errno_t reiser4_format_layout(reiser4_format_t *format, 
				     block_func_t func,
				     void *data);

#endif

