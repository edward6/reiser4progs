includedir	 	    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

spl40_sources	            = spl40.c spl40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libspl40-static.la
libspl40_static_la_SOURCES  = $(spl40_sources)
libspl40_static_la_LDFLAGS  = @PLUGIN_LDFLAGS@
libspl40_static_la_CFLAGS   = @GENERIC_CFLAGS@
else
STATIC_LIBS                 =
lib_LTLIBRARIES		    = libspl40.la
libspl40_la_LIBADD	    = @AAL_LIBS@ \
			      $(top_builddir)/plugin/object/obj40/libobj40-static.la
libspl40_la_SOURCES	    = $(spl40_sources)
libspl40_la_LDFLAGS	    = @PLUGIN_LDFLAGS@
libspl40_la_CFLAGS          = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                  = libspl40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES	    = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE

libspl40_alone_la_SOURCES   = $(spl40_sources)
libspl40_alone_la_CFLAGS    = @ALONE_CFLAGS@

endif

INCLUDES = $(includedir)
