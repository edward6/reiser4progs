includedir		    = -I$(top_srcdir)/include

tail40_sources		    = tail40.c tail40_repair.c tail40.h tail40_repair.h

STATIC_LIBS                 = libtail40-static.la
libtail40_static_la_SOURCES = $(tail40_sources)
libtail40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                  = libtail40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES          = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libtail40_alone_la_SOURCES  = $(tail40_sources)
libtail40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

