includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

nodeptr40_sources               = nodeptr40.c nodeptr40_repair.c nodeptr40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libnodeptr40-static.la
libnodeptr40_static_la_SOURCES	= $(nodeptr40_sources)
libnodeptr40_static_la_LDFLAGS	= @PLUGIN_LDFLAGS@
libnodeptr40_static_la_CFLAGS	= @GENERIC_CFLAGS@
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libnodeptr40.la
libnodeptr40_la_LIBADD		= @AAL_LIBS@

libnodeptr40_la_SOURCES		= $(nodeptr40_sources)
libnodeptr40_la_LDFLAGS		= @PLUGIN_LDFLAGS@
libnodeptr40_la_CFLAGS          = @GENERIC_CFLAGS@
endif

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

