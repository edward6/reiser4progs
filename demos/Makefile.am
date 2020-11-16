noinst_SCRIPTS  = detect

noinst_PROGRAMS = busy

AM_CPPFLAGS	= -I$(top_srcdir)/include

busy_SOURCES	= misc.c ls.c stat.c create.c ln.c rm.c cp.c trunc.c busy.c busy.h

busy_LDFLAGS    = @PROGS_LDFLAGS@

busy_CFLAGS	= @GENERIC_CFLAGS@

busy_LDADD 	= $(top_builddir)/libmisc/libmisc.la \
	          $(top_builddir)/libreiser4/libreiser4.la \
		  $(PROGS_LIBS)

EXTRA_DIST      = $(noinst_SCRIPTS)
