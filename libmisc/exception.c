/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   exception.c -- common for all progs exception handler and related
   functions. */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

#include <aal/libaal.h>
#include <misc/misc.h>

/* Strings for all exception types */
static char *type_names[] = {
	"",
        "Info ",
        "",
        "Warn ",
        "Error",
        "Fatal",
        "Bug  "
};

#define TYPES_COUNT (sizeof(type_names) / sizeof(void *))

/* This function returns number of specified turned on options */
static int misc_exception_option_count(
	aal_exception_option_t options,	    /* options to be inspected */
	int start)			    /* options will be inspected started
					       from */
{
	int i, count = 0;
    
	for (i = start; i < aal_log2(EXCEPTION_OPT_LAST); i++)
		count += ((1 << i) & options) ? 1 : 0;

	return count;
}

/* This function makes search for option by its name in passed available option
   set. */
static aal_exception_option_t misc_exception_oneof(
	char *name,			    /* option name to be checked */
	aal_exception_option_t options)     /* aavilable options */
{
	int i;
    
	if (!name || aal_strlen(name) == 0)
		return EXCEPTION_OPT_UNHANDLED;
    
	for (i = 0; i < aal_log2(EXCEPTION_OPT_LAST); i++) {
		if ((1 << i) & options) {
			char str1[256], str2[256];
			char *opt = aal_exception_option_name(1 << i);

			aal_memset(str1, 0, sizeof(str1));
			aal_memset(str2, 0, sizeof(str2));
	    
			misc_upper_case(str1, opt);
			misc_upper_case(str2, name);
	    
			if (aal_strncmp(str1, str2, aal_strlen(str2)) == 0 || 
			    (aal_strlen(str2) == 1 && str1[0] == name[0]))
			{
				return 1 << i;
			}
		}
	}
    
	return EXCEPTION_OPT_UNHANDLED;
}

/* Constructs exception message. */
static void misc_exception_print_wrap(aal_exception_t *exception,
				       void *stream)
{
	char buff[4096];

	aal_memset(buff, 0, sizeof(buff));
    
	if (exception->type != EXCEPTION_TYPE_MESSAGE &&
	    exception->type < TYPES_COUNT)
	{
		aal_snprintf(buff, sizeof(buff), "%s: ", 
			     type_names[exception->type]);
	}
    
	aal_strncat(buff, exception->message, 
		    aal_strlen(exception->message));

	misc_print_wrap(stream, buff);
}

/* This function prints exception options awailable to be choosen, takes user
   enter and converts it into aal_exception_option_t type. */
static aal_exception_option_t misc_exception_prompt(
	aal_exception_option_t options,  /* exception options can be selected */
	void *stream)
{
	int i;
	char *option;
	char prompt[256];

	if (misc_exception_option_count(options, 0) == 0)
		return EXCEPTION_OPT_UNHANDLED;
    
	aal_memset(prompt, 0, sizeof(prompt));
    
	aal_strncat(prompt, "(", 1);
	for (i = 1; i < aal_log2(EXCEPTION_OPT_LAST); i++) {
		if ((1 << i) & options) {
			char *opt = aal_exception_option_name(1 << i);
			int count = misc_exception_option_count(options, i + 1);
	    
			aal_strncat(prompt, opt, aal_strlen(opt));
	    
			if (i < aal_log2(EXCEPTION_OPT_LAST) - 1 && count  > 0)
				aal_strncat(prompt, "/", 1);
			else
				aal_strncat(prompt, "): ", 3);
		}
	}
    
	if (!(option = misc_readline(prompt, stream)) || aal_strlen(option) == 0)
		return EXCEPTION_OPT_UNHANDLED;
    
	return misc_exception_oneof(option, options);
}

/* Streams assigned with exception type are stored here */
static void *streams[10];

/* Current misc gauge. Used for correct pausing when exception */
extern aal_gauge_t *current_gauge;

/* Common exception handler for all reiser4progs. It implements exception
   handling in "question-answer" maner and used for all communications with
   user. */
aal_exception_option_t misc_exception_handler(
	aal_exception_t *exception)		/* exception to be processed */
{
#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
	int i;
	aal_list_t *variant = NULL;
#endif
	int tty;
	void *stream = stderr;
	aal_exception_option_t opt = EXCEPTION_OPT_UNHANDLED;
    
	if (misc_exception_option_count(exception->options, 0) == 1) {
		if (!(stream = streams[exception->type]))
			return exception->options;
	}
	
	if ((tty = fileno(stream)) == -1)
		return EXCEPTION_OPT_UNHANDLED;

	if (current_gauge && stream == stderr)
		aal_gauge_pause(current_gauge);
	else {
		if (isatty(tty))
			misc_wipe_line(stream);
	}

	misc_exception_print_wrap(exception, stream);
    
	if (misc_exception_option_count(exception->options, 0) == 1)
		return exception->options;

#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
	for (i = 1; i < aal_log2(EXCEPTION_OPT_LAST); i++) {
		if ((1 << i) & exception->options) {
			char *name = aal_exception_option_name(1 << i);
			variant = aal_list_append(variant, name);
		}
	}
	
	variant = aal_list_first(variant);
	misc_set_variant(variant);
#endif

	do {
		opt = misc_exception_prompt(exception->options, stream);
	} while (opt == EXCEPTION_OPT_UNHANDLED);
	
#if defined(HAVE_LIBREADLINE) && defined(HAVE_READLINE_READLINE_H)
	aal_list_free(variant, NULL, NULL);
	misc_set_variant(NULL);
#endif

	return opt;
}

/* This function sets up exception streams */
void misc_exception_set_stream(
	aal_exception_type_t type,	/* type to be assigned with stream */
	void *stream)	                /* stream to be assigned */
{
	streams[type] = stream;
}

/* This function gets exception streams */
void *misc_exception_get_stream(
	aal_exception_type_t type)	/* type exception stream will be obtained for */
{
	return streams[type];
}
