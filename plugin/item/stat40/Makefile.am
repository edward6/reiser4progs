includedir		    = -I$(top_srcdir)/include

stat40_sources              = stat40.c stat40_repair.c stat40.h stat40_repair.h

STATIC_LIBS                 = libstat40-static.la
libstat40_static_la_SOURCES = $(stat40_sources)
libstat40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                  = libstat40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES          = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libstat40_alone_la_SOURCES  = $(stat40_sources)
libstat40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

