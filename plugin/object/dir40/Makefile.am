includedir		    = -I$(top_srcdir)/include

dir40_sources		    = dir40.c dir40_repair.c dir40.h dir40_repair.h

STATIC_LIBS                 = libdir40-static.la
libdir40_static_la_SOURCES  = $(dir40_sources)
libdir40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                   = libdir40-minimal.la
else
MINIMAL_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libdir40_minimal_la_SOURCES    = $(dir40_sources)
libdir40_minimal_la_CFLAGS     = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
