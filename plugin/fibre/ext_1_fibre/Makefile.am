includedir			= -I$(top_srcdir)/include

ext_1_fibre_sources		= ext_1_fibre.c

STATIC_LIBS			= libext_1_fibre-static.la
libext_1_fibre_static_la_SOURCES= $(ext_1_fibre_sources)
libext_1_fibre_static_la_CFLAGS = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS			= libext_1_fibre-minimal.la
else
MINIMAL_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libext_1_fibre_minimal_la_SOURCES = $(ext_1_fibre_sources)
libext_1_fibre_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
