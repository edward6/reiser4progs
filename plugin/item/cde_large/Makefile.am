includedir		       = -I$(top_srcdir)/include
libdir			       = $(plugindir)

cde_large_sources              = cde_large.c cde_large_repair.c cde_large.h

if ENABLE_MONOLITHIC
STATIC_LIBS                    = libcde_large-static.la
libcde_large_static_la_SOURCES = $(cde_large_sources)
libcde_large_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libcde_large_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS                    = 
lib_LTLIBRARIES		       = libcde_large.la
libcde_large_la_LIBADD	       = @AAL_LIBS@ \
				 $(top_builddir)/libaux/libaux-static.la

libcde_large_la_SOURCES	       = $(cde_large_sources)
libcde_large_la_LDFLAGS	       = @PLUGIN_LDFLAGS@
libcde_large_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                     = libcde_large-alone.la
else
ALONE_LIBS                     =
endif

noinst_LTLIBRARIES	       = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libcde_large_alone_la_SOURCES  = $(cde_large_sources)
libcde_large_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
