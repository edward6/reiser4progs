/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#include <reiser4/libreiser4.h>

extern reiser4_profile_t defprof;

typedef struct pset_member {
	/* The corresponding slot in the profile. */
	rid_t prof;
	
#ifndef ENABLE_MINIMAL
	/* The flag if a plugin essential or not. Non-essential plugins may 
	   be changed at anytime; non-essential plugins may present in a file's 
	   SD even if they differ from the root ones to not get the object 
	   settings changed at the root pset change. */
	bool_t ess;
#endif
} pset_member_t;

#ifndef ENABLE_MINIMAL
pset_member_t pset_prof[PSET_LAST] = {
	[PSET_OBJ]	= {PROF_OBJ,		1},
	[PSET_DIR]	= {PROF_DIR,		1},
	[PSET_PERM]	= {PROF_PERM,		1},
	[PSET_POLICY]	= {PROF_POLICY,		0},
	[PSET_HASH]	= {PROF_HASH,		1},
	[PSET_FIBRE]	= {PROF_FIBRE,		1},
	[PSET_STAT]	= {PROF_STAT,		0},
	[PSET_DIRITEM]	= {PROF_DIRITEM,	1},

	/* Hmm, actually next 5 are essential for files, i.e. cannot be changed 
	   w/out convertion. However this flag indicates if a slot can present 
	   while differs from the root one. And these fields must always present 
	   in crc files. */
	[PSET_CRYPTO]	= {PROF_CRYPTO,		0},
	[PSET_DIGEST]	= {PROF_DIGEST,		0},
	[PSET_COMPRESS]	= {PROF_COMPRESS,	0},
	[PSET_CMODE]	= {PROF_CMODE,		0},
	[PSET_CLUSTER]	= {PROF_CLUSTER,	0},
	
	[PSET_TAIL]	= {PROF_TAIL,		1},
	[PSET_EXTENT]	= {PROF_EXTENT,		1},
	[PSET_CTAIL]	= {PROF_CTAIL,		1},
};

rid_t hset_prof[HSET_LAST] = {
	/* The plugin to create children. */
	[HSET_CREATE]	= PROF_HEIR_CREATE,
	[HSET_HASH]	= PROF_HEIR_HASH,
	[HSET_FIBRE]	= PROF_HEIR_FIBRE,
	[HSET_DIR_ITEM] = PROF_HEIR_DIRITEM,
};
#else
pset_member_t pset_prof[PSET_LAST] = {
	[PSET_OBJ]	= {PROF_OBJ},
	[PSET_DIR]	= {PROF_DIR},
	[PSET_PERM]	= {PROF_PERM},
	[PSET_POLICY]	= {PROF_POLICY},
	[PSET_HASH]	= {PROF_HASH},
	[PSET_FIBRE]	= {PROF_FIBRE},
	[PSET_STAT]	= {PROF_STAT},
	[PSET_DIRITEM]	= {PROF_DIRITEM},
	[PSET_CRYPTO]	= {PROF_CRYPTO},
	[PSET_DIGEST]	= {PROF_DIGEST},
	[PSET_COMPRESS]	= {PROF_COMPRESS},
	[PSET_CMODE]	= {PROF_CMODE},
	[PSET_CLUSTER]	= {PROF_CLUSTER},
};
#endif

#ifndef ENABLE_MINIMAL
static int pset_tree_isready(reiser4_tree_t *tree) {
	return tree->ent.pset[PSET_HASH] != NULL;
}
#endif

void reiser4_pset_complete(reiser4_tree_t *tree, object_info_t *info) {
	int i;

#ifndef ENABLE_MINIMAL
	/* This is needed for root recovery. Tree is not initialized by 
	   that time yet. */
	if (!pset_tree_isready(tree))
		return reiser4_pset_root(info);
#endif
	
	/* Only pset needs to be completed. */
	for (i = 0; i < PSET_LAST; i++) {
		if (info->pset.plug_mask & (1 << i))
			continue;

		info->pset.plug[i] = tree->ent.pset[i];
	}
}

