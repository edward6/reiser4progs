/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ccreg40_repair.c -- reiser4 crypto-compression regular file plugin
   repair code. */
 
#ifndef ENABLE_MINIMAL

#include <aux/aux.h>
#include <misc/misc.h>
#include "ccreg40.h"
#include "plugin/object/obj40/obj40_repair.h"

static int ccreg40_check_size(reiser4_object_t *cc, 
			      uint64_t *sdsize, 
			      uint64_t counted_size)
{
	return 0;
}

typedef struct ccreg40_hint {
	obj40_stat_hint_t stat;

	/* Item seek, found and next item offsets. */
	uint64_t prev_found; /* (key) offset found in the previous iteration */
	uint64_t seek;       /* expected offset for lookup */
	uint64_t found;      /* what has been really found */
	uint64_t maxreal;    /* maximal (key) offset in the found item */
	uint64_t sdsize;
	/* The following two fields are to delete wrecks of a disk cluster */
	uint64_t cut_from;   /* offset to cut from */
	uint32_t cut_size;   /* how many bytes to cut */
	uint32_t clsize;
	/* Total number of units in a disk cluster */
	uint32_t bytes;
	uint32_t adler;
	uint8_t mode;
	/* Indicates if cluster has a hole in key space */
	uint8_t hole;
	/* The cluster size & the buffer for the data. */
	uint8_t data[64 * 1024];
} ccreg40_hint_t;

static errno_t ccreg40_check_item(reiser4_object_t *cc, void *data) {
	ccreg40_hint_t *hint = (ccreg40_hint_t *)data;
	uint8_t shift;
	errno_t result;

	hint->found = objcall(&cc->body.key, get_offset);
	hint->maxreal = obj40_place_maxreal(&cc->body);
	
	aal_assert("vpf-1871", hint->maxreal >= hint->found);
	aal_assert("vpf-1872", hint->seek <= hint->found);
	
	/* check item plugin */
	if (cc->body.plug != reiser4_psctail(cc)) {
		fsck_mess("Found item of illegal plugin (%s) "
			  "with the key of this object ",
			  cc->body.plug->p.label);
		goto fatal;
	}
	/* check cluster shift. */
	result = ccreg40_get_cluster_shift(&cc->body, &shift);
	if (result < 0)
		return result;
	if (shift == UNPREPPED_CLUSTER_SHIFT) {
		fsck_mess("Found unprepped disk cluster ");
		goto fatal;
	}
	if (shift < MIN_VALID_CLUSTER_SHIFT ||
	    shift > MAX_VALID_CLUSTER_SHIFT ||
	    shift != misc_log2(hint->clsize)) {
		fsck_mess("Found item with wrong cluster shift %d, "
			  "should be %d", shift, misc_log2(hint->clsize));
		goto fatal;
	}
	if (hint->seek &&
	    !ccreg40_clsame(hint->prev_found, hint->found, hint->clsize) &&
	    ccreg40_cloff(hint->found, hint->clsize) != 0){
		fsck_mess("Found item of lenght (%llu) which has wrong "
			  "offset %llu, should be a multiple of logical "
			  "cluster size ",
			  hint->maxreal - hint->found + 1, hint->found);
		goto fatal;
	}
	if (!ccreg40_clsame(hint->found, hint->maxreal, hint->clsize)) {
		fsck_mess("Found item of length %llu and offset %llu, "
			  "which contains logical cluster boundary ",
			  hint->maxreal - hint->found + 1, hint->found);
		goto fatal;
	}
	return 0;
 fatal:
	fsck_mess("(file [%s] (%s), node [%llu], item [%u]). %s",
		  print_inode(obj40_core, &cc->info.object),
		  reiser4_psobj(cc)->p.label,
		  place_blknr(&cc->body),
		  cc->body.pos.item,
		  hint->mode == RM_BUILD ? " Removed." : "");
	hint->cut_from = hint->found;
	hint->cut_size = hint->maxreal - hint->found + 1;

	return hint->mode == RM_BUILD ? -ESTRUCT : RE_FATAL;
}

