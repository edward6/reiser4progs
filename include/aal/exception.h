/*
  exception.h -- exception factory functions.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_EXCEPTION_H
#define AAL_EXCEPTION_H

#include <aal/types.h>

extern char *aal_exception_type_name(aal_exception_type_t type);
extern char *aal_exception_option_name(aal_exception_option_t opt);

extern char *aal_exception_message(aal_exception_t *ex);
extern aal_exception_type_t aal_exception_type(aal_exception_t *ex);
extern aal_exception_option_t aal_exception_option(aal_exception_t *ex);

extern aal_exception_handler_t aal_exception_get_handler(void);
extern void aal_exception_set_handler(aal_exception_handler_t handler);

extern aal_exception_option_t aal_exception_throw(aal_exception_type_t type, 
						  aal_exception_option_t opt,
						  const char *message, ...);

extern void aal_exception_on(void);
extern void aal_exception_off(void);

#define aal_exception_fatal(msg, list...) \
        aal_exception_throw(EXCEPTION_FATAL, EXCEPTION_OK, msg, ##list)
	
#define aal_exception_bug(msg, list...)	\
        aal_exception_throw(EXCEPTION_BUG, EXCEPTION_OK, msg, ##list)
	
#define aal_exception_error(msg, list...) \
        aal_exception_throw(EXCEPTION_ERROR, EXCEPTION_OK, msg, ##list)
	
#define aal_exception_warn(msg, list...) \
        aal_exception_throw(EXCEPTION_WARNING, EXCEPTION_OK, msg, ##list)
	
#define aal_exception_info(msg, list...) \
        aal_exception_throw(EXCEPTION_INFORMATION, EXCEPTION_OK, msg, ##list)

#define aal_exception_yesno(msg, list...) \
        aal_exception_throw(EXCEPTION_INFORMATION, EXCEPTION_YESNO, msg, ##list)

#define aal_exception_okcancel(msg, list...) \
        aal_exception_throw(EXCEPTION_INFORMATION, EXCEPTION_OKCANCEL, msg, ##list)

#define aal_exception_retryignore(msg, list...) \
        aal_exception_throw(EXCEPTION_ERROR, EXCEPTION_RETRYIGNORE, msg, ##list)

#endif

