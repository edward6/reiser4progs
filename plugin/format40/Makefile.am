includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

format40_sources		= format40.c format40_repair.c format40.h

noinst_LTLIBRARIES		= libformat40-static.la
lib_LTLIBRARIES			= libformat40.la

libformat40_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libformat40_la_LDFLAGS		= $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libformat40_static_la_SOURCES	= $(format40_sources)
else
libformat40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la

libformat40_la_SOURCES		= $(format40_sources)
endif

INCLUDES = $(includedir)

