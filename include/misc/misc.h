/* Copyright (C) 2001, 2002, 2003 by Hans Reiser, licensing governed by
   reiser4progs/COPYING.
   
   misc.h -- reiser4progs common include. */

#ifndef MISC_H
#define MISC_H

#include <aal/aal.h>
#include <misc/misc.h>
#include <linux/major.h>

#define NO_ERROR	0  /* no errors */
#define OPER_ERROR	8  /* bug in the code, assertions, etc. */
#define USER_ERROR	16 /* wrong parameters, not allowed values, etc. */

#ifndef MAJOR
#  define MAJOR(rdev) ((rdev) >> 8)
#  define MINOR(rdev) ((rdev) & 0xff)
#endif

#ifndef SCSI_DISK_MAJOR
#  define SCSI_DISK_MAJOR(maj) ((maj) == SCSI_DISK0_MAJOR || \
	((maj) >= SCSI_DISK1_MAJOR && (maj) <= SCSI_DISK7_MAJOR))
#endif

#ifndef SCSI_BLK_MAJOR
#  define SCSI_BLK_MAJOR(maj) (SCSI_DISK_MAJOR(maj) || \
	(maj) == SCSI_CDROM_MAJOR)
#endif

#ifndef IDE_DISK_MAJOR
#  ifdef IDE9_MAJOR
#  define IDE_DISK_MAJOR(maj) ((maj) == IDE0_MAJOR || \
	(maj) == IDE1_MAJOR || (maj) == IDE2_MAJOR || \
	(maj) == IDE3_MAJOR || (maj) == IDE4_MAJOR || \
	(maj) == IDE5_MAJOR || (maj) == IDE6_MAJOR || \
	(maj) == IDE7_MAJOR || (maj) == IDE8_MAJOR || \
	(maj) == IDE9_MAJOR)
#  else
#  define IDE_DISK_MAJOR(maj) ((maj) == IDE0_MAJOR || \
	(maj) == IDE1_MAJOR || (maj) == IDE2_MAJOR || \
	(maj) == IDE3_MAJOR || (maj) == IDE4_MAJOR || \
	(maj) == IDE5_MAJOR)
#  endif
#endif

#include "gauge.h"
#include "mpressure.h"
#include "exception.h"
#include "param.h"
#include "version.h"
#include "ui.h"

#define INVAL_DIG (0x7fffffff)

extern void misc_plugins_print(void);
extern long long misc_size2long(const char *str);
extern void misc_uuid_unparse(char *uuid, char *string);
extern void misc_upper_case(char *dst, const char *src);
extern long long misc_str2long(const char *str, int base);
extern errno_t misc_dev_mounted(const char *name, const char *ops);

#endif

