includedir			= -I$(top_srcdir)/include

ext_1_fibre_sources		= ext_1_fibre.c

STATIC_LIBS			= libext_1_fibre-static.la
libext_1_fibre_static_la_SOURCES= $(ext_1_fibre_sources)
libext_1_fibre_static_la_CFLAGS = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS			= libext_1_fibre-alone.la
else
ALONE_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libext_1_fibre_alone_la_SOURCES = $(ext_1_fibre_sources)
libext_1_fibre_alone_la_CFLAGS  = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
