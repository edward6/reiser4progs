includedir		     = -I$(top_srcdir)/include

alloc40_sources		     = alloc40.c alloc40_repair.c alloc40.h alloc40_repair.h

STATIC_LIBS                  = liballoc40-static.la
liballoc40_static_la_SOURCES = $(alloc40_sources)
liballoc40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                   = liballoc40-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
liballoc40_alone_la_SOURCES  = $(alloc40_sources)
liballoc40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
