sbin_PROGRAMS 		= cpfs.reiser4
cpfs_reiser4_SOURCES 	= cpfs.c

cpfs_reiser4_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
			  $(top_builddir)/libmisc/libmisc.la \
			  $(PROGS_LIBS)

cpfs_reiser4_LDFLAGS    = @CPFS_LDFLAGS@
cpfs_reiser4_CFLAGS 	= @GENERIC_CFLAGS@

INCLUDES		= -I$(top_srcdir)/include
