/*
    ui.h -- common for all progs function for work with libreadline.
    Copyright 1996-2002 (C) Hans Reiser.
*/

#ifndef PROGS_UI_H
#define PROGS_UI_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

extern char *progs_ui_readline(char *prompt);
extern uint16_t progs_ui_screen_width(void);
extern void progs_ui_print_wrap(void *stream, char *text);
extern void progs_ui_wipe_line(void *stream);

#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
extern void progs_ui_set_possibilities(aal_list_t *list);
extern aal_list_t *progs_ui_get_possibilities(void);
#endif

extern void progs_misc_print_banner(char *name);

#endif

