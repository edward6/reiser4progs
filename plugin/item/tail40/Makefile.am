includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

tail40_sources		    = tail40.c

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libtail40-static.la
libtail40_static_la_SOURCES = $(tail40_sources)
libtail40_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
else
lib_LTLIBRARIES		    = libtail40.la
libtail40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
			      $(top_builddir)/libaux/libaux-static.la

libtail40_la_SOURCES	    = $(tail40_sources)
libtail40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                  = libtail40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES          = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libtail40_alone_la_SOURCES  = $(tail40_sources)
libtail40_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

