/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sym40.c -- reiser4 symlink file plugin. */

#include "sym40.h"
#include "sym40_repair.h"

#ifdef ENABLE_SYMLINKS

/* Reads whole symlink data to passed @buff. */
static int64_t sym40_read(reiser4_object_t *sym, 
			  void *buff, uint64_t n)
{
	errno_t res;

	aal_assert("umka-1571", buff != NULL);
	aal_assert("umka-1570", sym != NULL);

	/* Update stat data coord. */
	if ((res = obj40_update(sym)))
		return res;

	/* Reading symlink extension data. */
	if ((res = obj40_read_ext(sym, SDEXT_SYMLINK_ID, buff)))
		return res;

	return aal_strlen(buff);
}

#ifndef ENABLE_MINIMAL
/* Creates symlink and returns initialized instance to the caller */
static errno_t sym40_create(reiser4_object_t *sym, object_hint_t *hint) {
	aal_assert("umka-1740", hint != NULL);

	/* Create symlink sta data item. */
	return obj40_create_stat(sym, aal_strlen(hint->str), 0, 0, 0, 
				 hint->mode | S_IFLNK | 0644, hint->str);
}

/* Clober symlink, that is clobber its stat data. */
static errno_t sym40_clobber(reiser4_object_t *sym) {
	aal_assert("umka-2300", sym != NULL);
	return obj40_clobber(sym);
}

#endif

/* This function reads symlink, parses it by means of using aux_parse_path()
   with applying corresponding callback fucntions for searching stat data and
   searchig all entries. It returns stat data key of the object symlink points
   to. */
static errno_t sym40_follow(reiser4_object_t *sym,
			    reiser4_key_t *from,
			    reiser4_key_t *key)
{
	uint32_t size;
	errno_t res;
	char *path;
	
	aal_assert("umka-1775", key != NULL);
	aal_assert("umka-2245", from != NULL);
	aal_assert("umka-1774", sym != NULL);

	/* Maximal symlink size is MAX_ITEM_LEN. Take the block size to 
	   simplify it. */
	size = place_blksize(STAT_PLACE(sym));
	if (!(path = aal_calloc(size, 0)))
		return -ENOMEM;
	
	/* Read symlink data to @path */
	if ((res = sym40_read(sym, path, size) < 0))
		goto error;

	/* Calling symlink parse function and resolution function. */
	if ((res = obj40_core->object_ops.resolve(sym->info.tree,
						  path, from, key)))
	{
		goto error;
	}

	aal_free(path);
	return 0;
	
 error:
	aal_free(path);
	return res;
}

/* Symlink plugin itself. */
reiser4_object_plug_t sym40_plug = {
	.p = {
		.id    = {OBJECT_SYM40_ID, SYM_OBJECT, OBJECT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "sym40",
		.desc  = "Symlink file plugin.",
#endif
	},

#ifndef ENABLE_MINIMAL
	.inherit	= obj40_inherit,
	.create	        = sym40_create,
	.metadata       = obj40_metadata,
	.link           = obj40_link,
	.unlink         = obj40_unlink,
	.linked         = obj40_linked,
	.update         = obj40_save_stat,
	.clobber        = sym40_clobber,
	.recognize	= obj40_recognize,
	.check_struct   = sym40_check_struct,

	.layout         = NULL,
	.seek	        = NULL,
	.write	        = NULL,
	.convert        = NULL,
	.truncate       = NULL,
	.rem_entry      = NULL,
	.add_entry      = NULL,
	.build_entry    = NULL,
	.attach         = NULL,
	.detach         = NULL,
	
	.fake		= NULL,
	.check_attach 	= NULL,
#endif
	.lookup	        = NULL,
	.reset	        = NULL,
	.offset	        = NULL,
	.readdir        = NULL,
	.telldir        = NULL,
	.seekdir        = NULL,

	.stat           = obj40_load_stat,
	.read	        = sym40_read,
	.open	        = obj40_open,
	.close	        = NULL,
	.follow         = sym40_follow,

#ifndef ENABLE_MINIMAL
	.sdext_mandatory = (1 << SDEXT_LW_ID | 
			    1 << SDEXT_SYMLINK_ID),
	.sdext_unknown   = (1 << SDEXT_CLUSTER_ID),
#endif
};
#endif
