sbin_PROGRAMS 		   = measurefs.reiser4
measurefs_reiser4_SOURCES  = measurefs.c

measurefs_reiser4_LDADD    = $(top_builddir)/libreiser4/libreiser4.la \
			     $(top_builddir)/progs/libmisc/libmisc.la \
			     $(PROGS_LIBS)

measurefs_reiser4_LDFLAGS  = @PROGS_LDFLAGS@
measurefs_reiser4_CFLAGS   = @GENERIC_CFLAGS@

INCLUDES		   = -I$(top_srcdir)/include -I$(top_srcdir)/progs/include
