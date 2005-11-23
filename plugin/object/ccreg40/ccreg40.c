/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   ccreg40.c -- reiser4 crypto compression regular file plugin. */

#ifndef ENABLE_MINIMAL

#include "ccreg40.h"
#include "ccreg40_repair.h"

uint32_t ccreg40_get_cluster_size(reiser4_place_t *place) {
	trans_hint_t hint;
	ctail_hint_t chint;
	
	aal_assert("vpf-1866", place != NULL);
	
	hint.specific = &chint;
	hint.count = 1;

	if (objcall(place, object->fetch_units, &hint) != 1)
		return MAX_UINT32;
	
	return 1 << chint.shift;
}

errno_t ccreg40_set_cluster_size(reiser4_place_t *place, uint32_t cluster) {
	trans_hint_t hint;
	ctail_hint_t chint;
	
	aal_assert("vpf-1867", place != NULL);
	
	hint.specific = &chint;
	hint.count = 1;
	chint.shift = aal_log2(cluster);

	if (objcall(place, object->update_units, &hint) != 1)
		return -EIO;
	
	return 0;
}

/* Performs Cluster De-CryptoCompression. @count bytes are taken from @disk,
   get De-CC & the result is put into @clust. Returns the size of uncompressed
   cluster. compressed is an indicator if data on disk are smaller then the 
   cluster size. Note cluster size depends on file size for the last cluster. */
static int64_t ccreg40_decc_cluster(reiser4_object_t *cc, 
				    void *clust, void *disk, 
				    int64_t count, uint32_t compressed)
{
	if (cc->info.opset.plug[OPSET_CRYPTO] != CRYPTO_NONE_ID) {
		aal_error("Object [%s]: Can't extract encrypted "
			  "data. Not supported yet.",
			  print_inode(obj40_core, &cc->info.object));
		return -EINVAL;
	}

	/* Desite the set compression plugin, cluster can be either 
	   compressed or not. */
	if (compressed) {
		aal_error("Object [%s]: Can't extract compressed "
			  "data. Not supported yet.",
			  print_inode(obj40_core, &cc->info.object));
		return -EINVAL;
	}

	aal_memcpy(clust, disk, count);
	return count;
}

/* Performs Cluster CryptoCompression. @could bytes are taken from @clust, get
   CC & the result is put into @disk. Returns the size of CC-ed cluster. */
static int64_t ccreg40_cc_cluster(reiser4_object_t *cc, 
				  void *disk, void *clust, 
				  uint64_t count)
{
	if (cc->info.opset.plug[OPSET_CRYPTO] != CRYPTO_NONE_ID) {
		aal_error("Object [%s]: Can't encrypt data. Not supported "
			  "yet.", print_inode(obj40_core, &cc->info.object));
		return -EINVAL;
	}

	if (cc->info.opset.plug[OPSET_CMODE]->id.id != CMODE_NONE_ID ||
	    reiser4_compressed(cc->info.opset.plug[OPSET_COMPRESS]->id.id))
	{
		aal_error("Object [%s]: Can't compress data. Not supported "
			  "yet.", print_inode(obj40_core, &cc->info.object));
		return -EINVAL;
	}

	aal_memcpy(disk, clust, count);
	return count;
}

/* Cluster read operation. It reads exactly 1 cluster the @off offset belongs 
   to. De-CC it, copy the wanted part of the cluster into @buff & return the 
   amount of bytes put into @buff. */
