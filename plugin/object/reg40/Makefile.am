includedir		    = -I$(top_srcdir)/include

reg40_sources		    = reg40.c reg40_repair.c reg40.h reg40_repair.h

STATIC_LIBS                 = libreg40-static.la
libreg40_static_la_SOURCES  = $(reg40_sources)
libreg40_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                   = libreg40-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libreg40_alone_la_SOURCES    = $(reg40_sources)
libreg40_alone_la_CFLAGS     = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
