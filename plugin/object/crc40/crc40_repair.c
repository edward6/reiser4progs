/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   crc40_repair.c -- reiser4 crypto-compression regular file plugin
   repair code. */
 
#ifndef ENABLE_MINIMAL

#include <misc/misc.h>
#include "crc40_repair.h"

#define crc40_nextclust(offset, cluster) \
	(((offset) & ~((cluster) - 1)) + (cluster))

#define crc40_sameclust(off1, off2, clust) \
	(((off1) & ~((clust) - 1)) == ((off2) & ~((clust) - 1)))

static int crc40_check_size(reiser4_object_t *crc, 
			    uint64_t *sd_size, 
			    uint64_t counted_size)
{
	return 0;
}

typedef struct crc40_hint {
	obj40_stat_hint_t stat;

	/* Item seek, found and next item offsets. */
	uint64_t seek;
	uint64_t found;
	uint64_t maxreal;

	/* Bytes all clusters takes on disk. */
	uint32_t bytes;
	
	uint64_t sd_size;
	uint8_t mode;

	/* The list of holes within the cluster. If no compression detected, 
	   fill all holes with zeroes. */
	aal_list_t *list;
	
	uint32_t adler;
	
	/* The cluster size & the buffer for the data. */
	uint32_t size;
	uint8_t data[64 * 1024];
} crc40_hint_t;

static errno_t crc40_check_item(reiser4_object_t *crc, void *data) {
	crc40_hint_t *hint = (crc40_hint_t *)data;
	object_info_t *info;
	uint32_t cluster;
	errno_t res;
		
	hint->found = plug_call(crc->body.key.plug->pl.key,
				get_offset, &crc->body.key);
	hint->maxreal = obj40_place_maxreal(&crc->body);
	
	aal_assert("vpf-1871", hint->maxreal >= hint->found);
	aal_assert("vpf-1872", hint->seek <= hint->found);
	
	info = &crc->info;
	res = 0;
	
	/* Check the item plugin. */
	if (crc->body.plug != info->opset.plug[OPSET_CTAIL]) {
		fsck_mess("CRC file [%s] (%s), node [%llu], item "
			  "[%u]: item of the illegal plugin (%s) "
			  "with the key of this object found.%s",
			  print_inode(obj40_core, &info->object),
			  reiser4_oplug(crc)->label, place_blknr(&crc->body),
			  crc->body.pos.item, crc->body.plug->label, 
			  hint->mode == RM_BUILD ? " Removed." : "");
		
		return hint->mode == RM_BUILD ? -ESTRUCT : RE_FATAL;
	}
	
	/* Check the shift. */
	cluster = crc40_get_cluster_size(&crc->body);
	
	if (hint->size != cluster) {
		fsck_mess("CRC file [%s] (%s), node [%llu], item [%u]: item "
			  "of the wrong cluster size (%d) found, Should be "
			  "(%d).%s", print_inode(obj40_core, &info->object),
			  reiser4_oplug(crc)->label, place_blknr(&crc->body),
			  crc->body.pos.item, cluster, hint->size, 
			  hint->mode != RM_CHECK ? " Fixed." : "");

		/* Just fix the shift if wrong. */
		if (hint->mode == RM_CHECK) {
			res |= RE_FIXABLE;
		} else {
			crc40_set_cluster_size(&crc->body, hint->size);
		}
	}
	
	if (!crc40_sameclust(hint->found, hint->maxreal, hint->size)) {
		/* The item covers the cluster border. Delete it. */
		fsck_mess("CRC file [%s] (%s), node [%llu], item [%u]: "
			  "item of the lenght (%llu) found, it cannot "
			  "contain data of 2 clusters.%s", 
			  print_inode(obj40_core, &info->object),
			  reiser4_oplug(crc)->label, 
			  place_blknr(&crc->body), crc->body.pos.item,
			  hint->maxreal - hint->found + 1, 
			  hint->mode == RM_BUILD ? " Removed." : "");
		
		return hint->mode == RM_BUILD ? -ESTRUCT : RE_FATAL;
	}
	
	return res;
}

static int64_t crc40_read_item(reiser4_place_t *place, crc40_hint_t *hint) {
	trans_hint_t trans;
	uint64_t offset;
	int64_t count;

	aal_assert("vpf-1870", place->pos.unit == 0 || 
		               place->pos.unit == MAX_UINT32);
	
	/* Read the data. */
	offset = hint->found % hint->size;
	trans.count = hint->maxreal - hint->found;
	trans.specific = hint->data + offset;
	
	if ((count = plug_call(place->plug->pl.item->object, 
			       read_units, place, &trans)) < 0)
	{
		return count;
	}
	
	return 0;
}

static errno_t crc40_check_crc(crc40_hint_t *hint) {
	uint32_t adler, disk;
	uint64_t offset;
	
	offset = (hint->seek % hint->size) - sizeof(uint32_t);
	
	adler = misc_adler32(0, hint->data, offset);
	disk = *(uint32_t *)(hint->data + offset);
	
	return adler == disk ? 0 : RE_FATAL;
}

