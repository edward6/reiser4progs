/*
  header.c -- this just defines end symbol for plugins init and fini methods
  at the end of corresponding ELF-section. It is needed for monolithic style
  of building.
  
  Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

unsigned long __plugin_end __attribute__((__section__(".plugins"))) = 0;
