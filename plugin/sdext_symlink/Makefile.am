includedir			   = -I$(top_srcdir)/include
libdir				   = $(plugindir)

sdext_symlink_sources		   = sdext_symlink.c

noinst_LTLIBRARIES		   = libsdext_symlink-static.la
lib_LTLIBRARIES			   = libsdext_symlink.la

libsdext_symlink_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libsdext_symlink_la_LDFLAGS	   = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
libsdext_symlink_static_la_SOURCES = $(sdext_symlink_sources)
else
libsdext_symlink_la_LIBADD	   = $(top_builddir)/libaal/libaal.la \
				     $(top_builddir)/libaux/libaux-static.la
libsdext_symlink_la_SOURCES	   = $(sdext_symlink_sources)
endif

INCLUDES = $(includedir)

