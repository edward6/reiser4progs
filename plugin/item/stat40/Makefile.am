includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

stat40_sources              = stat40.c stat40_repair.c stat40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libstat40-static.la
libstat40_static_la_SOURCES = $(stat40_sources)
libstat40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
else
lib_LTLIBRARIES		    = libstat40.la
libstat40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
			      $(top_builddir)/libaux/libaux-static.la

libstat40_la_SOURCES	    = $(stat40_sources)
libstat40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                  = libstat40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES          = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libstat40_alone_la_SOURCES  = $(stat40_sources)
libstat40_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

