includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

reg40_sources		    = reg40.c reg40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libreg40-static.la
libreg40_static_la_SOURCES  = $(reg40_sources)
libreg40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                 =
lib_LTLIBRARIES		    = libreg40.la
libreg40_la_LIBADD	    = $(top_builddir)/libaal/libaal.la \
			      $(top_builddir)/plugin/file/file40/libfile40-static.la
libreg40_la_SOURCES	    = $(reg40_sources)
libreg40_la_LDFLAGS	    = $(PLUGIN_LDFLAGS)

endif

if ENABLE_ALONE
ALONE_LIBS                   = libreg40-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE

libreg40_alone_la_SOURCES  = $(reg40_sources)
libreg40_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin

endif

INCLUDES = $(includedir)
