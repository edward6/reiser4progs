/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   profile.c -- reiser4 profile functions. */

#ifndef ENABLE_STAND_ALONE

#include <reiser4/libreiser4.h>

/* Opset type -> Plugin type. INVAL_PID means that the slot is valid,
   but the library does not have any plugin written for it yet. See
   reiser4_opset_plug. */
rid_t opset_type[OPSET_LAST] = {
	[OPSET_OBJ] = OBJECT_PLUG_TYPE,
	[OPSET_DIR] = INVAL_PID,
	[OPSET_PERM] = INVAL_PID,
	[OPSET_POLICY] = POLICY_PLUG_TYPE,
	[OPSET_HASH] = HASH_PLUG_TYPE,
	[OPSET_FIBRE] = FIBRE_PLUG_TYPE,
	[OPSET_STAT] = ITEM_PLUG_TYPE,
	[OPSET_DENTRY] = ITEM_PLUG_TYPE,
	[OPSET_CRYPTO] = INVAL_PID,
	[OPSET_DIGEST] = INVAL_PID,
	[OPSET_COMPRES] = INVAL_PID,
	
	[OPSET_CREATE] = OBJECT_PLUG_TYPE,
	[OPSET_MKDIR] = OBJECT_PLUG_TYPE,
	[OPSET_SYMLINK] = OBJECT_PLUG_TYPE,
	[OPSET_MKNODE] = OBJECT_PLUG_TYPE,
	
	[OPSET_TAIL] = ITEM_PLUG_TYPE,
	[OPSET_EXTENT] = ITEM_PLUG_TYPE,
	[OPSET_ACL] = INVAL_PID
};

/* Opset type -> profile type. INVAL_PID means that there is no such a profile 
   slot or it is invalid (there is no such plugins written in the libreiser4).*/
rid_t opset_prof[OPSET_LAST] = {
	[OPSET_OBJ] = INVAL_PID,
	[OPSET_DIR] = INVAL_PID,
	[OPSET_PERM] = INVAL_PID,
	[OPSET_POLICY] = PROF_POLICY,
	[OPSET_HASH] = PROF_HASH,
	[OPSET_FIBRE] = PROF_FIBRE,
	[OPSET_STAT] = PROF_STAT,
	[OPSET_DENTRY] = PROF_DENTRY,
	[OPSET_CRYPTO] = INVAL_PID,
	[OPSET_DIGEST] = INVAL_PID,
	[OPSET_COMPRES] = INVAL_PID,

	[OPSET_CREATE] = PROF_REG,
	[OPSET_MKDIR] = PROF_DIR,
	[OPSET_SYMLINK] = PROF_SYM,
	[OPSET_MKNODE] = PROF_SPL,
	
	[OPSET_TAIL] = PROF_TAIL,
	[OPSET_EXTENT] = PROF_EXTENT,
	[OPSET_ACL] = INVAL_PID
};

/* Returns NULL if @member is valid but there is no written plugins in the 
   library for it yet (only @id == 0 are allowed). INVAL_PTR if plugin @id 
   should present for @member in the library but nothing is found. Othewise,
   returns the plugin pointer. */
reiser4_plug_t *reiser4_opset_plug(rid_t member, rid_t id) {
	reiser4_plug_t *plug;
	aal_assert("vpf-1613", member < OPSET_LAST);

	if (opset_type[member] == INVAL_PID && id == 0)
		return NULL;
	
	plug = reiser4_factory_ifind(opset_type[member], id);
	return plug == NULL ? INVAL_PTR : plug;
}

void reiser4_opset_root(reiser4_plug_t **opset) {
	uint8_t i;
	
	aal_assert("vpf-1639", opset != NULL);

	aal_memset(opset, 0, sizeof(reiser4_plug_t *) * OPSET_LAST);
	
	/* All plugins must present in the root. Get them from the profile
	   or set INVAL_PRT pointer. */
	for (i = 0; i < OPSET_LAST; i++) {
		opset[i] = opset_prof[i] == INVAL_PID ?
			INVAL_PTR : reiser4_profile_plug(opset_prof[i]);
	}
	
	/* Set the object plugin to the MKDIR fs-default plugin. */
	opset[OPSET_OBJ] = opset[OPSET_MKDIR];

	/* Directory plugin does not exist in progs at all. */
	opset[OPSET_DIR] = NULL;
}

errno_t reiser4_opset_init(reiser4_tree_t *tree) {
	reiser4_object_t *object;
	uint8_t i;
	
	aal_assert("vpf-1624", tree != NULL);

	if (!(object = reiser4_object_obtain(tree, NULL, &tree->key))) {
		aal_error("Failed to initialize the fs-global object "
			  "plugin set: failed to open the root directory.");
		return -EINVAL;
	}
	
	aal_memcpy(tree->ent.opset, object->ent->opset, 
		   sizeof(reiser4_plug_t *) * OPSET_LAST);
	
	reiser4_object_close(object);
	
	/* Check that all 'on-disk' plugins are obtained, set others 
	   from the profile.  */
	for (i = 0; i < OPSET_STORE_LAST; i++) {
		if (i != 1 && !tree->ent.opset[i]) {
			aal_error("The slot %u in the fs-global object "
				  "plugin set is not initialized.", i);
			return -EINVAL;
		}
	}

	for (i = OPSET_STORE_LAST; i < OPSET_LAST; i++) {
		if (!tree->ent.opset[i]) {
			tree->ent.opset[i] = (opset_prof[i] == INVAL_PID) ?
				NULL : reiser4_profile_plug(opset_prof[i]);
		}
	}
	
	return 0;
}

errno_t reiser4_pset_init(reiser4_tree_t *tree) {
	reiser4_plug_t *plug;
	uint16_t flags;
	char *name;
	
	aal_assert("vpf-1608", tree != NULL);
	aal_assert("vpf-1609", tree->fs != NULL);
	aal_assert("vpf-1610", tree->fs->format != NULL);

	/* Init the key plugin. */
	flags = plug_call(tree->fs->format->ent->plug->o.format_ops,
			  get_flags, tree->fs->format->ent);

	/* Hardcoded plugin names for 2 cases. */
	name = ((1 << REISER4_LARGE_KEYS) & flags)? "key_large" : "key_short";

	
	if (!(plug = reiser4_factory_nfind(name))) {
		aal_error("Can't find a plugin by the name \"%s\".", name);
		return -EINVAL;
	}

	if (plug->id.type != KEY_PLUG_TYPE) {
		aal_error("Wrong plugin of the type %u found by "
			  "the name \"%s\".", plug->id.type, name);
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