/* Returns NULL if @member is valid but there is no written plugins in the 
   library for it yet (only @id == 0 are allowed). INVAL_PTR if plugin @id 
   should present for @member in the library but nothing is found. Otherwise,
   returns the plugin pointer. */
static reiser4_plug_t *reiser4_pset_plug(rid_t member, rid_t id) {
	reiser4_plug_t *plug;
	aal_assert("vpf-1613", member < PSET_STORE_LAST);
	
	if (defprof.pid[pset_prof[member].prof].id.type == PARAM_PLUG_TYPE) {
#ifndef ENABLE_MINIMAL
		return id < defprof.pid[pset_prof[member].prof].max ? 
			NULL : INVAL_PTR;
#else
		return NULL;
#endif
	}
	
	plug = reiser4_factory_ifind(
		defprof.pid[pset_prof[member].prof].id.type, id);
	
	return plug ? plug : INVAL_PTR;
}

#ifndef ENABLE_MINIMAL
static reiser4_plug_t *reiser4_hset_plug(rid_t member, rid_t id) {
	aal_assert("vpf-1613", member < HSET_LAST);
	return reiser4_factory_ifind(
		defprof.pid[hset_prof[member]].id.type, id) ? : INVAL_PTR;
}
#endif

reiser4_plug_t *reiser4_pset_find(rid_t member, rid_t id, int is_pset) {
	return is_pset ? 
	       reiser4_pset_plug(member, id) :
	       reiser4_hset_plug(member, id);
}

#ifndef ENABLE_MINIMAL
/* Fill up missed pset slots in the root object. */
void reiser4_pset_root(object_info_t *info) {
	uint8_t i;
	
	aal_assert("vpf-1639", info != NULL);

	/* All plugins must present in the root. Get them from the profile. */
	for (i = 0; i < PSET_LAST; i++) {
		if (info->pset.plug_mask & (1 << i))
			continue;
		
		if (i == PSET_OBJ) {
			/* Special case: root file plug. */
			info->pset.plug[i] = reiser4_profile_plug(PROF_DIRFILE);
		} else if (defprof.pid[pset_prof[i].prof].id.type == 
			   PARAM_PLUG_TYPE) 
		{
			info->pset.plug[i] = 
				(void *)defprof.pid[pset_prof[i].prof].id.id;
			
		} else {
			info->pset.plug[i] = 
				reiser4_profile_plug(pset_prof[i].prof);
		}
	}

	for (i = 0; i < HSET_LAST; i++) {
		/* Set a hset plugin only if the plugin was overwritten 
		   in the profile. */
		if (!aal_test_bit(&defprof.mask, hset_prof[i]))
			continue;

		info->hset.plug[i] = reiser4_profile_plug(hset_prof[i]);
		info->hset.plug_mask |= (1 << i);
	}
}

/* Builds & returns the difference between tree pset & the current @pset. */
uint64_t reiser4_pset_build_mask(reiser4_tree_t *tree, 
				  reiser4_pset_t *pset) 
{

	uint64_t mask;
	uint8_t i;
	
	aal_assert("vpf-1644", tree != NULL);
	aal_assert("vpf-1646", pset != NULL);
	
	mask = 0;
	
	if (!pset_tree_isready(tree)) {
		/* If HASH plugin is not initialized, no object exists.
		   The special case for the root directory. All pset 
		   members must be stored. */
		mask = (1 << PSET_STORE_LAST) - 1;
		mask &= ~(1 << PSET_DIR);
		return mask;
	}
	
	for (i = 0; i < PSET_STORE_LAST; i++) {
		/* Leave non-essential members as is. */
                if (!pset_prof[i].ess)
                        continue;
		
		/* Store if do not match for essential plugins. */
		if (tree->ent.pset[i] != pset->plug[i])
			mask |= (1 << i);
		
		/* Do not store If match for essential plugins. */
		if (tree->ent.pset[i] == pset->plug[i])
			mask &= ~(1 << i);

		/* FIXME: What is a non-essential plugin is inherited from 
		   the parent does not match the fs-defaul one. It is not 
		   explicitely set for this particular file. Should it be 
		   stored on disk? 
		   The answer is NO for now -- non-essential plugins forget 
		   their special settings in parents. */
	}
	
	return mask;
}
#endif

