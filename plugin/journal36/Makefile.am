includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

journal36_sources		= journal36.c journal36.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES		= libjournal36-static.la

libjournal36_static_la_LIBADD	= 
libjournal36_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libjournal36_static_la_SOURCES	= $(journal36_sources)
else
lib_LTLIBRARIES			= libjournal36.la

libjournal36_la_LIBADD		= $(top_builddir)/libaal/libaal.la
libjournal36_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
libjournal36_la_SOURCES		= $(journal36_sources)
endif

INCLUDES = $(includedir)

