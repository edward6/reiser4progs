/* Copyright (C) 2001-2005 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   misc.c -- some common tools for all reiser4 utilities. */

#include <stdio.h>
#include <mntent.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <utime.h>

#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/types.h>

#include <misc/misc.h>
#include <reiser4/libreiser4.h>

#define KB 1024
#define MB (KB * KB)
#define GB (KB * MB)

#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
#  include <uuid/uuid.h>
#endif

/* Converts passed @sqtr into long long value. In the case of error, INVAL_DIG
   will be returned. */
long long misc_str2long(const char *str, int base) {
	char *error;
	long long result = 0;

	if (!str)
		return INVAL_DIG;

	result = strtol(str, &error, base);
	
	if (errno == ERANGE || *error)
		return INVAL_DIG;
	
	return result;
}


/* Converts human readable size string like "256M" into KB. In the case of
   error, INVAL_DIG will be returned. */
long long misc_size2long(const char *str) {
	int valid;
	char label;
	
	long long result;
	char number[255];
	 
	if (str) {
		aal_memset(number, 0, 255);
		aal_strncpy(number, str, sizeof(number));
		label = number[aal_strlen(number) - 1];

		valid = toupper(label) == toupper('K') ||
			toupper(label) == toupper('M') || 
			toupper(label) == toupper('G');

		if (valid)
			number[aal_strlen(number) - 1] = '\0';

		if ((result = misc_str2long(number, 10)) == INVAL_DIG)
			return result;

		if (toupper(label) == toupper('K'))
			return result;

		if (toupper(label) == toupper('M'))
			return result * KB;

		if (toupper(label) == toupper('G'))
			return result * MB;

		return result;
	}
	
	return INVAL_DIG;
}
/* Lookup the @file in the @mntfile. @file is mntent.mnt_fsname if @fsname 
   is set; mntent.mnt_dir otherwise. Return the mnt entry from the @mntfile.
   
   Warning: if the root fs is mounted RO, the content of /etc/mtab may be 
   not correct. */
static struct mntent *misc_mntent_lookup(const char *mntfile, 
					 const char *file, 
					 int path) 
{
	struct mntent *mnt;
	int name_match = 0;
	struct stat st;
	dev_t rdev = 0;
	dev_t dev = 0;
	ino_t ino = 0;
	char *name;
	FILE *fp;
	
	aal_assert("vpf-1674", mntfile != NULL);
	aal_assert("vpf-1675", file != NULL);

	if (stat(file, &st) == 0) {
		/* Devices is stated. */
		if (S_ISBLK(st.st_mode)) {
			rdev = st.st_rdev;
		} else {
			dev = st.st_dev;
			ino = st.st_ino;
		}
	}

	if ((fp = setmntent(mntfile, "r")) == NULL)
		return INVAL_PTR;

	while ((mnt = getmntent(fp)) != NULL) {
		/* Check if names match. */
		name = path ? mnt->mnt_dir : mnt->mnt_fsname;
		
		if (aal_strcmp(file, name) == 0)
			name_match = 1;

		if (stat(name, &st))
			continue;
		
		/* If names do not match, check if stats match. */
		if (!name_match) {
			if (rdev && S_ISBLK(st.st_mode)) {
				if (rdev != st.st_rdev)
					continue;
			} else if (dev && !S_ISBLK(st.st_mode)) {
				if (dev != st.st_dev ||
				    ino != st.st_ino)
					continue;
			} else {
				continue;
			}
		}

		/* If not path and not block device do not check anything more. */
		if (!path && !rdev) 
			break;

		if (path) {
			/* Either names or stats match. Make sure the st_dev of 
			   the path is same as @mnt_fsname device rdev. */
			if (stat(mnt->mnt_fsname, &st) == 0 && 
			    dev == st.st_rdev)
				break;
		} else {
			/* Either names or stats match. Make sure the st_dev of 
			   the mount entry is same as the given device rdev. */
			if (stat(mnt->mnt_dir, &st) == 0 && 
			    rdev == st.st_dev)
				break;
		}
	}

	endmntent (fp);
        return mnt;
}

