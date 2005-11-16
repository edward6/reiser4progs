/* Copyright 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   sdext_unix.c -- stat data exception plugin, that implements unix 
   stat data fields. */

#include "sdext_unix.h"
#include "sys/stat.h"

reiser4_core_t *sdext_unix_core = NULL;

static uint32_t sdext_unix_length(stat_entity_t *stat, void *hint) {
	return sizeof(sdext_unix_t);
}

#ifndef ENABLE_MINIMAL
static errno_t sdext_unix_open(stat_entity_t *stat, void *hint) {
	sdext_unix_t *ext;
	sdhint_unix_t *unixh;
    
	aal_assert("umka-886", stat != NULL);
	aal_assert("umka-887", hint != NULL);

	ext = (sdext_unix_t *)stat_body(stat);
	unixh = (sdhint_unix_t *)hint;
    
	unixh->uid = sdext_unix_get_uid(ext);
	unixh->gid = sdext_unix_get_gid(ext);
	unixh->atime = sdext_unix_get_atime(ext);
	unixh->mtime = sdext_unix_get_mtime(ext);
	unixh->ctime = sdext_unix_get_ctime(ext);

	if (S_ISBLK(stat->info.mode) || S_ISCHR(stat->info.mode)) {
		unixh->rdev = sdext_unix_get_rdev(ext);
		unixh->bytes = 0;
	} else {
		unixh->rdev = 0;
		unixh->bytes = sdext_unix_get_rdev(ext);
	}

	return 0;
}

static errno_t sdext_unix_init(stat_entity_t *stat, void *hint) {
	sdext_unix_t *ext;
	sdhint_unix_t *unixh;
    
	aal_assert("umka-884", stat != NULL);
	aal_assert("umka-885", hint != NULL);
	
	ext = (sdext_unix_t *)stat_body(stat);
	unixh = (sdhint_unix_t *)hint;
    
	sdext_unix_set_uid(ext, unixh->uid);
	sdext_unix_set_gid(ext, unixh->gid);
	sdext_unix_set_atime(ext, unixh->atime);
	sdext_unix_set_mtime(ext, unixh->mtime);
	sdext_unix_set_ctime(ext, unixh->ctime);

	if (S_ISBLK(stat->info.mode) || S_ISCHR(stat->info.mode)) {
		sdext_unix_set_rdev(ext, unixh->rdev);
	} else {
		sdext_unix_set_bytes(ext, unixh->bytes);
	}
	
	return 0;
}

extern errno_t sdext_unix_check_struct(stat_entity_t *stat, 
				       repair_hint_t *hint);

extern void sdext_unix_print(stat_entity_t *stat, 
			     aal_stream_t *stream, 
			     uint16_t options);

#endif

reiser4_sdext_plug_t sdext_unix_plug = {
	.p = {
		.id    = {SDEXT_UNIX_ID, 0, SDEXT_PLUG_TYPE},
#ifndef ENABLE_MINIMAL
		.label = "sdext_unix",
		.desc  = "Unix stat data extension plugin.",
#endif
	},
	
#ifndef ENABLE_MINIMAL
	.open	   	= sdext_unix_open,
	.init	   	= sdext_unix_init,
	.print     	= sdext_unix_print,
	.check_struct	= sdext_unix_check_struct,
#else
	.open	   	= NULL,
#endif
	.info		= NULL,
	.length	   	= sdext_unix_length
};
