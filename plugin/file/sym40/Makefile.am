includedir	 	    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

sym40_sources	            = sym40.c sym40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libsym40-static.la
libsym40_static_la_SOURCES  = $(sym40_sources)
libsym40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                 =
lib_LTLIBRARIES		    = libsym40.la
libsym40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
			      $(top_builddir)/plugin/file/object40/libobject40-static.la
libsym40_la_SOURCES	    = $(sym40_sources)
libsym40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

endif

if ENABLE_ALONE
ALONE_LIBS                  = libsym40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES	    = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE

libsym40_alone_la_SOURCES   = $(sym40_sources)
libsym40_alone_la_CFLAGS    = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin

endif

INCLUDES = $(includedir)
