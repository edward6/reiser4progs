/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#ifndef ENABLE_STAND_ALONE

#include <reiser4/libreiser4.h>

struct opset_member {
	/* Opset type -> Plugin type. INVAL_PID means that the slot is valid,
	   but the library does not have any plugin written for it yet. See
	   reiser4_opset_plug. */
	rid_t type;

	/* To be sure that a plugin found by type,id pair is valid, add the
	   group here. */
	rid_t group;
	
	/* Opset type -> profile type. INVAL_PID means that there is no such 
	   a profile slot or it is invalid (there is no such plugins written 
	   in the libreiser4).*/
	rid_t prof;

	/* If a plugin is essential or not. */
	bool_t ess;
};

typedef struct opset_member opset_member_t;

opset_member_t opset_prof[OPSET_LAST] = {
	[OPSET_OBJ] = {
		.type = INVAL_PID,
		.group = INVAL_PID,
		.prof = INVAL_PID,
		.ess = 0,
	},
	[OPSET_DIR] = {
		.type = INVAL_PID,
		.group = INVAL_PID,
		.prof = INVAL_PID,
		.ess = 0,
	},
	[OPSET_PERM] = {
		.type = INVAL_PID,
		.group = INVAL_PID,
		.prof = INVAL_PID,
		.ess = 1,
	},
	[OPSET_POLICY] = {
		.type = POLICY_PLUG_TYPE,
		.group = 0,
		.prof = PROF_POLICY,
		.ess = 0,
	},
	[OPSET_HASH] = {
		.type = HASH_PLUG_TYPE,
		.group = 0,
		.prof = PROF_HASH,
		.ess = 1,
	},
	[OPSET_FIBRE] = {
		.type = FIBRE_PLUG_TYPE,
		.group = 0,
		.prof = PROF_FIBRE,
		.ess = 1,
	},
	[OPSET_STAT] = {
		.type = ITEM_PLUG_TYPE,
		.group = STAT_ITEM,
		.prof = PROF_STAT,
		.ess = 0,
	},
	[OPSET_DIRITEM] = {
		.type = ITEM_PLUG_TYPE,
		.group = DIR_ITEM,
		.prof = PROF_DIRITEM,
		.ess = 1,
	},
	[OPSET_CRYPTO] = {
		.type = INVAL_PID,
		.group = INVAL_PID,
		.prof = INVAL_PID,
		.ess = 1,
	},
	[OPSET_DIGEST] = {
		.type = INVAL_PID,
		.group = INVAL_PID,
		.prof = INVAL_PID,
		.ess = 1,
	},
	[OPSET_COMPRES] = {
		.type = INVAL_PID,
		.group = INVAL_PID,
		.prof = INVAL_PID,
		.ess = 1,
	},
	
	/* Note, plugins below are not stored on-disk. */

	/* The 4 plugins below needs to be splited -- for now they are used for
	   new created objects and for already created Reg/Dir/Sym/Spc files. 
	   If the former ones are non-essential, the other 4 are essential. */
	[OPSET_CREATE] = {
		.type = OBJECT_PLUG_TYPE,
		.group = REG_OBJECT,
		.prof = PROF_REG,
		.ess = 1,
	},
	[OPSET_MKDIR] = {
		.type = OBJECT_PLUG_TYPE,
		.group = DIR_OBJECT,
		.prof = PROF_DIR,
		.ess = 1,
	},
	[OPSET_SYMLINK] = {
		.type = OBJECT_PLUG_TYPE,
		.group = SYM_OBJECT,
		.prof = PROF_SYM,
		.ess = 1,
	},
	[OPSET_MKNODE] = {
		.type = OBJECT_PLUG_TYPE,
		.group = SPL_OBJECT,
		.prof = PROF_SPL,
		.ess = 1,
	},
	[OPSET_TAIL] = {
		.type = ITEM_PLUG_TYPE,
		.group = TAIL_ITEM,
		.prof = PROF_TAIL,
		.ess = 1,
	},
	[OPSET_EXTENT] = {
		.type = ITEM_PLUG_TYPE,
		.group = EXTENT_ITEM,
		.prof = PROF_EXTENT,
		.ess = 1,
	},
	[OPSET_ACL] = {
		.type = INVAL_PID,
		.group = INVAL_PID,
		.prof = INVAL_PID,
		.ess = 0,
	}
};

/* Returns NULL if @member is valid but there is no written plugins in the 
   library for it yet (only @id == 0 are allowed). INVAL_PTR if plugin @id 
   should present for @member in the library but nothing is found. Othewise,
   returns the plugin pointer. */