static int64_t ccreg40_read_item(reiser4_place_t *place, ccreg40_hint_t *hint) {
	trans_hint_t trans;
	uint64_t offset;
	int64_t count;

	aal_assert("vpf-1870", place->pos.unit == 0 || 
		               place->pos.unit == MAX_UINT32);
	
	/* Read the data. */
	offset = hint->found % hint->clsize;
	trans.count = hint->maxreal - hint->found + 1;
	trans.specific = hint->data + offset;
	
	if ((count = objcall(place, object->read_units, &trans)) < 0)
		return count;
	
	return 0;
}

static errno_t ccreg40_check_crc(ccreg40_hint_t *hint) {
	uint32_t calc, found, offset;
	uint32_t *disk;

	aal_assert("edward-2", hint->bytes > sizeof(uint32_t));

	offset = hint->bytes - sizeof(uint32_t);

	calc = aux_adler32(0, (char *)hint->data, offset);
	disk = (uint32_t *)(hint->data + offset);
	found = LE32_TO_CPU(*disk);

	return calc == found ? 0 : RE_FATAL;
}

/*
 * Read a found item to the stream.
 * Check a checksum, if the previous iteration completed a disk cluster.
 */
static errno_t ccreg40_check_cluster(reiser4_object_t *cc, 
				     ccreg40_hint_t *hint,
				     uint8_t mode) 
{
	errno_t result = 0;
	errno_t res;
	uint32_t lcl_bytes;

	if ((cc->body.plug == NULL) ||
	    (hint->seek != 0 &&
	     !ccreg40_clsame(hint->prev_found, hint->found, hint->clsize))) {
		/* Cluster is over */

		if (hint->prev_found > hint->sdsize) {
			/* cluster is orphan */
			hint->bytes = 0;
			result = RE_FATAL;

			/* set offset of the cluster to be deleted. */
			hint->cut_from = ccreg40_clstart(hint->prev_found,
							 hint->clsize);
			fsck_mess("The file [%s] (%s): the cluster at [%llu] "
				  "offset %u bytes long is orphan.%s",
				  print_inode(obj40_core, &cc->info.object),
				  reiser4_psobj(cc)->p.label, hint->cut_from,
				  hint->clsize, hint->mode != RM_CHECK ?
				  " Removed." : "");
		}
		/**
		 * If there still is a hole in the keyspace, then
		 * check a checksum (no such hole means that no
		 * checksum was appended)
		 */
		else if (hint->bytes && hint->hole && ccreg40_check_crc(hint)) {
			/* wrong checksum */
			hint->bytes = 0;
			result = RE_FATAL;

			/* set offset of the cluster to be deleted. */
			hint->cut_from = ccreg40_clstart(hint->prev_found,
							 hint->clsize);
			fsck_mess("The file [%s] (%s): the cluster at [%llu] "
				  "offset %u bytes long is corrupted.%s",
				  print_inode(obj40_core, &cc->info.object),
				  reiser4_psobj(cc)->p.label, hint->cut_from,
				  hint->clsize, hint->mode != RM_CHECK ? 
				  " Removed." : "");
		}
		/* Fini all the data related to the previous cluster. */
		hint->stat.bytes += hint->bytes;
		hint->bytes = 0;
		hint->adler = 0;

		if (!cc->body.plug)
			/* finish with this object */
			return result;

		/* Update the cluster data. */
		aal_memset(hint->data, 0, hint->clsize);
	}
	/* An item found. */

	if ((res = ccreg40_read_item(&cc->body, hint)))
		return res;

	hint->prev_found = hint->found;
	hint->bytes += objcall(&cc->body, object->bytes);
	/**
	 * Calculate actual number of file's bytes in the
	 * logical cluster and figure out, if corresponding
	 * disk cluster has a hole in key space.
	 *
	 * We need this to figure out if disk cluster contains
	 * appended checksum.
	 */
	lcl_bytes = 0;
	if (ccreg40_clsame(hint->found, hint->sdsize - 1, hint->clsize))
		lcl_bytes = hint->sdsize % hint->clsize;
	if (lcl_bytes == 0)
		lcl_bytes = hint->clsize;
	hint->hole = (hint->bytes != lcl_bytes);

	return result;
}

