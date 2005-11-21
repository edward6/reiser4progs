/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ccreg40_repair.c -- reiser4 crypto-compression regular file plugin
   repair code. */
 
#ifndef ENABLE_MINIMAL

#include <aux/aux.h>
#include "ccreg40_repair.h"

static int ccreg40_check_size(reiser4_object_t *cc, 
			      uint64_t *sdsize, 
			      uint64_t counted_size)
{
	return 0;
}

typedef struct ccreg40_hint {
	obj40_stat_hint_t stat;

	/* Item seek, found and next item offsets. */
	uint64_t seek;
	uint64_t found;
	uint64_t maxreal;

	/* Bytes all clusters takes on disk. */
	uint32_t bytes;
	uint64_t sdsize;
	uint32_t adler;
	uint8_t mode;

	/* If a hole is detected. */
	uint8_t hole;
	
	/* The cluster size & the buffer for the data. */
	uint8_t data[64 * 1024];
	uint64_t clstart;
	uint32_t clsize;
} ccreg40_hint_t;

static errno_t ccreg40_check_item(reiser4_object_t *cc, void *data) {
	ccreg40_hint_t *hint = (ccreg40_hint_t *)data;
	object_info_t *info;
	uint32_t clsize;
	errno_t res;
		
	hint->found = objcall(&cc->body.key, get_offset);
	hint->maxreal = obj40_place_maxreal(&cc->body);
	
	aal_assert("vpf-1871", hint->maxreal >= hint->found);
	aal_assert("vpf-1872", hint->seek <= hint->found);
	
	info = &cc->info;
	res = 0;
	
	/* Check the item plugin. */
	if ((reiser4_plug_t *)cc->body.plug != 
	    info->opset.plug[OPSET_CTAIL]) 
	{
		fsck_mess("The file [%s] (%s), node [%llu], item "
			  "[%u]: item of the illegal plugin (%s) "
			  "with the key of this object found.%s",
			  print_inode(obj40_core, &info->object),
			  reiser4_oplug(cc)->p.label, place_blknr(&cc->body),
			  cc->body.pos.item, cc->body.plug->p.label, 
			  hint->mode == RM_BUILD ? " Removed." : "");
		
		return hint->mode == RM_BUILD ? -ESTRUCT : RE_FATAL;
	}
	
	/* Check the shift. */
	clsize = ccreg40_get_cluster_size(&cc->body);
	
	if (hint->clsize != clsize) {
		fsck_mess("The file [%s] (%s), node [%llu], item [%u]: item "
			  "of the wrong cluster size (%d) found, Should be "
			  "(%d).%s", print_inode(obj40_core, &info->object),
			  reiser4_oplug(cc)->p.label, place_blknr(&cc->body),
			  cc->body.pos.item, clsize, hint->clsize, 
			  hint->mode != RM_CHECK ? " Fixed." : "");

		/* Just fix the shift if wrong. */
		if (hint->mode == RM_CHECK) {
			res |= RE_FIXABLE;
		} else {
			ccreg40_set_cluster_size(&cc->body, hint->clsize);
		}
	}
	
	if (!ccreg40_clsame(hint->found, hint->maxreal, hint->clsize)) {
		/* The item covers the cluster border. Delete it. */
		fsck_mess("The file [%s] (%s), node [%llu], item [%u]: "
			  "item of the lenght (%llu) found, it cannot "
			  "contain data of 2 clusters.%s", 
			  print_inode(obj40_core, &info->object),
			  reiser4_oplug(cc)->p.label, 
			  place_blknr(&cc->body), cc->body.pos.item,
			  hint->maxreal - hint->found + 1, 
			  hint->mode == RM_BUILD ? " Removed." : "");
		
		return hint->mode == RM_BUILD ? -ESTRUCT : RE_FATAL;
	}
	
	return res;
}

static int64_t ccreg40_read_item(reiser4_place_t *place, ccreg40_hint_t *hint) {
	trans_hint_t trans;
	uint64_t offset;
	int64_t count;

	aal_assert("vpf-1870", place->pos.unit == 0 || 
		               place->pos.unit == MAX_UINT32);
	
	/* Read the data. */
	offset = hint->found % hint->clsize;
	trans.count = hint->maxreal - hint->found;
	trans.specific = hint->data + offset;
	
	if ((count = objcall(place, object->read_units, &trans)) < 0)
		return count;
	
	return 0;
}

static errno_t ccreg40_check_crc(ccreg40_hint_t *hint) {
	uint32_t adler, disk;
	uint64_t offset;
	
	offset = (hint->seek % hint->clsize) - sizeof(uint32_t);
	
	adler = aux_adler32(0, hint->data, offset);
	disk = *(uint32_t *)(hint->data + offset);
	
	return adler == disk ? 0 : RE_FATAL;
}

#if 0
static errno_t ccreg40_hole_save(ccreg40_hint_t *hint, 
				 uint64_t start, uint64_t end)
{
	/* A hole b/w items of the same cluster found.
	   Save the hole into the hole-list. */
	ptr_hint_t *ptr;

	if (!(ptr  = aal_calloc(sizeof(*ptr), 0)))
		return -ENOMEM;

	hint->list = aal_list_append(hint->list, ptr);
	if (!hint->list)
		return -ENOMEM;

	ptr->start = start;
	ptr->width = end - start;
	return 0;
}

