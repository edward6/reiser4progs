includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

dir40_sources		    = dir40.c dir40_repair.c dir40.h dir40_repair.h

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libdir40-static.la
libdir40_static_la_SOURCES  = $(dir40_sources)
libdir40_static_la_LDFLAGS  = @PLUGIN_LDFLAGS@
libdir40_static_la_CFLAGS   = @GENERIC_CFLAGS@
else
STATIC_LIBS                 =
lib_LTLIBRARIES		    = libdir40.la
libdir40_la_LIBADD	    = @AAL_LIBS@ \
			      $(top_builddir)/plugin/object/obj40/libobj40-static.la
libdir40_la_SOURCES	    = $(dir40_sources)
libdir40_la_LDFLAGS	    = @PLUGIN_LDFLAGS@
libdir40_la_CFLAGS          = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                   = libdir40-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libdir40_alone_la_SOURCES    = $(dir40_sources)
libdir40_alone_la_CFLAGS     = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
