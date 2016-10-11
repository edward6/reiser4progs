includedir			= -I$(top_srcdir)/include

compress_sources		= compress.c compress_mode.c cluster.c

STATIC_LIBS			= libcompress-static.la
libcompress_static_la_SOURCES	= $(compress_sources)
libcompress_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS			= libcompress-minimal.la
else
MINIMAL_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libcompress_minimal_la_SOURCES = $(compress_sources)
libcompress_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
