includedir		    = -I$(top_srcdir)/include

node40_sources              = node40.c node40_repair.c node40.h node40_repair.h

STATIC_LIBS		    = libnode40-static.la
libnode40_static_la_SOURCES = $(node40_sources)
libnode40_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                  = libnode40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES   	    = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libnode40_alone_la_SOURCES  = $(node40_sources)
libnode40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
