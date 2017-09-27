sbin_PROGRAMS 		   = volume.reiser4
volume_reiser4_SOURCES  = volume.c

volume_reiser4_LDADD    = $(top_builddir)/libreiser4/libreiser4.la \
			     $(top_builddir)/libmisc/libmisc.la \
			     $(PROGS_LIBS)

volume_reiser4_LDFLAGS  = @PROGS_LDFLAGS@
volume_reiser4_CFLAGS   = @GENERIC_CFLAGS@

INCLUDES		   = -I$(top_srcdir)/include
