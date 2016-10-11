includedir			= -I$(top_srcdir)/include

lexic_fibre_sources		= lexic_fibre.c

STATIC_LIBS			= liblexic_fibre-static.la
liblexic_fibre_static_la_SOURCES= $(lexic_fibre_sources)
liblexic_fibre_static_la_CFLAGS = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS			= liblexic_fibre-minimal.la
else
MINIMAL_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
liblexic_fibre_minimal_la_SOURCES = $(lexic_fibre_sources)
liblexic_fibre_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
