sbin_PROGRAMS 		= fsck.reiser4
fsck_reiser4_SOURCES 	= fsck.c backup.c fsck.h backup.h

fsck_reiser4_LDADD 	= $(top_builddir)/libmisc/libmisc.la \
			  $(top_builddir)/librepair/librepair.la \
			  $(top_builddir)/libreiser4/libreiser4.la \
			  @PROGS_LIBS@

fsck_reiser4_LDFLAGS    = @PROGS_LDFLAGS@
fsck_reiser4_CFLAGS     = @GENERIC_CFLAGS@

AM_CPPFLAGS		= -I$(top_srcdir)/include
