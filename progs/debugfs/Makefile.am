sbin_PROGRAMS 		 = debugfs.reiser4
debugfs_reiser4_SOURCES	 = debugfs.c debugfs.h print.c print.h browse.c \
			   browse.h types.h

debugfs_reiser4_LDADD 	 = $(top_builddir)/libreiser4/libreiser4.la \
			   $(top_builddir)/librepair/librepair.la \
			   $(top_builddir)/libmisc/libmisc.la \
			   $(PROGS_LIBS)

debugfs_reiser4_LDFLAGS  = @PROGS_LDFLAGS@
debugfs_reiser4_CFLAGS   = @GENERIC_CFLAGS@

INCLUDES		 = -I$(top_srcdir)/include
