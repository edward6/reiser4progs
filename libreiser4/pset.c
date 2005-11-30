/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#include <reiser4/libreiser4.h>

extern reiser4_profile_t defprof;

typedef struct opset_member {
	/* The corresponding slot in the profile. */
	rid_t prof;
	
#ifndef ENABLE_MINIMAL
	/* The flag if a plugin essential or not. Non-essential plugins may 
	   be changed at anytime; non-essential plugins may present in a file's 
	   SD even if they differ from the root ones to not get the object 
	   settings changed at the root pset change. */
	bool_t ess;
#endif
} opset_member_t;

#ifndef ENABLE_MINIMAL
opset_member_t opset_prof[OPSET_LAST] = {
	[OPSET_OBJ]	= {PROF_OBJ,		1},
	[OPSET_DIR]	= {PROF_DIR,		1},
	[OPSET_PERM]	= {PROF_PERM,		1},
	[OPSET_POLICY]	= {PROF_POLICY,		0},
	[OPSET_HASH]	= {PROF_HASH,		1},
	[OPSET_FIBRE]	= {PROF_FIBRE,		1},
	[OPSET_STAT]	= {PROF_STAT,		0},
	[OPSET_DIRITEM] = {PROF_DIRITEM,	1},

	/* Hmm, actually next 5 are essential for files, i.e. cannot be changed 
	   w/out convertion. However this flag indicates if a slot can present 
	   while differs from the root one. And these fields must always present 
	   in crc files. */
	[OPSET_CRYPTO]	= {PROF_CRYPTO,		0},
	[OPSET_DIGEST]	= {PROF_DIGEST,		0},
	[OPSET_COMPRESS]= {PROF_COMPRESS,	0},
	[OPSET_CMODE]	= {PROF_CMODE,		0},
	[OPSET_CLUSTER] = {PROF_CLUSTER,	0},
	
	/* The plugin to create children. */
	[OPSET_CREATE]	= {PROF_CREATE,		0},
	
	[OPSET_TAIL]	= {PROF_TAIL,		1},
	[OPSET_EXTENT]	= {PROF_EXTENT,		1},
	[OPSET_CTAIL]	= {PROF_CTAIL,		1},
};
#else
opset_member_t opset_prof[OPSET_LAST] = {
	[OPSET_OBJ]	= {PROF_OBJ},
	[OPSET_DIR]	= {PROF_DIR},
	[OPSET_PERM]	= {PROF_PERM},
	[OPSET_POLICY]	= {PROF_POLICY},
	[OPSET_HASH]	= {PROF_HASH},
	[OPSET_FIBRE]	= {PROF_FIBRE},
	[OPSET_STAT]	= {PROF_STAT},
	[OPSET_DIRITEM] = {PROF_DIRITEM},
	[OPSET_CRYPTO]	= {PROF_CRYPTO},
	[OPSET_DIGEST]	= {PROF_DIGEST},
	[OPSET_COMPRESS]= {PROF_COMPRESS},
	[OPSET_CMODE]	= {PROF_CMODE},
	[OPSET_CLUSTER] = {PROF_CLUSTER},
	[OPSET_CREATE]	= {PROF_CREATE},
};
#endif

#ifndef ENABLE_MINIMAL
static int pset_tree_isready(reiser4_tree_t *tree) {
	return tree->ent.opset[OPSET_HASH] != NULL;
}
#endif

void reiser4_opset_complete(reiser4_tree_t *tree, reiser4_opset_t *opset) {
	int i;

#ifndef ENABLE_MINIMAL
	/* This is needed for root recovery. Tree is not initialized by 
	   that time yet. */
	if (!pset_tree_isready(tree))
		return reiser4_opset_root(opset);
#endif
	
	for (i = 0; i < OPSET_LAST; i++) {
		if (opset->plug_mask & (1 << i))
			continue;

		opset->plug[i] = tree->ent.opset[i];
	}
}

/* Returns NULL if @member is valid but there is no written plugins in the 
   library for it yet (only @id == 0 are allowed). INVAL_PTR if plugin @id 
   should present for @member in the library but nothing is found. Otherwise,
   returns the plugin pointer. */
reiser4_plug_t *reiser4_opset_plug(rid_t member, rid_t id) {
	reiser4_plug_t *plug;
	aal_assert("vpf-1613", member < OPSET_STORE_LAST);
	
	if (defprof.pid[opset_prof[member].prof].id.type == PARAM_PLUG_TYPE) {
#ifndef ENABLE_MINIMAL
		return id < defprof.pid[opset_prof[member].prof].max ? 
			NULL : INVAL_PTR;
#else
		return NULL;
#endif
	}
	
	plug = reiser4_factory_ifind(
		defprof.pid[opset_prof[member].prof].id.type, id);
	
	return plug ? plug : INVAL_PTR;
}

