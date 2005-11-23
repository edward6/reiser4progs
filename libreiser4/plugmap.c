/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   plugmap.c -- plugin mapping reiser4 code. Performs the 
   [PLUGIN_TYPE, PLUGIN_ID] -> [PLUGIN_TYPE, PLUGIN_ID] pairs mapping. */

#ifndef ENABLE_MINIMAL

#include <reiser4/libreiser4.h>

/* CREATE plugin mapping. */
reiser4_create_plug_t create_reg40_plug = {
	.p = {
		.id = {CREATE_REG40_ID, 0, CREATE_PLUG_TYPE},
		.label = "create_reg40",
		.desc = "Create reg40 regular children plugin.",
	},
	
	.objid = OBJECT_REG40_ID,
};

reiser4_create_plug_t create_ccreg40_plug = {
	.p = {
		.id = {CREATE_CCREG40_ID, 0, CREATE_PLUG_TYPE},
		.label = "create_ccreg40",
		.desc = "Create ccreg40 regular children plugin.",
	},
	
	.objid = OBJECT_CCREG40_ID,
};
#endif
