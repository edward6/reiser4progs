includedir		       = -I$(top_srcdir)/include

body40_sources                 = body40.c body40.h

libbody40_static_la_SOURCES    = $(body40_sources)
libbody40_static_la_CFLAGS     = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                     = libbody40-alone.la
else
ALONE_LIBS                     =
endif

noinst_LTLIBRARIES	       = libbody40-static.la $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libbody40_alone_la_SOURCES     = $(body40_sources)
libbody40_alone_la_CFLAGS      = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
