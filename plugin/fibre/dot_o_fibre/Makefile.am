includedir			= -I$(top_srcdir)/include

dot_o_fibre_sources		= dot_o_fibre.c

STATIC_LIBS			= libdot_o_fibre-static.la
libdot_o_fibre_static_la_SOURCES= $(dot_o_fibre_sources)
libdot_o_fibre_static_la_CFLAGS = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS			= libdot_o_fibre-minimal.la
else
MINIMAL_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libdot_o_fibre_minimal_la_SOURCES = $(dot_o_fibre_sources)
libdot_o_fibre_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
