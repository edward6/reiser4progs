/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ui.h -- common for all progs function for work with libreadline. */

#ifndef MISC_UI_H
#define MISC_UI_H

#include <aal/libaal.h>

extern uint16_t misc_screen_width(void);
extern void misc_print_banner(char *str);
extern void misc_print_banner_noname(char *str);

extern void misc_wipe_line(void *stream);
extern void misc_print_wrap(void *stream, char *text);
extern char *misc_readline(char *prompt, void *stream);

#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
extern aal_list_t *misc_get_variant(void);
extern void misc_set_variant(aal_list_t *list);
#endif

#endif

