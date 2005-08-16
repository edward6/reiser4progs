/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#include <reiser4/libreiser4.h>

#define INVAL_TYPE		((uint8_t)~0)

typedef struct opset_member {
	/* Opset type -> Plugin type. INVAL_PID means that the slot is valid,
	   but the library does not have any plugin written for it yet. See
	   reiser4_opset_plug. */
	uint8_t type;
	
#ifndef ENABLE_MINIMAL
	/* Opset type -> profile type. INVAL_PID means that there is no such 
	   a profile slot or it is invalid (there is no such plugins written 
	   in the libreiser4).*/
	rid_t prof;
#endif
} opset_member_t;

opset_member_t opset_prof[OPSET_LAST] = {
	[OPSET_OBJ] = {
		.type = OBJECT_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = INVAL_PID,
#endif
	},
	[OPSET_DIR] = {
		.type = INVAL_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = INVAL_PID,
#endif
	},
	[OPSET_PERM] = {
		.type = INVAL_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = INVAL_PID,
#endif
	},
	[OPSET_POLICY] = {
		.type = POLICY_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_POLICY,
#endif
	},
	[OPSET_HASH] = {
		.type = HASH_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_HASH,
#endif
	},
	[OPSET_FIBRE] = {
		.type = FIBRE_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_FIBRE,
#endif
	},
	[OPSET_STAT] = {
		.type = ITEM_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_STAT,
#endif
	},
	[OPSET_DIRITEM] = {
		.type = ITEM_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_DIRITEM,
#endif
	},
	[OPSET_CRYPTO] = {
		.type = CRYPTO_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_CRYPTO,
#endif
	},
	[OPSET_DIGEST] = {
		.type = PARAM_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = INVAL_PID,
#endif
	},
	[OPSET_COMPRESS] = {
		.type = COMPRESS_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_COMPRESS,
#endif
	},
	[OPSET_COMPRESS_MODE] = {
		.type = INVAL_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = INVAL_PID,
#endif
	},
	[OPSET_CLUSTER] = {
		.type = PARAM_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = INVAL_PID,
#endif
	},

	/* The 4 plugins below are non-essential ones. They are used to understand
	   what object plugin should be used for create/mkdir/monode/symlink syscalls
	   for children whithin the object. */
	[OPSET_CREATE] = {
		.type = OBJECT_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_CREATE,
#endif
	},

	/* Note, plugins below are not stored on-disk. */
	
	[OPSET_MKDIR] = {
		.type = OBJECT_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_MKDIR,
#endif
	},
	[OPSET_SYMLINK] = {
		.type = OBJECT_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_SYMLINK,
#endif
	},
	[OPSET_MKNODE] = {
		.type = OBJECT_PLUG_TYPE,
#ifndef ENABLE_MINIMAL
		.prof = PROF_MKNODE,
#endif
	},
#ifndef ENABLE_MINIMAL
	[OPSET_TAIL] = {
		.type = ITEM_PLUG_TYPE,
		.prof = PROF_TAIL,
	},
	[OPSET_EXTENT] = {
		.type = ITEM_PLUG_TYPE,
		.prof = PROF_EXTENT,
	}
#endif
};

/* Returns NULL if @member is valid but there is no written plugins in the 
   library for it yet (only @id == 0 are allowed). INVAL_PTR if plugin @id 
   should present for @member in the library but nothing is found. Othewise,
   returns the plugin pointer. */
reiser4_plug_t *reiser4_opset_plug(rid_t member, rid_t id) {
	reiser4_plug_t *plug;
	aal_assert("vpf-1613", member < OPSET_LAST);

	if (opset_prof[member].type == INVAL_TYPE && id == 0)
		return NULL;
	
	if (opset_prof[member].type == PARAM_PLUG_TYPE)
		return NULL;
	
	if (!(plug = reiser4_factory_ifind(opset_prof[member].type, id)))
		return INVAL_PTR;
	
	return plug;
}

#ifndef ENABLE_MINIMAL
void reiser4_opset_profile(reiser4_plug_t **opset) {
	uint8_t i;
	
	aal_assert("vpf-1639", opset != NULL);

	/* All plugins must present in the root. Get them from the profile. */
	for (i = 0; i < OPSET_LAST; i++) {
		if (opset[i] != NULL)
			continue;
		
		opset[i] = opset_prof[i].prof == INVAL_PID ? 
			NULL : reiser4_profile_plug(opset_prof[i].prof);
	}
	
	/* Directory plugin does not exist in progs at all. */
	opset[OPSET_DIR] = NULL;
}

void reiser4_opset_diff(reiser4_tree_t *tree, reiser4_opset_t *opset) {
	uint8_t i;
	
	aal_assert("vpf-1644", tree != NULL);
	aal_assert("vpf-1646", opset != NULL);

	if (!tree->ent.opset[OPSET_HASH]) {
		/* The special case -- the root directory. 
		   All opset members must be stored. */
		opset->mask = (1 << OPSET_LAST) - 1;
		opset->mask &= ~(1 << OPSET_DIR);
		return;
	}
	
	for (i = OPSET_DIR + 1; i < OPSET_LAST; i++) {
		/* Only not fs-default essential plugins are stored. */
		if (tree->ent.opset[i] != opset->plug[i]) {
			opset->mask |= (1 << i);
			continue;
		}


		/* Remove from SD existent essential plugins that match 
		   the fs-default ones. */
		if (opset->mask & (1 << i)) {
			opset->mask &= ~(1 << i);
			opset->plug[i] = INVAL_PTR;
			continue;
		}

		/* All present Non-Essential plugins are left on disk. */
		opset->mask &= ~(1 << i);
		opset->plug[i] = NULL;
	}
}

