/* Copyright (C) 2001, 2002, 2003, 2004 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   misc.c -- some common tools for all reiser4 utilities. */

#include <stdio.h>
#include <mntent.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/mount.h>

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

/* Checking if specified partition is mounted. It is possible devfs is used, and
   all devices are links like hda1->ide/host0/bus0/target0/lun0/part1. Therefore
   we use stat function, rather than lstat: stat(2) follows links and return
   stat information for link target. In this case it will return stat info for
   /dev/ide/host0/bus0/target0/lun0/part1 file. Then we compare its st_rdev
   field with st_rdev filed of every device from /proc/mounts. If match occurs
   then we have device existing in /proc/mounts file, which is therefore mounted
   at the moment.

   Also this function checks whether passed device mounted with specified
   options.  Options string may look like "ro,noatime".
    
   We are using stating of every mount entry instead of just name comparing,
   because file /proc/mounts may contain as devices like /dev/hda1 as
   ide/host0/bus0/targ... */
int misc_dev_mounted(
	const char *name,	/* device name to be checked */
	const char *mode)	/* mount options for check */
{
	FILE *mnt;
	errno_t res;
	struct mntent *ent;
	struct stat giv_st;
	struct stat mnt_st;
	struct statfs fs_st;

	/* Stating given device */
	if ((res = stat(name, &giv_st)) == -1)
		return res;

	/* Check only block devices. 
	   FIXME: files mounted through loop should be checked also. */
	if (!S_ISBLK(giv_st.st_mode))
		return 0;
		
	/* Procfs magic is 0x9fa0 */
	if (statfs("/proc", &fs_st) == -1 || fs_st.f_type != 0x9fa0) {

                /* Proc is not mounted, check if it is the root partition. */
		if ((res = stat("/", &mnt_st)) == -1) 
			return res;
 
		if (mnt_st.st_dev == giv_st.st_rdev) 	    
			return 1;	
        
		return 0;
	}
    
	/* Check /proc/mounts file. */
	if (!(mnt = setmntent("/proc/mounts", "r")))
		return 0;

	/* Loop until all entries are looked. */
	while ((ent = getmntent(mnt))) {
		/* Getting stat information and check if this is device we are
		   interested in to check. */
		if (stat(ent->mnt_fsname, &mnt_st) == 0) {

			/* Here we check also if current entry is not directory,
			   because we will have hit if it is a directory like
			   /proc or /sys and device we are checking is loop and
			   they both lie on the same device (say /dev/hda1). */
			if (!S_ISDIR(mnt_st.st_mode) &&
			    mnt_st.st_rdev == giv_st.st_rdev)
			{
				char *token;

				/* Okay, we have found needed device, now we
				   check mount options. */
				while (mode &&
				       (token = aal_strsep((char **)&mode, ",")))
				{
					if (!hasmntopt(ent, token))
						goto error_free_mnt;
				}
		
				endmntent(mnt);
				return 1;
			}
		}
	}

 error_free_mnt:
	endmntent(mnt);
	return 0;
}

void misc_upper_case(char *dst, const char *src) {
	int i = 0;
	const char *s;

	s = src;
	while (*s)
		dst[i++] = toupper(*s++);
	dst[i] = '\0';
}

static errno_t callback_print_plug(reiser4_plug_t *plug, void *data) {
	printf("\"%s\": %s\n", plug->label, plug->desc);
	return 0;
}

void misc_plugins_print(void) {
	printf("Known plugins:\n");
	reiser4_factory_foreach(callback_print_plug, NULL);
	printf("\n");
}

void misc_uuid_unparse(char *uuid, char *string) {
#if defined(HAVE_LIBUUID) && defined(HAVE_UUID_UUID_H)
	uuid_unparse(uuid, string);
#endif
}
