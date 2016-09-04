includedir			= -I$(top_srcdir)/include

format41_sources		= format41.c format41_repair.c format41.h format41_repair.h

STATIC_LIBS                     = libformat41-static.la
libformat41_static_la_SOURCES	= $(format41_sources)
libformat41_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libformat41-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES	        = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libformat41_minimal_la_SOURCES    = $(format41_sources)
libformat41_minimal_la_CFLAGS     = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

