/*
    librepair/oid.c - methods are needed for work with broken oid allocator.
    Copyright (C) 1996-2002 Hans Reiser.
*/

#include <repair/librepair.h>

static errno_t repair_oid_check(reiser4_fs_t *fs) {
    return 0;
}

errno_t repair_oid_open(reiser4_fs_t *fs) {
    int res;

    aal_assert("vpf-403", fs != NULL, return -1);
    aal_assert("vpf-405", fs->format != NULL, return -1);
 
    /* Initializes oid allocator */
    fs->oid = reiser4_oid_open(fs->format);
  
    if ((res = repair_oid_check(fs)))
	goto error_free_oid;

    return 0;
    
error_free_oid:
    if (fs->oid)
	reiser4_oid_close(fs->oid);

    return res;
}