errno_t ccreg40_check_struct(reiser4_object_t *cc, 
			     place_func_t func,
			     void *data, uint8_t mode)
{
	object_info_t *info;
	ccreg40_hint_t hint;
	trans_hint_t trans;
	errno_t res;

	aal_assert("vpf-1829", cc != NULL);
	aal_assert("vpf-1836", cc->info.tree != NULL);

	info = &cc->info;
	aal_memset(&hint, 0, sizeof(hint));
	
	if ((res = obj40_prepare_stat(cc, S_IFREG, mode)))
		return res;
	
	/* Try to register SD as an item of this file. */
	if (func && func(&info->start, data))
		return -EINVAL;
	
	res = 0;
	hint.mode = mode;
	hint.clsize = reiser4_pscluster(cc)->clsize;
	hint.sdsize = obj40_get_size(cc);

	while(1) {
		lookup_t lookup;
		
		/* Get next item. */
		lookup = obj40_check_item(cc, ccreg40_check_item, NULL, &hint);
		
		if (repair_error_fatal(lookup)) {
			    if ((lookup & RE_FATAL) && mode == RM_BUILD) {
				    /*
				     * Delete item found in this iteration
				     */
				    res &= ~RE_FATAL;
				    res |= obj40_cut(cc, &trans, hint.cut_from,
						     hint.cut_size, NULL, NULL);
				    if (res < 0)
					    return res;
				    hint.seek = hint.maxreal + 1;
				    obj40_seek(cc, hint.seek);
				    continue;
			    }
			    else
				    return lookup;
		}
		else if (lookup == ABSENT)
			cc->body.plug = NULL;
		
		/* Register the item. */
		if (cc->body.plug && func && func(&cc->body, data)) {
			aal_bug("vpf-1869", "The item [%s] should not be "
				"registered yet.", print_key(obj40_core, 
							     &info->object));
		}
		/* check cluster found in previous iteration */
		if ((res |= ccreg40_check_cluster(cc, &hint, mode)) < 0)
			return res;

		if (res & RE_FATAL) {
			/*
			 * Delete cluster found in previous iteration
			 */
			if (mode == RM_BUILD) {
				res &= ~RE_FATAL;
				res |= obj40_cut(cc, &trans, hint.cut_from,
						 hint.clsize, NULL, NULL);
				if (res < 0)
					return res;
			}
		}
		
		/* If the file size is the max possible one, break out 
		   here to not seek to 0. */
		if (!cc->body.plug || hint.maxreal == MAX_UINT64)
			break;
		
		hint.seek = hint.maxreal + 1;
		obj40_seek(cc, hint.seek);
	}

	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL)) {
		obj40_stat_ops_t ops;
		
		aal_memset(&ops, 0, sizeof(ops));
		
		ops.check_size = ccreg40_check_size;
		ops.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;
		/**
		 * don't report about wrong bytes for ccreg40
		 * objects, as in most cases it is because kernel
		 * doesn't support i_blocks and i_bytes for such
		 * objects because of performance issues.
		 */
		ops.check_bytes = mode == RM_CHECK ? SKIP_METHOD : 0;
		ops.check_bytes_report = SKIP_METHOD;
		
		hint.stat.mode = S_IFREG;
		hint.stat.size = objcall(&cc->position, get_offset);

		res |= obj40_update_stat(cc, &ops, &hint.stat, mode);
	}
	
	obj40_reset(cc);

	return res;
}

#endif

/*
  Local variables:
  c-indentation-style: "K&R"
  mode-name: "LC"
  c-basic-offset: 8
  tab-width: 8
  fill-column: 80
  scroll-step: 1
  End:
*/
