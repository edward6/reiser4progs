includedir	 	    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

sym40_sources	            = sym40.c sym40_repair.c sym40.h sym40_repair.h

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libsym40-static.la
libsym40_static_la_SOURCES  = $(sym40_sources)
libsym40_static_la_LDFLAGS  = @PLUGIN_LDFLAGS@
libsym40_static_la_CFLAGS   = @GENERIC_CFLAGS@
else
STATIC_LIBS                 =
lib_LTLIBRARIES		    = libsym40.la
libsym40_la_LIBADD	    = @AAL_LIBS@ \
			      $(top_builddir)/plugin/object/obj40/libobj40-static.la
libsym40_la_SOURCES	    = $(sym40_sources)
libsym40_la_LDFLAGS	    = @PLUGIN_LDFLAGS@
libsym40_la_CFLAGS          = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                  = libsym40-alone.la
else
ALONE_LIBS                  =
endif

noinst_LTLIBRARIES	    = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE

libsym40_alone_la_SOURCES   = $(sym40_sources)
libsym40_alone_la_CFLAGS    = @ALONE_CFLAGS@

endif

INCLUDES = $(includedir)
