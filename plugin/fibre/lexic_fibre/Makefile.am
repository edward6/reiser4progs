includedir			= -I$(top_srcdir)/include

lexic_fibre_sources		= lexic_fibre.c

STATIC_LIBS			= liblexic_fibre-static.la
liblexic_fibre_static_la_SOURCES= $(lexic_fibre_sources)
liblexic_fibre_static_la_CFLAGS = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS			= liblexic_fibre-alone.la
else
ALONE_LIBS			=
endif

noinst_LTLIBRARIES		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
liblexic_fibre_alone_la_SOURCES = $(lexic_fibre_sources)
liblexic_fibre_alone_la_CFLAGS  = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
