includedir		   = -I$(top_srcdir)/include
libdir			   = $(plugindir)

cde40_sources              = cde40.c cde40_repair.c cde40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                = libcde40-static.la
libcde40_static_la_SOURCES = $(cde40_sources)
libcde40_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libcde40_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS                = 
lib_LTLIBRARIES		   = libcde40.la
libcde40_la_LIBADD	   = @AAL_LIBS@ \
			     $(top_builddir)/libaux/libaux-static.la

libcde40_la_SOURCES	   = $(cde40_sources)
libcde40_la_LDFLAGS	   = @PLUGIN_LDFLAGS@
libcde40_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                 = libcde40-alone.la
else
ALONE_LIBS                 =
endif

noinst_LTLIBRARIES	   = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libcde40_alone_la_SOURCES  = $(cde40_sources)
libcde40_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
