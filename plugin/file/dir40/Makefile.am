includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

dir40_sources		    = dir40.c dir40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libdir40-static.la
libdir40_static_la_SOURCES  = $(dir40_sources)
libdir40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                 =
lib_LTLIBRARIES		    = libdir40.la
libdir40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
			      $(top_builddir)/plugin/file/object40/libobject40-static.la
libdir40_la_SOURCES	    = $(dir40_sources)
libdir40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

endif

if ENABLE_ALONE
ALONE_LIBS                   = libdir40-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE

libdir40_alone_la_SOURCES  = $(dir40_sources)
libdir40_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin

endif

INCLUDES = $(includedir)
