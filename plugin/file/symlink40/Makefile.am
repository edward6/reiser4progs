includedir		        = -I$(top_srcdir)/include
libdir			        = $(plugindir)

symlink40_sources	        = symlink40.c symlink40.h

if ENABLE_MONOLITHIC
noinst_LTLIBRARIES	        = libsymlink40-static.la
libsymlink40_static_la_SOURCES  = $(symlink40_sources)
else
lib_LTLIBRARIES		        = libsymlink40.la
libsymlink40_la_LIBADD	        = $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaal/libaux.la \
				  $(top_builddir)/plugin/file/file40/libfile40-static.la
libsymlink40_la_SOURCES	        = $(symlink40_sources)
endif

libsymlink40_static_la_LDFLAGS  = $(PLUGIN_LDFLAGS)
libsymlink40_la_LDFLAGS	        = $(PLUGIN_LDFLAGS)

INCLUDES = $(includedir)

