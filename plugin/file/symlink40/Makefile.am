includedir		        = -I$(top_srcdir)/include
libdir			        = $(plugindir)

symlink40_sources	        = symlink40.c symlink40.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libsymlink40-static.la
libsymlink40_static_la_SOURCES  = $(symlink40_sources)
libsymlink40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                     =
lib_LTLIBRARIES		        = libsymlink40.la
libsymlink40_la_LIBADD	        = $(top_builddir)/libaal/libaal.la \
			          $(top_builddir)/plugin/file/file40/libfile40-static.la
libsymlink40_la_SOURCES	        = $(symlink40_sources)
libsymlink40_la_LDFLAGS	        = $(PLUGIN_LDFLAGS)

endif

if ENABLE_ALONE
ALONE_LIBS                      = libsymlink40-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE

libsymlink40_alone_la_SOURCES   = $(symlink40_sources)
libsymlink40_alone_la_CFLAGS    = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin

endif

INCLUDES = $(includedir)