static errno_t ccreg40_holes_free(void *p, void *data) {
	ptr_hint_t *ptr = (ptr_hint_t *)p;
	
	if (data) {
		/* Insert the zeroed hole. */
		reiser4_object_t *cc;
		trans_hint_t trans;
		
		cc = (reiser4_object_t *)data;
		ccreg40_write_clust(cc, &trans, NULL, ptr->start, 
				    ptr->width, );
	}

	aal_free(ptr);
	return 0;
}
#endif

static errno_t cc_write_item(reiser4_place_t *place, void *data) {
	return ccreg40_set_cluster_size(place, *(uint32_t *)data);
}

static errno_t ccreg40_check_cluster(reiser4_object_t *cc, 
				     ccreg40_hint_t *hint,
				     uint8_t mode) 
{
	trans_hint_t trans;
	uint64_t offset;
	errno_t result;
	errno_t res;
	int start;
	int last;
	
	result = 0;
	start = (ccreg40_clstart(hint->seek, hint->clsize) == hint->seek);
	last = (hint->sdsize == hint->seek);
	
	if ((cc->body.plug == NULL) || (hint->seek && start) || 
	    !ccreg40_clsame(hint->seek, hint->found, hint->clsize))
	{
		/* Cluster is over. */
		if (start || (last && !cc->body.plug)) {
			/* The previous cluster is not compressed:
			   1) there were @hint->clsize bytes in it;
			   2) file size is reached and no more items found. */
			uint64_t clstart, clsize;

			clstart = ccreg40_clstart(hint->seek, hint->clsize);
			clstart -= (start ? hint->clsize : 0);
			
			clsize = (start ? hint->clsize : hint->sdsize - clstart);
			
			/* If there is a hole in the previous cluster, 
			   overwrite it. */
			if (hint->hole) {
				fsck_mess("The file [%s] (%s): the not-compressed "
					  "cluster at [%llu] offset has some items "
					  "missed.%s", 
					  print_inode(obj40_core, &cc->info.object),
					  reiser4_oplug(cc)->p.label, hint->clstart,
					  hint->mode != RM_CHECK ? " Filled with "
					  "zeroes." : "");
			
				if (hint->mode == RM_BUILD) {
					reiser4_item_plug_t *plug;
					plug = (reiser4_item_plug_t *)
						cc->info.opset.plug[OPSET_CTAIL];
					
					res = obj40_write(cc, &trans, hint->data,
							  clstart, clsize, plug, 
							  cc_write_item, 
							  &hint->clsize);
					if (res < 0) return res;
					
					hint->bytes += trans.bytes;
				} else if (hint->hole) {
					result = RE_FATAL;
				}
			}
		} else if (hint->hole || ccreg40_check_crc(hint)) {
			/* 1. There is a hole at the end of the cluster &&
			   2. Not the last cluster or sdsize is not equal to 
			      the real amount of bytes. 
			 
			   There are holes in the middle of the cluster or 
			   checksum does not match. Delete the whole cluster. */
			
			hint->bytes = 0;
			result = RE_FATAL;
			
			/* Start offset of the cluster to be deleted. */
			hint->clstart = ccreg40_clstart(hint->seek, 
							hint->clsize);
			
			fsck_mess("The file [%s] (%s): the cluster at [%llu] "
				  "offset %u bytes long is corrupted.%s",
				  print_inode(obj40_core, &cc->info.object),
				  reiser4_oplug(cc)->p.label, hint->clstart,
				  hint->clsize, hint->mode != RM_CHECK ? 
				  " Removed." : "");
		}
		
		/* Fini all the data related to the previous cluster. */
		hint->stat.bytes += hint->bytes;
		hint->bytes = 0;
		hint->adler = 0;
		hint->hole = 0;
		
		if (!cc->body.plug)
			return result;
		
		/* Update the cluster data. */
		aal_memset(hint->data, 0, hint->clsize);
	}
	
	/* An item found. */
	offset = ccreg40_clstart(hint->found, hint->clsize);
	offset = offset >= hint->seek ? offset : hint->seek;
	
	/* A hole b/w items or in the beginning found. */
	if (hint->found - offset)
		hint->hole = 1;
	
	if ((res = ccreg40_read_item(&cc->body, hint)))
		return res;
	
	hint->bytes += objcall(&cc->body, object->bytes);
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
	hint.clsize = ccreg40_clsize(cc);
	hint.sdsize = obj40_get_size(cc);

	while(1) {
		lookup_t lookup;
		
		/* Get next item. */
		lookup = obj40_check_item(cc, ccreg40_check_item, NULL, &hint);
		
		if (repair_error_fatal(lookup))
			return lookup;
		else if (lookup == ABSENT)
			cc->body.plug = NULL;
		
		/* Register the item. */
		if (cc->body.plug && func && func(&cc->body, data)) {
			aal_bug("vpf-1869", "The item [%s] should not be "
				"registered yet.", print_key(obj40_core, 
							     &info->object));
		}
		
		if ((res |= ccreg40_check_cluster(cc, &hint, mode)) < 0)
			return res;

		if (res & RE_FATAL) {
			/* Delete the whole cluster. */
			
			if (mode == RM_BUILD) {
				res &= ~RE_FATAL;
				res |= obj40_cut(cc, &trans, hint.clstart,
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
		
		hint.stat.mode = S_IFREG;
		hint.stat.size = objcall(&cc->position, get_offset);

		res |= obj40_update_stat(cc, &ops, &hint.stat, mode);
	}
	
	obj40_reset(cc);

	return res;
}

#endif
