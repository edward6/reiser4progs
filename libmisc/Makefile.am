miscincludedir		= -I$(top_srcdir)/include

if ENABLE_MINIMAL
MINIMAL_LIBS		= libmisc-minimal.la
else
MINIMAL_LIBS =
endif

noinst_LTLIBRARIES	= libmisc.la $(MINIMAL_LIBS)

libmisc_la_SOURCES	= misc.c profile.c exception.c gauge.c ui.c \
			  mpressure.c

libmisc_la_LIBADD  	= @AAL_LIBS@ $(UUID_LIBS) @PROGS_LIBS@ \
			  $(top_builddir)/libaux/libaux-static.la

libmisc_la_CFLAGS       = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
libmisc_minimal_la_SOURCES = $(libmisc_la_SOURCES)

libmisc_minimal_la_LIBADD  = @AAL_LIBS@ $(UUID_LIBS) @PROGS_LIBS@ \
			     $(top_builddir)/libaux/libaux-minimal.la

libmisc_minimal_la_CFLAGS  = $(libmisc_la_CFLAGS)
endif

AM_CPPFLAGS                = $(miscincludedir)
