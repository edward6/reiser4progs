/* Copyright 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   repair/librepair.h -- the central recovery include file. */

#ifndef LIBREPAIR_H
#define LIBREPAIR_H

#ifdef __cplusplus
extern "C" {
#endif
	
#include <reiser4/reiser4.h>
#include <repair/repair.h>
#include <repair/plugin.h>
#include <repair/filesystem.h>
#include <repair/tree.h>
#include <repair/format.h>
#include <repair/alloc.h>
#include <repair/master.h>
#include <repair/journal.h>
#include <repair/node.h>
#include <repair/place.h>
#include <repair/item.h>
#include <repair/object.h>

/*  -------------------------------------------------
    | Common scheem for communication with users.   |
    |-----------------------------------------------|
    |  stream  | default | with log | with 'no-log' |
    |----------|---------|----------|---------------|
    | warn     | stderr  | stderr   |  -            |
    | info     | stderr  | log      |  -            |
    | error    | stderr  | log      |  -            |
    | fatal    | stderr  | stderr   | stderr        |
    | bug      | stderr  | stderr   | stderr        |
    -------------------------------------------------
    
    info   - Information about what is going on. 
    warn   - warnings to users about what is going on, which should be viewed 
             on-line.
    error  - Problems. 
    fatal  - Fatal problems which are supposed to be viewed on-line. 

    Modifiers: Auto (choose the default answer for all questions) and Verbose 
    (provide some extra information) and Quiet (quiet progress and provide only
    fatal and bug infotmation to stderr; does not affect the log content though
    if log presents). */

#ifdef __cplusplus
}
#endif

#endif
