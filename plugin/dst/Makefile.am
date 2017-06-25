includedir			= -I$(top_srcdir)/include

dst_sources			= dst.c		

STATIC_LIBS			= libdst-static.la
libdst_static_la_SOURCES	= $(dst_sources)
libdst_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS			= libdst-minimal.la
else
MINIMAL_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libdst_minimal_la_SOURCES = $(dst_sources)
libdst_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
