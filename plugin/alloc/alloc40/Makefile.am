includedir		     = -I$(top_srcdir)/include

alloc40_sources		     = alloc40.c alloc40_repair.c alloc40.h alloc40_repair.h

STATIC_LIBS                  = liballoc40-static.la
liballoc40_static_la_SOURCES = $(alloc40_sources)
liballoc40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                   = liballoc40-minimal.la
else
MINIMAL_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
liballoc40_minimal_la_SOURCES  = $(alloc40_sources)
liballoc40_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