#ifndef ENABLE_MINIMAL
/* Fill up missed opset slots in the root object. */
void reiser4_opset_root(reiser4_opset_t *opset) {
	uint8_t i;
	
	aal_assert("vpf-1639", opset != NULL);

	/* All plugins must present in the root. Get them from the profile. */
	for (i = 0; i < OPSET_LAST; i++) {
		if (opset->plug_mask & (1 << i))
			continue;
		
		if (i == OPSET_OBJ) {
			/* Special case: root file plug. */
			opset->plug[i] = reiser4_profile_plug(PROF_DIRFILE);
		} else if (defprof.pid[opset_prof[i].prof].id.type == 
			   PARAM_PLUG_TYPE) 
		{
			opset->plug[i] = 
				(void *)defprof.pid[opset_prof[i].prof].id.id;
			
		} else {
			opset->plug[i] = 
				reiser4_profile_plug(opset_prof[i].prof);
		}
	}
}

/* Builds & returns the difference between tree opset & the current @opset. */
uint64_t reiser4_opset_build_mask(reiser4_tree_t *tree, 
				  reiser4_opset_t *opset) 
{

	uint64_t mask;
	uint8_t i;
	
	aal_assert("vpf-1644", tree != NULL);
	aal_assert("vpf-1646", opset != NULL);
	
	mask = 0;
	
	if (!pset_tree_isready(tree)) {
		/* If HASH plugin is not initialized, no object exists.
		   The special case for the root directory. All opset 
		   members must be stored. */
		mask = (1 << OPSET_STORE_LAST) - 1;
		mask &= ~(1 << OPSET_DIR);
		return mask;
	}
	
	for (i = 0; i < OPSET_STORE_LAST; i++) {
		/* Leave non-essential members as is. */
                if (!opset_prof[i].ess)
                        continue;
		
		/* Store if do not match for essential plugins. */
		if (tree->ent.opset[i] != opset->plug[i])
			mask |= (1 << i);
		
		/* Do not store If match for essential plugins. */
		if (tree->ent.opset[i] == opset->plug[i])
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

errno_t reiser4_pset_tree(reiser4_tree_t *tree) {
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

	tree->ent.tpset[TPSET_KEY] = plug;
	tree->ent.tpset[TPSET_REGFILE] = reiser4_profile_plug(PROF_REGFILE);
	tree->ent.tpset[TPSET_DIRFILE] = reiser4_profile_plug(PROF_DIRFILE);
	tree->ent.tpset[TPSET_SYMFILE] = reiser4_profile_plug(PROF_SYMFILE);
	tree->ent.tpset[TPSET_SPLFILE] = reiser4_profile_plug(PROF_SPLFILE);

	/* Init other tpset plugins. */
#ifndef ENABLE_MINIMAL
	tree->ent.tpset[TPSET_NODE] = reiser4_profile_plug(PROF_NODE);
	tree->ent.tpset[TPSET_NODEPTR] = reiser4_profile_plug(PROF_NODEPTR);
#endif

	/* These plugins should be initialized at the tree init. Later they can 
	   be reinitialized with the root directory pset or anything else. */

	/* Build the param pset mask, keep it in the tree instance. */
	for (i = 0; i < OPSET_LAST; i++) {
		if (defprof.pid[opset_prof[i].prof].id.type == PARAM_PLUG_TYPE)
			tree->ent.param_mask |= (1 << i);
	}
	
	return 0;
}

#ifndef ENABLE_MINIMAL
errno_t reiser4_opset_tree(reiser4_tree_t *tree, int check) {
	uint64_t mask;
	int i;
#else
errno_t reiser4_opset_tree(reiser4_tree_t *tree) {
#endif
	reiser4_object_t *object;
	
	aal_assert("vpf-1624", tree != NULL);

	if (!(object = reiser4_object_obtain(tree, NULL, &tree->key))) {
		aal_error("Failed to initialize the fs-global object "
			  "plugin set: failed to open the root directory.");
		return -EINVAL;
	}
	
	aal_memcpy(tree->ent.opset, object->info.opset.plug, 
		   sizeof(reiser4_plug_t *) * OPSET_LAST);

	tree->ent.opset[OPSET_OBJ] = NULL;
	tree->ent.opset[OPSET_DIR] = NULL;
	
#ifndef ENABLE_MINIMAL
	mask = object->info.opset.plug_mask;
#endif

	reiser4_object_close(object);

#ifndef ENABLE_MINIMAL
	/* Check that all 'on-disk' plugins are obtained. */
	for (i = 0; i < OPSET_STORE_LAST; i++) {
		/* If root should not be checked (debugfs), skip the loop. */
		if (!check)
			continue;
		
		if (mask & (1 << i)) {
			/* Set in mask & initialized. */
			if (tree->ent.opset[i])
				continue;
			
			/* Set in mask & PARAMETER. */
			if (defprof.pid[opset_prof[i].prof].id.type == 
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
	for (; i < OPSET_LAST; i++) {
		if (tree->ent.opset[i])
			continue;

		tree->ent.opset[i] = reiser4_profile_plug(opset_prof[i].prof);
	}
#endif

	return 0;
}