reiser4_plug_t *reiser4_opset_plug(rid_t member, rid_t id) {
	reiser4_plug_t *plug;
	aal_assert("vpf-1613", member < OPSET_LAST);

	if (opset_prof[member].type == INVAL_PID && id == 0)
		return NULL;
	
	if (!(plug = reiser4_factory_ifind(opset_prof[member].type, id)))
		return INVAL_PTR;
	
	if (plug->id.group != opset_prof[member].group)
		return INVAL_PTR;
	
	return plug;
}

void reiser4_opset_root(reiser4_opset_t *opset) {
	uint8_t i;
	
	aal_assert("vpf-1639", opset != NULL);

	/* All plugins must present in the root. Get them from the profile. */
	for (i = 0; i < OPSET_LAST; i++) {
		if (opset->plug[i])
			continue;
		
		opset->plug[i] = opset_prof[i].prof == INVAL_PID ? 
			NULL : reiser4_profile_plug(opset_prof[i].prof);
	}
	
	/* Set the object plugin to the MKDIR fs-default plugin. */
	if (!opset->plug[OPSET_OBJ])
		opset->plug[OPSET_OBJ] = opset->plug[OPSET_MKDIR];

	/* Directory plugin does not exist in progs at all. */
	opset->plug[OPSET_DIR] = NULL;
}

errno_t reiser4_opset_init(reiser4_tree_t *tree, int check) {
	reiser4_object_t *object;
	uint8_t i;
	
	aal_assert("vpf-1624", tree != NULL);

	if (!(object = reiser4_object_obtain(tree, NULL, &tree->key))) {
		aal_error("Failed to initialize the fs-global object "
			  "plugin set: failed to open the root directory.");
		return -EINVAL;
	}
	
	aal_memcpy(tree->ent.opset, object->ent->opset.plug, 
		   sizeof(reiser4_plug_t *) * OPSET_LAST);
	
	reiser4_object_close(object);
	
	/* Check that all 'on-disk' plugins are obtained. */
	for (i = 0; i < OPSET_STORE_LAST; i++) {
		/* If rot is should not be checked (debugreiserfs), 
		   skip this loop. */
		if (!check)
			break;

		if (!tree->ent.opset[i] && opset_prof[i].prof != INVAL_PID) {
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
	
	return 0;
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
	
	for (i= 0; i < OPSET_LAST; i++) {
		/* Leave non-essential members as is. */
		if (!opset_prof[i].ess)
			continue;

		/* Only not fs-default essential plugins are stored. */
		if (tree->ent.opset[i] != opset->plug[i]) {
			opset->mask |= (1 << i);
			continue;
		}


		/* Remove from SD existent essential plugins that match 
		   the fs-defaul ones. */
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

errno_t reiser4_pset_init(reiser4_tree_t *tree) {
	reiser4_plug_t *plug;
	uint16_t flags;
	rid_t pid;
	
	aal_assert("vpf-1608", tree != NULL);
	aal_assert("vpf-1609", tree->fs != NULL);
	aal_assert("vpf-1610", tree->fs->format != NULL);

	/* Init the key plugin. */
	flags = plug_call(tree->fs->format->ent->plug->o.format_ops,
			  get_flags, tree->fs->format->ent);

	/* Hardcoded plugin ids for 2 cases. */
	pid = ((1 << REISER4_LARGE_KEYS) & flags)? KEY_LARGE_ID : KEY_SHORT_ID;
	
	if (!(plug = reiser4_factory_ifind(KEY_PLUG_TYPE, pid))) {
		aal_error("Can't find a key plugin by its id %d.", pid);
		return -EINVAL;
	}

	tree->ent.tpset[TPSET_KEY] = plug;

	/* Init other tpset plugins. */
	tree->ent.tpset[TPSET_NODE] = reiser4_profile_plug(PROF_NODE);
	tree->ent.tpset[TPSET_NODEPTR] = reiser4_profile_plug(PROF_NODEPTR);
	
	/* These plugins should be initialized at the tree init. Later they can 
	   be reinitialized with the root directory pset or anything else. */
	tree->ent.opset[OPSET_CREATE] = reiser4_profile_plug(PROF_REG);
	tree->ent.opset[OPSET_MKDIR] = reiser4_profile_plug(PROF_DIR);
	tree->ent.opset[OPSET_SYMLINK] = reiser4_profile_plug(PROF_SYM);
	tree->ent.opset[OPSET_MKNODE] = reiser4_profile_plug(PROF_SPL);
	
	return 0;
}


#endif
