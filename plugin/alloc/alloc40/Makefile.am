includedir		     = -I$(top_srcdir)/include
libdir			     = $(plugindir)

alloc40_sources		     = alloc40.c alloc40_repair.c alloc40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                  = liballoc40-static.la
liballoc40_static_la_SOURCES = $(alloc40_sources)
liballoc40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                  =
lib_LTLIBRARIES		     = liballoc40.la
liballoc40_la_LIBADD	     = $(top_builddir)/libaal/libaal.la \
			       $(top_builddir)/libaux/libaux-static.la

liballoc40_la_SOURCES	     = $(alloc40_sources)
liballoc40_la_LDFLAGS	     = $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                   = liballoc40-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
liballoc40_alone_la_SOURCES  = $(alloc40_sources)
liballoc40_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)
