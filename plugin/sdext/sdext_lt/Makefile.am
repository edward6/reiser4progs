includedir			= -I$(top_srcdir)/include

sdext_lt_sources		= sdext_lt.c sdext_lt_repair.c sdext_lt.h

STATIC_LIBS                     = libsdext_lt-static.la
libsdext_lt_static_la_SOURCES 	= $(sdext_lt_sources)
libsdext_lt_static_la_CFLAGS 	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libsdext_lt-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsdext_lt_minimal_la_SOURCES    = $(sdext_lt_sources)
libsdext_lt_minimal_la_CFLAGS     = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

