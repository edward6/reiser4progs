includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

nodeptr40_sources               = nodeptr40.c nodeptr40_repair.c nodeptr40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libnodeptr40-static.la
libnodeptr40_static_la_SOURCES	= $(nodeptr40_sources)
libnodeptr40_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libnodeptr40.la
libnodeptr40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libnodeptr40_la_SOURCES		= $(nodeptr40_sources)
libnodeptr40_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                      = libnodeptr40-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libnodeptr40_alone_la_SOURCES   = $(nodeptr40_sources)
libnodeptr40_alone_la_CFLAGS    = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

