/*
    repair/filter.h -- the structures and methods needed for fsck pass1. 
    Copyright (C) 1996 - 2002 Hans Reiser
*/

#ifndef REPAIR_FILTER_H
#define REPAIR_FILTER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <repair/repair.h>

extern errno_t repair_filter_joint_open(reiser4_joint_t **, blk_t, void *);
extern errno_t repair_filter_before_traverse(reiser4_joint_t *, reiser4_coord_t *, 
    void *);
extern errno_t repair_filter_after_traverse(reiser4_joint_t *, reiser4_coord_t *, 
    void *);
extern errno_t repair_filter_setup_traverse(reiser4_joint_t *, reiser4_coord_t *, 
    void *);
extern errno_t repair_filter_update_traverse(reiser4_joint_t *, reiser4_coord_t *, 
    void *);
extern errno_t repair_filter_joint_check(reiser4_joint_t *, void *);
extern errno_t repair_filter_setup(reiser4_fs_t *, repair_check_t *);
extern errno_t repair_filter_update(reiser4_fs_t *, repair_check_t *);

#endif
