/*
  gauge.h -- progress-bar functions.
    
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_GAUGE_H
#define AAL_GAUGE_H

#ifndef ENABLE_STAND_ALONE

#include <aal/types.h>

extern void aal_gauge_reset(aal_gauge_t *gauge);
extern void aal_gauge_start(aal_gauge_t *gauge);
extern void aal_gauge_done(aal_gauge_t *gauge);
extern void aal_gauge_touch(aal_gauge_t *gauge);
extern void aal_gauge_free(aal_gauge_t *gauge);
extern void aal_gauge_pause(aal_gauge_t *gauge);
extern void aal_gauge_resume(aal_gauge_t *gauge);

extern void aal_gauge_update(aal_gauge_t *gauge,
			     uint32_t value);

extern void aal_gauge_rename(aal_gauge_t *gauge,
			     const char *name, ...);

extern aal_gauge_t *aal_gauge_create(uint32_t type,
				     const char *name,
				     void *data);

extern void aal_gauge_set_handler(uint32_t type,
				  aal_gauge_handler_t handler);

extern aal_gauge_handler_t aal_gauge_get_handler(uint32_t type);

#endif

#endif

