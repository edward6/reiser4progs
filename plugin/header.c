/*
  header.c -- this just defines start symbols for plugins init and fini methods
  at the start of corresponding ELF-section. It is needed for monolithic style
  of building.
  Copyright (C) 1996-2002 Hans Reiser.
*/

unsigned long __plugin_start __attribute__((__section__(".plugins"))) = 0;
