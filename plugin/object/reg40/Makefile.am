includedir		    = -I$(top_srcdir)/include
libdir			    = $(plugindir)

reg40_sources		    = reg40.c reg40_repair.c reg40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                 = libreg40-static.la
libreg40_static_la_SOURCES  = $(reg40_sources)
libreg40_static_la_LDFLAGS  = @PLUGIN_LDFLAGS@
libreg40_static_la_CFLAGS   = @GENERIC_CFLAGS@
else
STATIC_LIBS                 =
lib_LTLIBRARIES		    = libreg40.la
libreg40_la_LIBADD	    = @AAL_LIBS@ \
			      $(top_builddir)/plugin/object/obj40/libobj40-static.la
libreg40_la_SOURCES	    = $(reg40_sources)
libreg40_la_LDFLAGS	    = @PLUGIN_LDFLAGS@
libreg40_la_CFLAGS          = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                   = libreg40-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libreg40_alone_la_SOURCES    = $(reg40_sources)
libreg40_alone_la_CFLAGS     = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
