includedir			= -I$(top_srcdir)/include

ext_3_fibre_sources		= ext_3_fibre.c

STATIC_LIBS			= libext_3_fibre-static.la
libext_3_fibre_static_la_SOURCES= $(ext_3_fibre_sources)
libext_3_fibre_static_la_CFLAGS = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS			= libext_3_fibre-alone.la
else
ALONE_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libext_3_fibre_alone_la_SOURCES = $(ext_3_fibre_sources)
libext_3_fibre_alone_la_CFLAGS  = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
