includedir			= -I$(top_srcdir)/include

sdext_lw_sources		= sdext_lw.c sdext_lw_repair.c sdext_lw.h

STATIC_LIBS                     = libsdext_lw-static.la
libsdext_lw_static_la_SOURCES 	= $(sdext_lw_sources)
libsdext_lw_static_la_CFLAGS 	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libsdext_lw-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsdext_lw_minimal_la_SOURCES    = $(sdext_lw_sources)
libsdext_lw_minimal_la_CFLAGS     = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

