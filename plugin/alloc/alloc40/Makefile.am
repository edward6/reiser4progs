includedir		     = -I$(top_srcdir)/include
libdir			     = $(plugindir)

alloc40_sources		     = alloc40.c alloc40_repair.c alloc40.h alloc40_repair.h

if ENABLE_MONOLITHIC
STATIC_LIBS                  = liballoc40-static.la
liballoc40_static_la_SOURCES = $(alloc40_sources)
liballoc40_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
liballoc40_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS                  =
lib_LTLIBRARIES		     = liballoc40.la
liballoc40_la_LIBADD	     = @AAL_LIBS@ \
			       $(top_builddir)/libaux/libaux-static.la

liballoc40_la_SOURCES	     = $(alloc40_sources)
liballoc40_la_LDFLAGS	     = @PLUGIN_LDFLAGS@
liballoc40_la_CFLAGS	     = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                   = liballoc40-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
liballoc40_alone_la_SOURCES  = $(alloc40_sources)
liballoc40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
