includedir		     = -I$(top_srcdir)/include
libdir			     = $(plugindir)

alloc40_sources		     = alloc40.c alloc40.h

noinst_LTLIBRARIES	     = liballoc40-static.la
lib_LTLIBRARIES		     = liballoc40.la

liballoc40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
liballoc40_la_LDFLAGS	     = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
liballoc40_static_la_SOURCES = $(alloc40_sources)
else
liballoc40_la_LIBADD	     = $(top_builddir)/libaal/libaal.la \
			       $(top_builddir)/libaux/libaux-static.la

liballoc40_la_SOURCES	     = $(alloc40_sources)
endif

INCLUDES = $(includedir)