static int64_t ccreg40_read_clust(reiser4_object_t *cc, trans_hint_t *hint, 
				  void *buff, uint64_t off, uint64_t count,
				  uint32_t fsize)
{
	uint8_t clust[64 *1024];
	uint8_t disk[64 * 1024];
	uint64_t clstart;
	uint32_t clsize;
	int64_t read;
	
	if (off > fsize)
		return 0;
	
	clsize = ((reiser4_cluster_plug_t *)
		  cc->info.opset.plug[OPSET_CLUSTER])->clsize;
	clstart = ccreg40_clstart(off, clsize);
	if (clsize > fsize - clstart)
		clsize = fsize - clstart;
	
	/* Reading data. */
	if ((read = obj40_read(cc, hint, disk, clstart, clsize)) < 0)
		return read;
	
	if (read == 0) {
		read = clstart + clsize - off;
		if ((uint64_t)read > count)
			read = count;
		
		aal_memset(buff, 0, read);
		return read;
	}
	
	/* Extract the read cluster to the given buffer. */
	if ((read = ccreg40_decc_cluster(cc, clust, disk, read,
					 read < clsize)) < 0)
	{
		return read;
	}

	if (read != clsize) {
		aal_error("File [%s]: Failed to read the cluster at the offset "
			  "(%llu).", print_inode(obj40_core, &cc->info.object),
			  clstart);
		return -EIO;
	}
	
	off -= clstart;
	read = clsize - off;
	if ((uint64_t)read > count)
		read = count;
	
	aal_memcpy(buff, clust + off, read);
	return read;
}

static errno_t cc_write_item(reiser4_place_t *place, void *data) {
	return ccreg40_set_cluster_size(place, *(uint32_t *)data);
}

/* Cluster write operation. It write exactly 1 cluster given in @buff. */
static int64_t ccreg40_write_clust(reiser4_object_t *cc, trans_hint_t *hint,
				   void *buff, uint64_t off, uint64_t count,
				   uint64_t fsize)
{
	reiser4_item_plug_t *iplug;
	uint8_t clust[64 *1024];
	uint8_t disk[64 * 1024];
	uint64_t clstart;
	uint32_t clsize;
	int64_t written;
	uint64_t end;
	int64_t done;
	
	done = 0;
	clsize = ((reiser4_cluster_plug_t *)
		  cc->info.opset.plug[OPSET_CLUSTER])->clsize;
	clstart = ccreg40_clstart(off, clsize);
	
	/* Set @end to the cluster end offset. */
	end = clstart + clsize;
	if (end > fsize)
		end = fsize;
	
	if (clstart >= fsize) {
		aal_memset(clust, 0, clsize);
	} else if (off != clstart || off + count < end) {
		if ((done = ccreg40_read_clust(cc, hint, clust, clstart, 
					       end - clstart, fsize)) < 0)
		{
			return done;
		}
		
		if ((uint64_t)done != end - clstart) {
			aal_error("File [%s]: Failed to read the "
				  "cluster at the offset (%llu).",
				  print_inode(obj40_core, &cc->info.object),
				  off);
			return -EIO;
		}
	}
	
	end = clstart + clsize;
	if (end > off + count)
		end = off + count;
	
	count = end - off;
	
	aal_memcpy(clust + off - clstart, buff, count);
	
	end = (clstart + done > off + count) ? clstart + done : off + count;
	
	if ((done = ccreg40_cc_cluster(cc, disk, clust, end - clstart)) < 0)
		return done;
	
	iplug = (reiser4_item_plug_t *)cc->info.opset.plug[OPSET_CTAIL];
	if ((written = obj40_write(cc, hint, clust, clstart, done,
				   iplug, cc_write_item, &clsize)) < 0)
	{
		return written;
	}
	
	if (written < done) {
		aal_error("File [%s]: There are less bytes "
			  "written (%llu) than asked (%llu).",
			  print_inode(obj40_core, &cc->info.object),
			  written, done);
		return -EIO;
	}
	
	return count;
}

static int64_t ccreg40_read(reiser4_object_t *cc, 
			    void *buff, uint64_t n)
{
	trans_hint_t hint;
	uint64_t count;
	uint64_t fsize;
	int64_t read;
	uint64_t off;
	errno_t res;
	
	aal_assert("vpf-1873", cc != NULL);
	aal_assert("vpf-1874", buff != NULL);
	
	if ((res = obj40_update(cc)))
		return res;
	
	count = 0;
	off = obj40_offset(cc);
	fsize = obj40_get_size(cc);
	
	if (off > fsize)
		return 0;
	
	if (n > fsize - off)
		n = fsize - off;

	while (n) {
		/* Reading data. */
		if ((read = ccreg40_read_clust(cc, &hint, buff, 
					       off, n, fsize)) < 0)
		{
			return read;
		}
		
		aal_assert("vpf-1879", (uint64_t)read <= n);
		
		count += read;
		buff += read;
		off += read;
		n -= read;
	}
	
	obj40_seek(cc, off);
	return count;
}

