includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

if ENABLE_COMPACT
key40_sources			= key40.c key40.h
else
key40_sources                   = key40.c key40_repair.c key40.h
endif


noinst_LTLIBRARIES		= libkey40-static.la
lib_LTLIBRARIES			= libkey40.la

libkey40_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libkey40_la_LDFLAGS		= $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libkey40_static_la_SOURCES	= $(key40_sources)
#else
libkey40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libkey40_la_SOURCES		= $(key40_sources)
#endif

INCLUDES = $(includedir)

