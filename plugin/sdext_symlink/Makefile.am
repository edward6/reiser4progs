includedir			   = -I$(top_srcdir)/include
libdir				   = $(plugindir)

sdext_symlink_sources		   = sdext_symlink.c

lib_LTLIBRARIES			   = libsdext_symlink.la
noinst_LTLIBRARIES		   = libsdext_symlink-static.la

libsdext_symlink_la_LIBADD	   = 
libsdext_symlink_la_LDFLAGS	   = $(PLUGIN_LDFLAGS)
libsdext_symlink_la_SOURCES	   = $(sdext_symlink_sources)

libsdext_symlink_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
libsdext_symlink_static_la_SOURCES = $(sdext_symlink_sources)

INCLUDES = $(includedir)

