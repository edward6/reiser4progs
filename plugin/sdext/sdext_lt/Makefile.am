includedir			= -I$(top_srcdir)/include

sdext_lt_sources		= sdext_lt.c sdext_lt_repair.c sdext_lt.h

STATIC_LIBS                     = libsdext_lt-static.la
libsdext_lt_static_la_SOURCES 	= $(sdext_lt_sources)
libsdext_lt_static_la_CFLAGS 	= @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libsdext_lt-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libsdext_lt_alone_la_SOURCES    = $(sdext_lt_sources)
libsdext_lt_alone_la_CFLAGS     = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

