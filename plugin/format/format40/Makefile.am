includedir			= -I$(top_srcdir)/include

format40_sources		= format40.c format40_repair.c format40.h format40_repair.h

STATIC_LIBS                     = libformat40-static.la
libformat40_static_la_SOURCES	= $(format40_sources)
libformat40_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libformat40-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES	        = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libformat40_minimal_la_SOURCES    = $(format40_sources)
libformat40_minimal_la_CFLAGS     = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)

