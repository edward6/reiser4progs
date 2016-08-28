/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   pset.c -- reiser4 plugin set (&heir set) repair functions. */

#include <repair/librepair.h>

extern reiser4_profile_t defprof;
extern rid_t pset_prof[];

errno_t repair_pset_check_backup(backup_hint_t *hint) {
	struct reiser4_pset_backup *pset;
	int present;
	rid_t id;
	char *p;
	int i;
	
	aal_assert("vpf-1909", hint != NULL);
	
	p = hint->block.data + hint->off[BK_PSET];
	present = !aal_strncmp(p, PSET_MAGIC, aal_strlen(PSET_MAGIC));
	if ((hint->version == 0 && present == 1) || 
	    (hint->version > 0 && present == 0))
	{
		return RE_FATAL;
	}
	
	if (hint->version == 0)
		return 0;
	
	pset = (struct reiser4_pset_backup *)(p + aal_strlen(PSET_MAGIC));
	
	for (i = 0; i < PSET_STORE_LAST; i++) {
		id = aal_get_le32(pset, id[i]);
		
		if (id >= defprof.pid[pset_prof[i]].max) 
			return RE_FATAL;
	}
	
	hint->off[BK_PSET + 1] += sizeof(rid_t) * (PSET_STORE_LAST + 5);
	return 0;
}

errno_t repair_pset_root_check(reiser4_fs_t *fs, 
			       reiser4_object_t *root, 
			       uint8_t mode)
{
	struct reiser4_pset_backup *pset;
	rid_t id, root_id;
	int i;
	
	aal_assert("vpf-1910", fs != NULL);
	aal_assert("vpf-1911", root != NULL);
	
	if (!fs->backup)
		return 0;

	pset = (struct reiser4_pset_backup *)
		(fs->backup->hint.block.data + 
		 fs->backup->hint.off[BK_PSET] +
		 aal_strlen(PSET_MAGIC));
	
	/* Check that backed up pset matches the root one. */
	for (i = 0; i < PSET_STORE_LAST; i++) {
		id = aal_get_le32(pset, id[i]);
		
		root_id = fs->tree->ent.param_mask & (1 << i) ? 
			(unsigned long )root->info.pset.plug[i] :
			root->info.pset.plug[i]->id.id;
		
		if (root_id == id)
			continue;

		fsck_mess("The Plugin Set slot %u of the root directory is %u, "
			  "does not match the backed up value %u.%s", i, root_id, 
			  id, mode == RM_BUILD ? " Fixed." : "");
		
		if (mode != RM_BUILD)
			return RE_FATAL;
		
		if (fs->tree->ent.param_mask & (1 << i)) {
			root->info.pset.plug[i] = (void *)(unsigned long)id;
		} else {
			root->info.pset.plug[i] = reiser4_factory_ifind(
				defprof.pid[pset_prof[i]].id.type, id);

			aal_assert("vpf-1912", root->info.pset.plug[i] != NULL);
		}
	}
	
	return 0;
}
