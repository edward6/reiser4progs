includedir		    = -I$(top_srcdir)/include

reg40_sources		    = reg40.c reg40_repair.c reg40.h reg40_repair.h

STATIC_LIBS                 = libreg40-static.la
libreg40_static_la_SOURCES  = $(reg40_sources)
libreg40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                   = libreg40-minimal.la
else
MINIMAL_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libreg40_minimal_la_SOURCES    = $(reg40_sources)
libreg40_minimal_la_CFLAGS     = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
