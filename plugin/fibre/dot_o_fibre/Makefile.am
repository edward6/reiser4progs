includedir			= -I$(top_srcdir)/include

dot_o_fibre_sources		= dot_o_fibre.c

STATIC_LIBS			= libdot_o_fibre-static.la
libdot_o_fibre_static_la_SOURCES= $(dot_o_fibre_sources)
libdot_o_fibre_static_la_CFLAGS = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS			= libdot_o_fibre-alone.la
else
ALONE_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libdot_o_fibre_alone_la_SOURCES = $(dot_o_fibre_sources)
libdot_o_fibre_alone_la_CFLAGS  = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
