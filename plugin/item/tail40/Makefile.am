includedir		    = -I$(top_srcdir)/include

tail40_sources		    = tail40.c tail40_repair.c tail40.h tail40_repair.h

STATIC_LIBS                 = libtail40-static.la
libtail40_static_la_SOURCES = $(tail40_sources)
libtail40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                  = libtail40-minimal.la
else
MINIMAL_LIBS                  =
endif

noinst_LTLIBRARIES          = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libtail40_minimal_la_SOURCES  = $(tail40_sources)
libtail40_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

