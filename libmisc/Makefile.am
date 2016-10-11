noinst_LTLIBRARIES	= libmisc.la

libmisc_la_SOURCES	= misc.c profile.c exception.c gauge.c ui.c \
			  mpressure.c

libmisc_la_LIBADD  	= @AAL_LIBS@ $(UUID_LIBS) @PROGS_LIBS@ \
			  $(top_builddir)/libaux/libaux-static.la

libmisc_la_CFLAGS       = @GENERIC_CFLAGS@

AM_CPPFLAGS                = -I$(top_srcdir)/include
