/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   pset.h -- reiser4 plugin set functions. */

#ifndef REISER4_PSET_H
#define REISER4_PSET_H

#ifndef ENABLE_MINIMAL

#define PSET_MAGIC "PsEt"

struct reiser4_pset_backup {
	rid_t id[PSET_STORE_LAST];
};

extern void reiser4_pset_root(object_info_t *info);

extern uint64_t reiser4_pset_build_mask(reiser4_tree_t *tree, 
					reiser4_pset_t *pset);

extern errno_t reiser4_pset_tree(reiser4_tree_t *tree, int check);

extern errno_t reiser4_pset_backup(reiser4_tree_t *tree, 
				   backup_hint_t *hint);

#else

extern errno_t reiser4_pset_tree(reiser4_tree_t *tree);

#endif

extern void reiser4_pset_complete(reiser4_tree_t *tree, 
				  object_info_t *info);

extern errno_t reiser4_tset_init(reiser4_tree_t *tree);

extern reiser4_plug_t *reiser4_pset_find(rid_t member, rid_t id, int is_pset);

#endif