static int misc_root_mounted(const char *device) {
	struct stat rootst, devst;
	
	aal_assert("vpf-1676", device != NULL);

	if (stat("/", &rootst) != 0) 
		return -1;

	if (stat(device, &devst) != 0)
		return -1;

	if (!S_ISBLK(devst.st_mode) || 
	    devst.st_rdev != rootst.st_dev)
		return 0;

	return 1;
}

static int misc_file_ro(char *file) {
	if (utime(file, 0) == -1) {
		if (errno == EROFS)
			return 1;
	}

	return 0;
}

struct mntent *misc_mntent(const char *device) {
	int proc = 0, path = 0, root = 0;
	
	struct mntent *mnt;
	struct statfs stfs;

	aal_assert("vpf-1677", device != NULL);
	
	/* Check if the root. */
	if (misc_root_mounted(device) == 1)
		root = 1;
	
#ifdef __linux__
	/* Check if /proc is procfs. */
	if (statfs("/proc", &stfs) == 0 && stfs.f_type == 0x9fa0) {
		proc = 1;
		
		if (root) {
			/* Lookup the "/" entry in /proc/mounts. Special 
			   case as root entry can present as:
				rootfs / rootfs rw 0 0
			   Look up the mount point in this case. */
			mnt = misc_mntent_lookup("/proc/mounts", "/", 1);
		} else {
			/* Lookup the @device /proc/mounts */
			mnt = misc_mntent_lookup("/proc/mounts", device, 0);
		}
		
		if (mnt == INVAL_PTR) 
			proc = 0;
		else if (mnt)
			return mnt;
	}
#endif /* __linux__ */

#if defined(MOUNTED) || defined(_PATH_MOUNTED)

#ifndef MOUNTED
    #define MOUNTED _PATH_MOUNTED
#endif
	/* Check in MOUNTED (/etc/mtab) if RW. */
	if (!misc_file_ro(MOUNTED)) {
		path = 1;

		if (root) {
			mnt = misc_mntent_lookup(MOUNTED, "/", 1);
		} else {
			mnt = misc_mntent_lookup(MOUNTED, device, 0);
		}

		if (mnt == INVAL_PTR) 
			path = 0;
		else if (mnt)
			return mnt;
	}
#endif /* defined(MOUNTED) || defined(_PATH_MOUNTED) */
	
	/* If has not been checked in neither /proc/mounts nor /etc/mtab (or 
	   errors have occurred), return INVAL_PTR, NULL otherwise. */
	return (!proc && !path) ? INVAL_PTR : NULL;
}

int misc_dev_mounted(const char *device) {
	struct mntent *mnt;
	
	/* Check for the "/" first to avoid any possible problem with 
	   reflecting the root fs info in mtab files. */
	if (misc_root_mounted(device) == 1) {
		return misc_file_ro("/") ? MF_RO : MF_RW;
	}
	
	/* Lookup the mount entry. */
	if ((mnt = misc_mntent(device)) == NULL) {
		return MF_NOT_MOUNTED;
	} else if (mnt == INVAL_PTR) {
		return 0;
	}

	return hasmntopt(mnt, MNTOPT_RO) ? MF_RO : MF_RW;
}

void misc_upper_case(char *dst, const char *src) {
	int i = 0;
	const char *s;

	s = src;
	while (*s)
		dst[i++] = toupper(*s++);
	dst[i] = '\0';
}

static errno_t cb_print_plug(reiser4_plug_t *plug, void *data) {
	uint8_t w1;
	w1 = 18 - aal_strlen(plug->label);
	printf("\"%s\"%*s(id:0x%x type:0x%x) [%s]\n", plug->label, 
	       w1, " ", plug->id.id, plug->id.type, plug->desc);
	return 0;
}

void misc_plugins_print(void) {
	printf("Known plugins:\n");
	reiser4_factory_foreach(cb_print_plug, NULL);
	printf("\n");
}

void misc_uuid_unparse(char *uuid, char *string) {
#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	uuid_unparse((unsigned char *)uuid, string);
#endif
}
