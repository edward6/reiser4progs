sbin_PROGRAMS 		   = measurefs.reiser4
measurefs_reiser4_SOURCES  = measurefs.c

measurefs_reiser4_LDADD    = $(top_builddir)/libmisc/libmisc.la \
			     $(top_builddir)/libreiser4/libreiser4.la \
			     $(PROGS_LIBS)

measurefs_reiser4_LDFLAGS  = @PROGS_LDFLAGS@
measurefs_reiser4_CFLAGS   = @GENERIC_CFLAGS@

AM_CPPFLAGS		   = -I$(top_srcdir)/include
