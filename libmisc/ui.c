/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ui.c -- common for all progs function for work with libreadline. */

#include <pty.h>
#include <stdio.h>
#include <string.h>
#include <aal/libaal.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)

#ifndef HAVE_RL_COMPLETION_MATCHES
#  define OLD_READLINE (1)
#endif

#include <readline/readline.h>
#include <readline/history.h>

#ifndef HAVE_RL_COMPLETION_MATCHES
#  define rl_completion_matches completion_matches
#endif

#ifndef rl_compentry_func_t
#  define rl_compentry_func_t void
#endif

static aal_list_t *variant = NULL;

#endif /* defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H) */

#include <misc/misc.h>

extern aal_exception_option_t
misc_exception_handler(aal_exception_t *exception);

/* This function gets user enter */
char *misc_readline(
	char *prompt,       /* prompt to be printed */
	void *stream)	    /* stream to be used */
{
	char *line;
    
	aal_assert("umka-1021", prompt != NULL);
	aal_assert("umka-1536", stream != NULL);
    
#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
	rl_instream = stdin;
	rl_outstream = stream;
	
	if ((line = readline(prompt)) && aal_strlen(line)) {
		HIST_ENTRY *last_entry = current_history();
		if (!last_entry || aal_strncmp(last_entry->line, line, aal_strlen(line)))
			add_history(line);
	}
#else
	fprintf(stream, prompt);
    
	if (!(line = aal_calloc(256, 0)))
		return NULL;
    
	fgets(line, 256, stdin);
#endif
    
	if (line) {
		uint32_t len = aal_strlen(line);
		if (len) {
			if (line[len - 1] == '\n' || line[len - 1] == '\040')
				line[len - 1] = '\0';
		}
	}

	return line;
}

/* Gets screen width */
uint16_t misc_screen_width(void) {
	struct winsize winsize;
    
	if (ioctl(2, TIOCGWINSZ, &winsize))
		return 0;
    
	return winsize.ws_col == 0 ? 80 : winsize.ws_col;
}

void misc_wipe_line(void *stream) {
	int i, width = misc_screen_width();
    
	fprintf(stream, "\r");
	for (i = 0; i < width - 2; i++)
		fprintf(stream, " ");

	fprintf(stream, "\r");
}

/* Constructs exception message */
void misc_print_wrap(void *stream, char *text) {
	char *string, *word;
	uint32_t screen_width;
	uint32_t line_width;

	if (!stream || !text)
		return;
    
	if (!(screen_width = misc_screen_width()))
		screen_width = 80;

	for (line_width = 0; (string = aal_strsep(&text, "\n")); ) {
		for (; (word = aal_strsep(&string, " ")); ) {
			if (line_width + aal_strlen(word) > screen_width) {
				fprintf(stream, "\n");
				line_width = 0;
			}

			fprintf(stream, word);

			line_width += aal_strlen(word);

			if (line_width + 1 < screen_width) {
				fprintf(stream, " ");
				line_width++;
			}
		}

		fprintf(stream, "\n");
		line_width = 0;
	}
}

#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
static char *misc_generator(char *text, int state) {
	char *opt;
	char s[80], s1[80];
	static aal_list_t *cur = NULL;
    
	if (!state) 
		cur = variant;
    
	while (cur) {
		aal_memset(s, 0, sizeof(s));
		aal_memset(s1, 0, sizeof(s1));
	
		opt = (char *)cur->data;
		cur = cur->next;

		misc_upper_case(s, opt); 
		misc_upper_case(s1, text);

		if (!aal_strncmp(s, s1, aal_strlen(s1)))
			return aal_strndup(opt, aal_strlen(opt));
	}
    
	return NULL;
}

static char **misc_complete(char *text, int start, int end) {
	rl_compentry_func_t *gen =
		(rl_compentry_func_t *)misc_generator;
	
	return rl_completion_matches(text, gen);
}

void misc_set_variant(aal_list_t *list) {
	variant = list;
}

aal_list_t *misc_get_variant(void) {
	return variant;
}
#endif

/* Common alpha handler. Params are the same as in numeric handler */
static void misc_alpha_handler(
	const char *prompt, char *defvalue,
	aal_check_alpha_func_t check_func, 
	void *data)
{
	char *line;
	char buff[255];
    
	aal_assert("umka-1133", prompt != NULL);
    
	aal_memset(buff, 0, sizeof(buff));
	aal_snprintf(buff, sizeof(buff), "%s [%s]: ", prompt, defvalue);
    
	while (1) {
		if (aal_strlen((line = misc_readline(buff, stderr))) == 0) {
			if (!check_func || check_func(defvalue, data))
				return;
		}

		if (!check_func || check_func(line, data))
			break;
	}

	aal_memcpy(defvalue, line, aal_strlen(line));
}

/* Common for all misc ui get numeric handler */
static int64_t misc_numeric_handler(
	const char *prompt,			/* prompt message */
	int64_t defvalue,			/* default value */
	aal_check_numeric_func_t check_func,	/* user's check method */
	void *data)				/* opaque data for check_func */
{
	char buff[255];
	int64_t value = 0;
    
	aal_assert("umka-1132", prompt != NULL);
    
	aal_memset(buff, 0, sizeof(buff));
    
	aal_snprintf(buff, sizeof(buff), "%s [%lli]: ", 
		     prompt, defvalue);
    
	while (1) {
		char *line;
	
		if (aal_strlen((line = misc_readline(buff, stderr))) == 0) 
			return defvalue;

		if ((value = misc_size2long(line)) == INVAL_DIG) {
			aal_error("Invalid numeric has been detected (%s). "
				  "Number is expected (1, 1K, 1M, 1G)", line);
			continue;
		}
	
		if (!check_func || check_func(value, data))
			break;
	}
    
	return value; 
}

void misc_print_banner(char *name) {
	char *banner;
   
	fprintf(stderr, "%s %s\n", name, VERSION);
	fprintf(stderr, "Format release: 4.%d.%d\n",
		get_release_number_major(),
		get_release_number_minor());
    
	if (!(banner = aal_calloc(255, 0)))
		return;
    
	aal_snprintf(banner, 255, BANNER);
	misc_print_wrap(stderr, banner);
	fprintf(stderr, "\n");
	aal_free(banner);
}

static void _init(void) __attribute__((constructor));

static void _init(void) {
	
#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
	rl_initialize();
	rl_attempted_completion_function = 
		(CPPFunction *)misc_complete;
#endif
    
	aal_exception_set_handler(misc_exception_handler);
	aal_ui_set_numeric_handler(misc_numeric_handler);
	aal_ui_set_alpha_handler(misc_alpha_handler);

	/* Setting up the gauges */
	aux_gauge_set_handler(misc_progress_handler, GT_PROGRESS);
}