static int64_t ccreg40_write(reiser4_object_t *cc, 
			     void *buff, uint64_t n)
{
	trans_hint_t hint;
	uint64_t fsize;
	uint64_t count;
	uint64_t bytes;
	uint64_t off;
	errno_t res;
	
	aal_assert("vpf-1877", cc != NULL);
	aal_assert("vpf-1878", buff != NULL);
	
	if ((res = obj40_update(cc)))
		return res;
	
	fsize = obj40_get_size(cc);

	off = obj40_offset(cc);
	count = 0;
	bytes = 0;
	
	while (n) {
		if ((res = ccreg40_write_clust(cc, &hint, buff, 
					       off, n, fsize)) < 0)
		{
			return res;
		}

		aal_assert("vpf-1880", (uint64_t)res <= n);
		
		bytes += hint.bytes;
		count += res;
		buff += res;
		off += res;
		n -= res;
	}
	
	obj40_seek(cc, off);
	
	off = fsize > off ? 0 : off - fsize;
	
	/* Updating the SD place and update size, bytes there. */
	if ((res = obj40_touch(cc, off, bytes)))
		return res;
	
	return count;
}

static errno_t ccreg40_truncate(reiser4_object_t *cc, uint64_t n) {
	return obj40_truncate(cc, n, 
		(reiser4_item_plug_t *)cc->info.opset.plug[OPSET_CTAIL]);
}

static errno_t ccreg40_clobber(reiser4_object_t *cc) {
	errno_t res;
	
	aal_assert("vpf-1881", cc != NULL);
	
	if ((res = ccreg40_truncate(cc, 0)) < 0)
		return res;
	
	return obj40_clobber(cc);
}

static errno_t ccreg40_layout(reiser4_object_t *cc,
			      region_func_t func,
			      void *data)
{
	obj40_reset(cc);
	return obj40_layout(cc, func, NULL, data);
}

static errno_t ccreg40_metadata(reiser4_object_t *cc,
				place_func_t func,
				void *data)
{
	obj40_reset(cc);
	return obj40_traverse(cc, func, NULL, data);
}

/* CRC regular file plugin. */
reiser4_object_plug_t ccreg40_plug = {
	.p = {
		.id    = {OBJECT_CCREG40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
		.label = "ccreg40",
		.desc  = "Crypto-Compression regular file plugin.",
	},

	.inherit	= obj40_inherit,
	.create	        = obj40_create,
	.write	        = ccreg40_write,
	.truncate       = ccreg40_truncate,
	.layout         = ccreg40_layout,
	.metadata       = ccreg40_metadata,
	.convert        = NULL,
	.update         = obj40_save_stat,
	.link           = obj40_link,
	.unlink         = obj40_unlink,
	.linked         = obj40_linked,
	.clobber        = ccreg40_clobber,
	.recognize	= obj40_recognize,
	.check_struct   = ccreg40_check_struct,
	
	.add_entry      = NULL,
	.rem_entry      = NULL,
	.build_entry    = NULL,
	.attach         = NULL,
	.detach         = NULL,
	
	.fake		= NULL,
	.check_attach 	= NULL,
	.lookup	        = NULL,
	.follow         = NULL,
	.readdir        = NULL,
	.telldir        = NULL,
	.seekdir        = NULL,
		
	.stat           = obj40_load_stat,
	.open	        = obj40_open,
	.close	        = NULL,
	.reset	        = obj40_reset,
	.seek	        = obj40_seek,
	.offset	        = obj40_offset,
	.read	        = ccreg40_read,

	.sdext_mandatory = (1 << SDEXT_LW_ID),
	.sdext_unknown   = (1 << SDEXT_SYMLINK_ID |
			    1 << SDEXT_CRYPTO_ID)
};
#endif