#endif

errno_t reiser4_pset_init(reiser4_tree_t *tree) {
	reiser4_plug_t *plug;
	rid_t pid;
	
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
	
	/* These plugins should be initialized at the tree init. Later they can 
	   be reinitialized with the root directory pset or anything else. */
	tree->ent.opset[OPSET_CREATE] = reiser4_profile_plug(PROF_CREATE);
	tree->ent.opset[OPSET_MKDIR] = reiser4_profile_plug(PROF_MKDIR);
	tree->ent.opset[OPSET_SYMLINK] = reiser4_profile_plug(PROF_SYMLINK);
	tree->ent.opset[OPSET_MKNODE] = reiser4_profile_plug(PROF_MKNODE);
#else
	if (!(tree->ent.tpset[TPSET_NODE] =
	      reiser4_factory_ifind(NODE_PLUG_TYPE, NODE_REISER40_ID)))
	{
		aal_bug("vpf-1805", "Failed to find a plugin type (%u), id(%u)",
			NODE_PLUG_TYPE, NODE_REISER40_ID);
	}
		
	if (!(tree->ent.tpset[TPSET_NODEPTR] =
	      reiser4_factory_ifind(ITEM_PLUG_TYPE, ITEM_NODEPTR40_ID)))
	{
		aal_bug("vpf-1806", "Failed to find a plugin type (%s), id(%u)",
			ITEM_PLUG_TYPE, ITEM_NODEPTR40_ID);
	}

	if (!(tree->ent.opset[OPSET_CREATE] =
	      reiser4_factory_ifind(OBJECT_PLUG_TYPE, OBJECT_REG40_ID)))
	{
		aal_bug("vpf-1801", "Failed to find a plugin type (%s), id(%u)",
			OBJECT_PLUG_TYPE, OBJECT_REG40_ID);
	}

	if (!(tree->ent.opset[OPSET_MKDIR] =
	      reiser4_factory_ifind(OBJECT_PLUG_TYPE, OBJECT_DIR40_ID)))
	{
		aal_bug("vpf-1802", "Failed to find a plugin type (%s), id(%u)",
			OBJECT_PLUG_TYPE, OBJECT_DIR40_ID);
	}
	
	if (!(tree->ent.opset[OPSET_SYMLINK] =
	      reiser4_factory_ifind(OBJECT_PLUG_TYPE, OBJECT_SYM40_ID)))
	{
		aal_bug("vpf-1803", "Failed to find a plugin type (%s), id(%u)",
			OBJECT_PLUG_TYPE, OBJECT_SYM40_ID);
	}
	
	if (!(tree->ent.opset[OPSET_MKNODE] =
	      reiser4_factory_ifind(OBJECT_PLUG_TYPE, OBJECT_SPL40_ID)))
	{
		aal_bug("vpf-1804", "Failed to find a plugin type (%s), id(%u)",
			OBJECT_PLUG_TYPE, OBJECT_SPL40_ID);
	}
#endif

	return 0;
}

#ifndef ENABLE_MINIMAL
errno_t reiser4_opset_init(reiser4_tree_t *tree, int check) {
	int i;
#else
errno_t reiser4_opset_init(reiser4_tree_t *tree) {
#endif
	reiser4_object_t *object;
	
	aal_assert("vpf-1624", tree != NULL);

	if (!(object = reiser4_object_obtain(tree, NULL, &tree->key))) {
		aal_error("Failed to initialize the fs-global object "
			  "plugin set: failed to open the root directory.");
		return -EINVAL;
	}
	
	aal_memcpy(tree->ent.opset, object->ent->opset.plug, 
		   sizeof(reiser4_plug_t *) * OPSET_LAST);
	
	reiser4_object_close(object);

#ifndef ENABLE_MINIMAL
	/* Check that all 'on-disk' plugins are obtained. */
	for (i = 0; i < OPSET_STORE_LAST; i++) {
		/* If root should not be checked (debugreiserfs), 
		   skip this loop. */
		if (!check)
			break;

		if (!tree->ent.opset[i] && 
		    opset_prof[i].type != INVAL_TYPE &&
		    opset_prof[i].type != PARAM_PLUG_TYPE) 
		{
			aal_error("The slot %u in the fs-global object "
				  "plugin set is not initialized.", i);
			return -EINVAL;
		}
	}

	/* Set others from the profile. */
	for (; i < OPSET_LAST; i++) {
		if (!tree->ent.opset[i] && opset_prof[i].prof != INVAL_PID)
			tree->ent.opset[i] = reiser4_profile_plug(opset_prof[i].prof);
	}
#endif

	return 0;
}

