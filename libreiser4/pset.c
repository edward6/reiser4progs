/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#include <reiser4/libreiser4.h>

extern reiser4_profile_t defprof;

int opset_prof[OPSET_LAST] = {
	[OPSET_OBJ]	= PROF_OBJ,
	[OPSET_DIR]	= PROF_DIR,
	[OPSET_PERM]	= PROF_PERM,
	[OPSET_POLICY]	= PROF_POLICY,
	[OPSET_HASH]	= PROF_HASH,
	[OPSET_FIBRE]	= PROF_FIBRE,
	[OPSET_STAT]	= PROF_STAT,
	[OPSET_DIRITEM] = PROF_DIRITEM,
	[OPSET_CRYPTO]	= PROF_CRYPTO,
	[OPSET_DIGEST]	= PROF_DIGEST,
	[OPSET_COMPRESS]= PROF_COMPRESS,
	[OPSET_CMODE]	= PROF_CMODE,
	[OPSET_CLUSTER] = PROF_CLUSTER,
	[OPSET_CREATE]	= PROF_CREATE,
	[OPSET_REGFILE]	= PROF_REGFILE,
	[OPSET_DIRFILE]	= PROF_DIRFILE,
	[OPSET_SYMFILE]	= PROF_SYMFILE,
	[OPSET_SPLFILE]	= PROF_SPLFILE,
#ifndef ENABLE_MINIMAL
	[OPSET_TAIL]	= PROF_TAIL,
	[OPSET_EXTENT]	= PROF_EXTENT,
#endif
};

#ifndef ENABLE_MINIMAL
static int pset_tree_isready(reiser4_tree_t *tree) {
	return tree->ent.opset[OPSET_HASH] != NULL;
}
#endif

void reiser4_opset_complete(reiser4_tree_t *tree, reiser4_opset_t *opset) {
	int i;
	
#ifndef ENABLE_MINIMAL
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
	
	if (defprof.pid[opset_prof[member]].id.type == PARAM_PLUG_TYPE) {
#ifndef ENABLE_MINIMAL
		return id < defprof.pid[opset_prof[member]].max ? 
			NULL : INVAL_PTR;
#else
		return NULL;
#endif
	}
	
	plug = reiser4_factory_ifind(
		defprof.pid[opset_prof[member]].id.type, id);
	
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
		
		if (defprof.pid[opset_prof[i]].id.id == INVAL_PID) {
			if (i == OPSET_OBJ) {
				/* Special case: root file plug. */
				opset->plug[i] = 
					reiser4_profile_plug(PROF_DIRFILE);
			} else {			
				/* Skip root dir plug & others with INVAL id. */
				continue;
			}
		} else if (defprof.pid[opset_prof[i]].id.type == 
			   PARAM_PLUG_TYPE) 
		{
			opset->plug[i] = 
				(void *)defprof.pid[opset_prof[i]].id.id;
			
		} else {
			opset->plug[i] = reiser4_profile_plug(opset_prof[i]);
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

		/* Skip the DIR flag. */
		mask &= ~(1 << OPSET_DIR);
		return mask;
	}
	
	for (i = 0; i < OPSET_STORE_LAST; i++) {
		/* If both are initialized, store if do not match */
		if (tree->ent.opset[i] != opset->plug[i])
			mask |= (1 << i);
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
	pid = plug_call(tree->fs->format->ent->plug->o.format_ops,
			key_pid, tree->fs->format->ent);

	if (!(plug = reiser4_factory_ifind(KEY_PLUG_TYPE, pid))) {
		aal_error("Can't find a key plugin by its id %d.", pid);
		return -EINVAL;
	}

	tree->ent.tpset[TPSET_KEY] = plug;

	/* Init other tpset plugins. */
#ifndef ENABLE_MINIMAL
	tree->ent.tpset[TPSET_NODE] = reiser4_profile_plug(PROF_NODE);
	tree->ent.tpset[TPSET_NODEPTR] = reiser4_profile_plug(PROF_NODEPTR);
#endif

	/* These plugins should be initialized at the tree init. Later they can 
	   be reinitialized with the root directory pset or anything else. */
	tree->ent.opset[OPSET_REGFILE] = reiser4_profile_plug(PROF_REGFILE);
	tree->ent.opset[OPSET_DIRFILE] = reiser4_profile_plug(PROF_DIRFILE);
	tree->ent.opset[OPSET_SYMFILE] = reiser4_profile_plug(PROF_SYMFILE);
	tree->ent.opset[OPSET_SPLFILE] = reiser4_profile_plug(PROF_SPLFILE);

	/* Build the param pset mask, keep it in the tree instance. */
	for (i = 0; i < OPSET_LAST; i++) {
		if (defprof.pid[opset_prof[i]].id.type == PARAM_PLUG_TYPE)
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

#ifndef ENABLE_MINIMAL
	mask = object->info.opset.plug_mask;
#endif

	reiser4_object_close(object);

#ifndef ENABLE_MINIMAL
	/* Check that all 'on-disk' plugins are obtained. */
	for (i = 0; i < OPSET_STORE_LAST; i++) {
		/* Zero not-defined slots in tree pset. */
		if (defprof.pid[opset_prof[i]].id.id == INVAL_PID) {
			tree->ent.opset[i] = NULL;
			continue;
		}
			
		/* If root should not be checked (debugfs), skip the loop. */
		if (!check)
			continue;
		
		if (mask & (1 << i)) {
			/* Set in mask & initialized. */
			if (tree->ent.opset[i])
				continue;
			
			/* Set in mask & PARAMETER. */
			if (defprof.pid[opset_prof[i]].id.type == 
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

		tree->ent.opset[i] = reiser4_profile_plug(opset_prof[i]);
	}
#endif

	return 0;
}
