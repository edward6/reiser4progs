includedir			= -I$(top_srcdir)/include

nodeptr40_sources               = nodeptr40.c nodeptr40_repair.c nodeptr40.h nodeptr40_repair.h

STATIC_LIBS                     = libnodeptr40-static.la
libnodeptr40_static_la_SOURCES	= $(nodeptr40_sources)
libnodeptr40_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libnodeptr40-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES	        = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libnodeptr40_minimal_la_SOURCES   = $(nodeptr40_sources)
libnodeptr40_minimal_la_CFLAGS    = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)

