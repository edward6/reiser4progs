includedir		       = -I$(top_srcdir)/include

body40_sources                 = body40.c body40.h

libbody40_static_la_SOURCES    = $(body40_sources)
libbody40_static_la_CFLAGS     = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                     = libbody40-minimal.la
else
MINIMAL_LIBS                     =
endif

noinst_LTLIBRARIES	       = libbody40-static.la $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libbody40_minimal_la_SOURCES     = $(body40_sources)
libbody40_minimal_la_CFLAGS      = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
