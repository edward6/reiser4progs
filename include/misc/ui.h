/*
  ui.h -- common for all progs function for work with libreadline.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef PROGS_UI_H
#define PROGS_UI_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <aal/aal.h>

extern uint16_t progs_screen_width(void);

extern char *progs_readline(char *prompt, void *stream);
extern void progs_print_wrap(void *stream, char *text);
extern void progs_wipe_line(void *stream);

#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
extern void progs_set_variant(aal_list_t *list);
extern aal_list_t *progs_get_variant(void);
#endif

extern void progs_print_banner(char *name);

#endif

