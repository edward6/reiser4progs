includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

oid40_sources		    = oid40.c oid40.h

if ENABLE_MONOLITHIC
STATIC_LIBS		    = liboid40-static.la
liboid40_static_la_SOURCES  = $(oid40_sources)
liboid40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS		    =
lib_LTLIBRARIES		    = liboid40.la
liboid40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
	  		      $(top_builddir)/libaux/libaux-static.la

liboid40_la_SOURCES	    = $(oid40_sources)
liboid40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                  = liboid40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES   	    = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
liboid40_alone_la_SOURCES   = $(oid40_sources)
liboid40_alone_la_CFLAGS    = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

