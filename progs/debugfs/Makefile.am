sbin_PROGRAMS 		 = debugfs.reiser4
debugfs_reiser4_SOURCES	 = debugfs.c debugfs.h print.c print.h browse.c \
			   browse.h types.h

debugfs_reiser4_LDADD 	 = $(top_builddir)/libmisc/libmisc.la \
			   $(top_builddir)/librepair/librepair.la \
			   $(top_builddir)/libreiser4/libreiser4.la \
			   $(PROGS_LIBS)

debugfs_reiser4_LDFLAGS  = @PROGS_LDFLAGS@
debugfs_reiser4_CFLAGS   = @GENERIC_CFLAGS@

AM_CPPFLAGS		 = -I$(top_srcdir)/include