errno_t reiser4_tset_init(reiser4_tree_t *tree) {
	reiser4_plug_t *plug;
	rid_t pid;
	int i;
	
	aal_assert("vpf-1608", tree != NULL);
	aal_assert("vpf-1609", tree->fs != NULL);
	aal_assert("vpf-1610", tree->fs->format != NULL);

	/* Init the key plugin. */
	pid = reiser4call(tree->fs->format, key_pid);

	if (!(plug = reiser4_factory_ifind(KEY_PLUG_TYPE, pid))) {
		aal_error("Can't find a key plugin by its id %d.", pid);
		return -EINVAL;
	}

	tree->ent.tset[TSET_KEY] = plug;
	tree->ent.tset[TSET_REGFILE] = reiser4_profile_plug(PROF_REGFILE);
	tree->ent.tset[TSET_DIRFILE] = reiser4_profile_plug(PROF_DIRFILE);
	tree->ent.tset[TSET_SYMFILE] = reiser4_profile_plug(PROF_SYMFILE);
	tree->ent.tset[TSET_SPLFILE] = reiser4_profile_plug(PROF_SPLFILE);

	/* Init other tset plugins. */
#ifndef ENABLE_MINIMAL
	tree->ent.tset[TSET_NODE] = reiser4_profile_plug(PROF_NODE);
	tree->ent.tset[TSET_NODEPTR] = reiser4_profile_plug(PROF_NODEPTR);
#endif

	/* These plugins should be initialized at the tree init. Later they can 
	   be reinitialized with the root directory pset or anything else. */

	/* Build the param pset mask, keep it in the tree instance. */
	for (i = 0; i < PSET_LAST; i++) {
		if (defprof.pid[pset_prof[i].prof].id.type == PARAM_PLUG_TYPE)
			tree->ent.param_mask |= (1 << i);
	}
	
	return 0;
}

#ifndef ENABLE_MINIMAL
errno_t reiser4_pset_tree(reiser4_tree_t *tree, int check) {
	uint64_t mask;
	int i;
#else
errno_t reiser4_pset_tree(reiser4_tree_t *tree) {
#endif
	reiser4_object_t *object;
	
	aal_assert("vpf-1624", tree != NULL);

	if (!(object = reiser4_object_obtain(tree, NULL, &tree->key))) {
		aal_error("Failed to initialize the fs-global object "
			  "plugin set: failed to open the root directory.");
		return -EINVAL;
	}
	
	aal_memcpy(tree->ent.pset, object->info.pset.plug, 
		   sizeof(reiser4_plug_t *) * PSET_LAST);

	tree->ent.pset[PSET_OBJ] = NULL;
	tree->ent.pset[PSET_DIR] = NULL;
	
#ifndef ENABLE_MINIMAL
	mask = object->info.pset.plug_mask;
#endif

	reiser4_object_close(object);

#ifndef ENABLE_MINIMAL
	/* Check that all 'on-disk' plugins are obtained. */
	for (i = PSET_DIR + 1; i < PSET_STORE_LAST; i++) {
		/* If root should not be checked (debugfs), skip the loop. */
		if (!check)
			continue;
		
		if (mask & (1 << i)) {
			/* Set in mask & initialized. */
			if (tree->ent.pset[i])
				continue;
			
			/* Set in mask & PARAMETER. */
			if (defprof.pid[pset_prof[i].prof].id.type == 
			    PARAM_PLUG_TYPE)
			{
				continue;
			}
		} 
		
		/* Other cases are errors. */
		aal_error("The slot %u in the fs-global object "
			  "plugin set is not initialized.", i);
		return -EINVAL;
	}

	/* Set others from the profile. */
	for (; i < PSET_LAST; i++) {
		if (tree->ent.pset[i])
			continue;

		tree->ent.pset[i] = reiser4_profile_plug(pset_prof[i].prof);
	}
#endif

	return 0;
}
