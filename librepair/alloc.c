/*
    librepair/alloc.c - methods are needed to work with broken allocator.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

static errno_t repair_alloc_check(reiser4_fs_t *fs) {
    /* FIXME-VITALY: Check checksums and fill corrupted bitmap blocks with 0xff
     * when main things start working. */
    return 0;
}

errno_t repair_alloc_open(reiser4_fs_t *fs) {
    while ((fs->alloc = reiser4_alloc_open(fs->format, 
	reiser4_format_get_len(fs->format))) == NULL) 
    {
	/* unable to open an allocator */
	/* FIXME-VITALY: fsck should be run with profile. While working with 
	*  format plugins (alloc, oid, etc), their ids must be equal. Write 
	*  their comare code later. */
	return -1;
    }

    return repair_alloc_check(fs);	
}

