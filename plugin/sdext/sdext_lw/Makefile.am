includedir			= -I$(top_srcdir)/include

sdext_lw_sources		= sdext_lw.c sdext_lw_repair.c sdext_lw.h

STATIC_LIBS                     = libsdext_lw-static.la
libsdext_lw_static_la_SOURCES 	= $(sdext_lw_sources)
libsdext_lw_static_la_CFLAGS 	= @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libsdext_lw-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libsdext_lw_alone_la_SOURCES    = $(sdext_lw_sources)
libsdext_lw_alone_la_CFLAGS     = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

