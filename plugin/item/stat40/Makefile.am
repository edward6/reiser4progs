includedir		    = -I$(top_srcdir)/include

stat40_sources              = stat40.c stat40_repair.c stat40.h stat40_repair.h

STATIC_LIBS                 = libstat40-static.la
libstat40_static_la_SOURCES = $(stat40_sources)
libstat40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                  = libstat40-minimal.la
else
MINIMAL_LIBS                  =
endif

noinst_LTLIBRARIES          = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libstat40_minimal_la_SOURCES  = $(stat40_sources)
libstat40_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)