static errno_t crc40_hole_save(crc40_hint_t *hint, 
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

static errno_t crc40_holes_free(void *p, void *data) {
	ptr_hint_t *ptr = (ptr_hint_t *)p;
	
	if (data) {
		/* Insert the zeroed hole. */
	}

	aal_free(ptr);
	return 0;
}

static errno_t crc40_check_cluster(reiser4_object_t *crc, 
				   crc40_hint_t *hint, 
				   uint8_t mode) 
{
	uint64_t offset;
	uint64_t count;
	errno_t res;
	int start;
	int over;
	int last;
	
	res = 0;
	start = ((hint->seek & ~(hint->size - 1)) == hint->seek);
	last = (hint->sd_size == hint->seek);
	over = (crc->body.plug == NULL);
	if (over == 0)
		over = !crc40_sameclust(hint->seek, hint->found, hint->size);
	
	if (over) {
		/* Cluster is over. */
		if (start) {
			/* There is no hole at the end of the previous cluster.
			   I.e. there is no compression. Fill all the holes with
			   zeroes if there is any. */
		} else if (crc->body.plug || last) {
			/* 1. There is a hole at the end of the cluster &&
			   2. Not the last cluster or sd_size is not equal to 
			      the real amount of bytes. */
			if (aal_list_len(hint->list) || crc40_check_crc(hint)) {
				/* There are holes in the middle of the cluster
				   or checksum does not match. Delete the whole 
				   cluster. */
				hint->bytes = 0;
				res = RE_FATAL;
			}
		}
		
		/* The cluster is over. Fini all the related data. */
		hint->stat.bytes += hint->bytes;
		hint->bytes = 0;
		hint->adler = 0;

		aal_list_free(hint->list, crc40_holes_free, 
			      start ? hint : NULL);
	}
	
	/* An item found. */
	offset = hint->found & ~(hint->size - 1);
	offset = offset >= hint->seek ? : hint->seek;
	count = hint->found - offset;
	
	if (count) {
		/* A hole b/w items or in the beginning found. 
		   Save the hole into the hole-list. */
		if ((res = crc40_hole_save(hint, offset,
					   hint->found)))
		{
			return res;
		}
	} else if (!aal_list_len(hint->list)) {
		/* No hole is found yet. Read the item. */
		if ((res = crc40_read_item(&crc->body, hint)))
			return res;
	}
	
	hint->bytes += plug_call(crc->body.plug->pl.item->object,
				 bytes, &crc->body);

	return res;
}

errno_t crc40_check_struct(reiser4_object_t *crc, 
			   place_func_t func,
			   void *data, uint8_t mode)
{
	object_info_t *info;
	crc40_hint_t hint;
	errno_t res, result;
	
	aal_assert("vpf-1829", crc != NULL);
	aal_assert("vpf-1836", crc->info.tree != NULL);

	info = &crc->info;
	aal_memset(&hint, 0, sizeof(hint));
	
	if ((res = obj40_prepare_stat(crc, S_IFREG, mode)))
		return res;
	
	/* Obtain hint.sd_size */
	
	/* Check the sdext_crypto.keylen, it cannot be any. */
	
	/* Try to register SD as an item of this file. */
	if (func && func(&info->start, data))
		return -EINVAL;
	
	result = 0;
	hint.mode = mode;
	hint.size = reiser4_cluster_size(info->opset.plug[OPSET_CLUSTER]);
	
	while(1) {
		lookup_t lookup;
		
		/* Get next item. */
		lookup = obj40_check_item(crc, crc40_check_item, NULL, &hint);
		
		if (repair_error_fatal(lookup)) {
			aal_list_free(hint.list, crc40_holes_free, NULL);
			return lookup;
		} else if (lookup == ABSENT)
			crc->body.plug = NULL;
		
		/* Register the item. */
		if (crc->body.plug && func && func(&crc->body, data)) {
			aal_bug("vpf-1869", "The item [%s] should not be "
				"registered yet.", print_key(obj40_core, 
							     &info->object));
		}
		
		if ((res |= crc40_check_cluster(crc, &hint, mode)) < 0) {
			aal_list_free(hint.list, crc40_holes_free, NULL);
			return res;
		}
		
		if (res & RE_FATAL) {
			/* Delete the whole cluster. */
			
			continue;
		}
		
		/* If the file size is the max possible one, break out 
		   here to not seek to 0. */
		if (!crc->body.plug || hint.maxreal == MAX_UINT64)
			break;
		
		hint.seek = hint.maxreal + 1;
		obj40_seek(crc, hint.seek);
	}

	/* Fix the SD, if no fatal corruptions were found. */
	if (!(res & RE_FATAL)) {
		obj40_stat_ops_t ops;
		
		aal_memset(&ops, 0, sizeof(ops));
		
		ops.check_size = crc40_check_size;
		ops.check_nlink = mode == RM_BUILD ? 0 : SKIP_METHOD;
		
		hint.stat.mode = S_IFREG;
		hint.stat.size = plug_call(crc->position.plug->pl.key,
					   get_offset, &crc->position);

		res |= obj40_update_stat(crc, &ops, &hint.stat, mode);
	}
	
	obj40_reset(crc);
	
	return res;
}

#endif
