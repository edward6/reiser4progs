includedir		     = -I$(top_srcdir)/include
libdir			     = $(plugindir)

alloc40_sources		     = alloc40.c alloc40.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES	     = liballoc40-static.la

liballoc40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
liballoc40_static_la_SOURCES = $(alloc40_sources)
else
lib_LTLIBRARIES		     = liballoc40.la

liballoc40_la_LIBADD	     = $(top_builddir)/libaal/libaal.la \
			       $(top_builddir)/libaux/libaux-static.la
	
liballoc40_la_LDFLAGS	     = $(PLUGIN_LDFLAGS)
liballoc40_la_SOURCES	     = $(alloc40_sources)
endif

INCLUDES = $(includedir)
