includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

nodeptr40_sources               = nodeptr40.c nodeptr40_repair.c nodeptr40.h

noinst_LTLIBRARIES		= libnodeptr40-static.la
lib_LTLIBRARIES			= libnodeptr40.la

libnodeptr40_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libnodeptr40_la_LDFLAGS		= $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libnodeptr40_static_la_SOURCES	= $(nodeptr40_sources)
else
libnodeptr40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libnodeptr40_la_SOURCES		= $(nodeptr40_sources)
endif

INCLUDES = $(includedir)

