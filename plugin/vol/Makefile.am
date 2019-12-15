includedir			= -I$(top_srcdir)/include

volume_sources			= volume.c

STATIC_LIBS			= libvolume-static.la
libvolume_static_la_SOURCES	= $(volume_sources)
libvolume_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS			= libvolume-minimal.la
else
MINIMAL_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libvolume_minimal_la_SOURCES = $(volume_sources)
libvolume_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
