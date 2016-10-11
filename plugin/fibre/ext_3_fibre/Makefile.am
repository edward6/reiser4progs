includedir			= -I$(top_srcdir)/include

ext_3_fibre_sources		= ext_3_fibre.c

STATIC_LIBS			= libext_3_fibre-static.la
libext_3_fibre_static_la_SOURCES= $(ext_3_fibre_sources)
libext_3_fibre_static_la_CFLAGS = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS			= libext_3_fibre-minimal.la
else
MINIMAL_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libext_3_fibre_minimal_la_SOURCES = $(ext_3_fibre_sources)
libext_3_fibre_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
