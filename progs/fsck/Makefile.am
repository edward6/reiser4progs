sbin_PROGRAMS 		= fsck.reiser4
fsck_reiser4_SOURCES 	= fsck.c gauge.c fsck.h gauge.h

fsck_reiser4_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
			  $(top_builddir)/librepair/librepair.la \
			  $(top_builddir)/libmisc/libmisc.la \
			  @PROGS_LIBS@

fsck_reiser4_CFLAGS     = @GENERIC_CFLAGS@

INCLUDES		= -I$(top_srcdir)/include
