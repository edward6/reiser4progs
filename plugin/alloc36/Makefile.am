includedir		     = -I$(top_srcdir)/include
libdir			     = $(plugindir)

alloc36_sources		     = alloc36.c alloc36.h

noinst_LTLIBRARIES	     = liballoc36-static.la
lib_LTLIBRARIES		     = liballoc36.la

liballoc36_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
liballoc36_la_LDFLAGS	     = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
liballoc36_static_la_SOURCES = $(alloc36_sources)
else
liballoc36_la_LIBADD	     = $(top_builddir)/libaal/libaal.la	\
			       $(top_builddir)/libaux/libaux-static.la

liballoc36_la_SOURCES	     = $(alloc36_sources)
endif

INCLUDES = $(includedir)

