includedir			= -I$(top_srcdir)/include

nodeptr40_sources               = nodeptr40.c nodeptr40_repair.c nodeptr40.h nodeptr40_repair.h

STATIC_LIBS                     = libnodeptr40-static.la
libnodeptr40_static_la_SOURCES	= $(nodeptr40_sources)
libnodeptr40_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libnodeptr40-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libnodeptr40_alone_la_SOURCES   = $(nodeptr40_sources)
libnodeptr40_alone_la_CFLAGS    = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

