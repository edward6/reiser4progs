includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

if ENABLE_COMPACT
nodeptr40_sources		= nodeptr40.c nodeptr40.h
else
nodeptr40_sources               = nodeptr40.c nodeptr40_repair.c nodeptr40.h
endif

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES		= libnodeptr40-static.la

libnodeptr40_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libnodeptr40_static_la_SOURCES	= $(nodeptr40_sources)
else
lib_LTLIBRARIES			= libnodeptr40.la

libnodeptr40_la_LIBADD		= $(top_builddir)/libaal/libaal.la
libnodeptr40_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
libnodeptr40_la_SOURCES		= $(nodeptr40_sources)
endif

INCLUDES = $(includedir)

