/*
  header.c -- this just defines start symbols for plugins init and fini methods
  at the start of corresponding ELF-section. It is needed for monolithic style
  of building.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licencing governed by
  reiser4progs/COPYING.
*/

unsigned long __plugin_start __attribute__((__section__(".plugins"))) = 0;
