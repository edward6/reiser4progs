/*
  header.c -- this just defines end symbol for plugins init and fini methods
  at the end of corresponding ELF-section. It is needed for monolithic style
  of building.
  Copyright (C) 1996-2002 Hans Reiser.
*/

unsigned long __plugin_end __attribute__((__section__(".plugins"))) = 0;
