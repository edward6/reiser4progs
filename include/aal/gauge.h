/*
  gauge.h -- progress-bar structures.
  Copyright 1996-2002 (C) Hans Reiser.
*/

#ifndef GAUGE_H
#define GAUGE_H

typedef struct aal_gauge aal_gauge_t;

enum aal_gauge_type {
	GAUGE_PERCENTAGE,
	GAUGE_INDICATOR,
	GAUGE_SILENT
};

typedef enum aal_gauge_type aal_gauge_type_t;

enum aal_gauge_state {
	GAUGE_STARTED,
	GAUGE_RUNNING,
	GAUGE_PAUSED,
	GAUGE_DONE,
};

typedef enum aal_gauge_state aal_gauge_state_t;

typedef void (*aal_gauge_handler_t)(aal_gauge_t *);

struct aal_gauge {
	aal_gauge_type_t type;
	aal_gauge_state_t state;
	aal_gauge_handler_t handler;

	void *data;
    
	char name[256];
	uint32_t value;
};

extern aal_gauge_t *aal_gauge_create(aal_gauge_type_t type, const char *name,
				aal_gauge_handler_t handler, void *data);

extern void aal_gauge_update(aal_gauge_t *gauge, uint32_t value);

extern void aal_gauge_rename(aal_gauge_t *gauge, const char *name, ...)
                             __check_format__(printf, 2, 3);

extern void aal_gauge_reset(aal_gauge_t *gauge);
extern void aal_gauge_start(aal_gauge_t *gauge);
extern void aal_gauge_done(aal_gauge_t *gauge);
extern void aal_gauge_touch(aal_gauge_t *gauge);
extern void aal_gauge_free(aal_gauge_t *gauge);
extern void aal_gauge_pause(aal_gauge_t *gauge);

#endif

