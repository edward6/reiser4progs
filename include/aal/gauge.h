/*
  gauge.h -- progress-bar functions.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_GAUGE_H
#define AAL_GAUGE_H

#include <aal/types.h>

extern aal_gauge_t *aal_gauge_create(aal_gauge_type_t type,
				     const char *name,
				     aal_gauge_handler_t handler,
				     void *data);

extern void aal_gauge_update(aal_gauge_t *gauge, uint32_t value);

extern void aal_gauge_rename(aal_gauge_t *gauge, const char *name, ...)
                             __aal_check_format__(printf, 2, 3);

extern void aal_gauge_reset(aal_gauge_t *gauge);
extern void aal_gauge_start(aal_gauge_t *gauge);
extern void aal_gauge_done(aal_gauge_t *gauge);
extern void aal_gauge_touch(aal_gauge_t *gauge);
extern void aal_gauge_free(aal_gauge_t *gauge);
extern void aal_gauge_pause(aal_gauge_t *gauge);
extern void aal_gauge_resume(aal_gauge_t *gauge);

#endif

