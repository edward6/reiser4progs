includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

if ENABLE_COMPACT
format40_sources		= format40.c format40.h
else
format40_sources		= format40.c format40_repair.c format40.h
endif

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES		= libformat40-static.la

libformat40_static_la_LIBADD	= 
libformat40_static_la_LDFLAGS	= $(PLUGIN_LDFLAGS)
libformat40_static_la_SOURCES	= $(format40_sources)
else
lib_LTLIBRARIES			= libformat40.la

libformat40_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la
				  
libformat40_la_LDFLAGS		= $(PLUGIN_LDFLAGS)
libformat40_la_SOURCES		= $(format40_sources)
endif

INCLUDES = $(includedir)

