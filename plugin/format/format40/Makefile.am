includedir			= -I$(top_srcdir)/include

format40_sources		= format40.c format40_repair.c format40.h format40_repair.h

STATIC_LIBS                     = libformat40-static.la
libformat40_static_la_SOURCES	= $(format40_sources)
libformat40_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libformat40-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libformat40_alone_la_SOURCES    = $(format40_sources)
libformat40_alone_la_CFLAGS     = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

