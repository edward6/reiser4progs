/*
  exception.c -- exceptions handling functions. Exception factory is used for
  providing unified interface for error handling.

  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef ENABLE_STAND_ALONE

#include <aal/aal.h>

static aal_exception_handler_t exception_handler = NULL;

/* Strings for all exception types */
static char *type_names[] = {
	"Information", 
	"Warning", 
	"Error", 
	"Fatal", 
	"Bug"
};

/* Strings for all exception options */
static char *option_names[] = {
	"Yes", 
	"No", 
	"OK", 
	"Retry", 
	"Ignore", 
	"Cancel"
};

static int disable_count = 0;

/* 
  Helper functions for getting different exception attributes (option string,
  type string, etc). They are used in exception handing functions.
*/
char *aal_exception_type_name(
	aal_exception_type_t type)	/* type to be converted to string */
{
	return type_names[type - 1];
}

/* Returns exception type from passed exception instance */
aal_exception_type_t aal_exception_type(
	aal_exception_t *exception)	/* exception type will be obtained of */
{
	return exception->type;
}

/* Converts passed exception option into corresponding string */
char *aal_exception_option_name(
	aal_exception_option_t opt)	/* option to be converted to string */
{
	return option_names[aal_log2(opt) - 1];
}

/* Returns exception option from passed exception */
aal_exception_option_t aal_exception_option(
	aal_exception_t *exception)	/* exception option will be obtained of */
{
	return exception->options;
}

/* Retutrns exception message */
char *aal_exception_message(
	aal_exception_t *exception)	/* exception message will be obtained of */
{
	return exception->message;
}

/* 
  Sets alternative exception handler, if passed handler isn't NULL. Otherwise
  sets exception handler into default one.
*/
void aal_exception_set_handler(
	aal_exception_handler_t handler) /* new exception handler */
{
	exception_handler = handler;
}

aal_exception_handler_t aal_exception_get_handler(void) {
	return exception_handler;
}

/* Finishes exception life cycle, that is, destroys exception */
void aal_exception_catch(
	aal_exception_t *exception)	/* exception, to be destroyed */
{
	if (!exception) return;
    
	aal_free(exception->message);
	aal_free(exception);
}

/* 
  The job of this function is to call current exception handler and return the
  result of handling (for instance, retry, ignore, etc).
*/
static aal_exception_option_t aal_exception_actual_throw(
	aal_exception_t *exception)	/* exception to be thrown */
{
	aal_exception_option_t opt;

	if (!exception_handler || disable_count)
		return EXCEPTION_UNHANDLED;
	
	opt = exception_handler(exception);
	aal_exception_catch(exception);
    
	return opt;
}

/* 
  Public function for throw exception. It creates new exception instance and
  pass the control to aal_exception_actual_throw function for further handling.
*/
aal_exception_option_t aal_exception_throw(
	aal_exception_type_t type,	/* exception type */
	aal_exception_option_t opts,	/* exception options */
	const char *message,		/* format string for exception message */ 
	...)				/* a number of parameters for format string */
{
	va_list arg_list;
	aal_exception_t *exception;

	/* Alloacting memory for exception */
	if (!(exception = (aal_exception_t *)aal_malloc(sizeof(aal_exception_t))))
		goto error_no_memory;

	/* Allocating memory for exception message */
	if (!(exception->message = (char*)aal_calloc(256, 0)))
		goto error_no_memory;

	/* Initializing exception instance by passed params */
	exception->type = type;
	exception->options = opts;

	/* Forming exception message using passed format string and parameters */
	va_start(arg_list, message);
	aal_vsnprintf(exception->message, 256, message, arg_list);
	va_end(arg_list);
    
	return aal_exception_actual_throw(exception);
    
 error_no_memory:
	return EXCEPTION_UNHANDLED;
}

/* 
  These functions are used for switching exception factory into silent mode.
  This mode forces it do not handle exceptions at all. As it may be used few
  times while the control flows through the stack, here is the special counter
  for keeping track of how many times exception factory was disabled.
*/
void aal_exception_off(void) {
	disable_count++;
}

void aal_exception_on(void) {
	if (disable_count > 0)
		disable_count--;
}

#endif
