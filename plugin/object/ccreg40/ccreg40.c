/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   crc40.c -- reiser4 crypto compression regular file plugin. */

#ifndef ENABLE_MINIMAL

#include "ccreg40.h"
#include "ccreg40_repair.h"

uint32_t ccreg40_get_cluster_size(reiser4_place_t *place) {
	trans_hint_t hint;
	ctail_hint_t chint;
	
	aal_assert("vpf-1866", place != NULL);
	
	hint.specific = &chint;
	hint.count = 1;

	if (plug_call(place->plug->pl.item->object, 
		      fetch_units, place, &hint) != 1)
	{
		return MAX_UINT32;
	}

	return 1 << chint.shift;
}

errno_t ccreg40_set_cluster_size(reiser4_place_t *place, uint32_t cluster) {
	trans_hint_t hint;
	ctail_hint_t chint;
	
	aal_assert("vpf-1867", place != NULL);
	
	hint.specific = &chint;
	hint.count = 1;
	chint.shift = aal_log2(cluster);

	if (plug_call(place->plug->pl.item->object, 
		      update_units, place, &hint) != 0)
	{
		return -EIO;
	}

	return 0;
}

static int64_t ccreg40_extract_data(reiser4_object_t *crc, void *clust, 
				    void *buff, uint64_t off, int64_t read)
{
	if (crc->info.opset.plug[OPSET_CRYPTO] != CRYPTO_NONE_ID) {
		aal_error("Object [%s]: Can't extract encrypted "
			  "data. Not supported yet.",
			  print_inode(obj40_core, &crc->info.object));
		return -EINVAL;
	}

	if (!reiser4_nocomp(crc->info.opset.plug[OPSET_CRYPTO]->id.id)) {
		aal_error("Object [%s]: Can't extract compressed "
			  "data. Not supported yet.",
			  print_inode(obj40_core, &crc->info.object));
		return -EINVAL;
	}

	off -= ccreg40_clstart(off, ccreg40_clsize(crc));
	aal_memcpy(buff, clust + off, read);
	
	return read;
}

static int64_t ccreg40_read(reiser4_object_t *crc, 
			    void *buff, uint64_t n)
{
	trans_hint_t hint;
	uint32_t clsize;
	uint64_t fsize;
	uint64_t count;
	uint64_t end;
	uint64_t off;
	int64_t read;
	void *clust;
	
	aal_assert("vpf-1873", crc != NULL);
	aal_assert("vpf-1874", buff != NULL);
	
	fsize = obj40_size(crc);
	clsize = ccreg40_clsize(crc);
	
	if (!(clust = aal_calloc(clsize, 0))) {
		aal_error("Can't allocate memory for a cluster.");
		return -ENOMEM;
	}
	
	/* Init the hint params: start key, count to be read, buffer, etc. */
	aal_memset(&hint, 0, sizeof(hint));
	aal_memcpy(&hint.offset, &crc->position, sizeof(hint.offset));
	
	off = obj40_offset(crc);
	end = (n > fsize ? fsize : off > fsize - n ? fsize : off + n);
	hint.specific = clust;

	count = 0;
	
	while (off < end) {
		hint.count = (end > ccreg40_clnext(off, clsize) ? 
			      ccreg40_clnext(off, clsize) : end);
		hint.count -= ccreg40_clstart(off, clsize);
		
		plug_call(hint.offset.plug->pl.key, set_offset, &hint.offset,
			  off - ccreg40_clstart(off, clsize));

		/* Reading data. */
		if ((read = obj40_read(crc, &hint)) < 0) {
			aal_free(clust);
			return read;
		}
		
		/* Extract the read cluster to the user buffer. */
		if ((read = ccreg40_extract_data(crc, clust, buff + count,
						 off, read)) < 0)
		{
			aal_free(clust);
			return read;
		}
		
		off += read;
		count += read;
	}
	
	if (off > end) {
		aal_bug("vpf-1875", "More bytes read %llu than "
			"present %llu in the file [%s].", read, n,
			print_inode(obj40_core, &crc->info.start));
	}
	
	obj40_seek(crc, end);
	aal_free(clust);
	
	return n;
}

static int64_t ccreg40_write(reiser4_object_t *crc, 
			     void *buff, uint64_t n)
{
	aal_error("Plugin \"%s\": The method ->write is not "
		  "implemented yet.", reiser4_oplug(crc)->label);

	return -EIO;
}

/* CRC regular file operations. */
static reiser4_object_plug_t ccreg40 = {
	.inherit	= obj40_inherit,
	.create	        = obj40_create,
	.write	        = ccreg40_write,
	.truncate       = NULL,
	.layout         = NULL,
	.metadata       = NULL,
	.convert        = NULL,
	.update         = obj40_save_stat,
	.link           = obj40_link,
	.unlink         = obj40_unlink,
	.linked         = obj40_linked,
	.clobber        = NULL,
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
			    1 << SDEXT_CLUSTER_ID)
};

/* CRC regular file plugin. */
reiser4_plug_t ccreg40_plug = {
	.cl    = class_init,
	.id    = {OBJECT_CRC40_ID, REG_OBJECT, OBJECT_PLUG_TYPE},
	.label = "crc40",
	.desc  = "Crypto-Compression regular file plugin.",
	.pl = {
		.object = &ccreg40
	}
};

/* Plugin factory related stuff. This method will be called during plugin
   initializing in plugin factory. */
static reiser4_plug_t *ccreg40_start(reiser4_core_t *c) {
	obj40_core = c;
	return &ccreg40_plug;
}

plug_register(ccreg40, ccreg40_start, NULL);
#endif
