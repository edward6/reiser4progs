sbin_PROGRAMS 		   = resizefs.reiser4
resizefs_reiser4_SOURCES   = resizefs.c

resizefs_reiser4_LDADD     = $(top_builddir)/libreiser4/libreiser4.la \
			     $(top_builddir)/libmisc/libmisc.la \
			     $(PROGS_LIBS)

resizefs_reiser4_LDFLAGS   = @PROGS_LDFLAGS@
resizefs_reiser4_CFLAGS    = @GENERIC_CFLAGS@

INCLUDES		   = -I$(top_srcdir)/include
