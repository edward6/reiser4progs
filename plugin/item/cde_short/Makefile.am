includedir		       = -I$(top_srcdir)/include
libdir			       = $(plugindir)

cde_short_sources              = cde_short.c cde_short_repair.c cde_short.h

if ENABLE_MONOLITHIC
STATIC_LIBS                    = libcde_short-static.la
libcde_short_static_la_SOURCES = $(cde_short_sources)
libcde_short_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libcde_short_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS                    = 
lib_LTLIBRARIES		       = libcde_short.la
libcde_short_la_LIBADD	       = @AAL_LIBS@ \
				 $(top_builddir)/libaux/libaux-static.la

libcde_short_la_SOURCES	       = $(cde_short_sources)
libcde_short_la_LDFLAGS	       = @PLUGIN_LDFLAGS@
libcde_short_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                     = libcde_short-alone.la
else
ALONE_LIBS                     =
endif

noinst_LTLIBRARIES	       = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libcde_short_alone_la_SOURCES  = $(cde_short_sources)
libcde_short_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
