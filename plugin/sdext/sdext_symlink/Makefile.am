includedir			   = -I$(top_srcdir)/include
libdir				   = $(plugindir)

sdext_symlink_sources		   = sdext_symlink.c

if ENABLE_MONOLITHIC
STATIC_LIBS                        = libsdext_symlink-static.la
libsdext_symlink_static_la_SOURCES = $(sdext_symlink_sources)
libsdext_symlink_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                        =
lib_LTLIBRARIES			   = libsdext_symlink.la
libsdext_symlink_la_LIBADD	   = $(top_builddir)/libaal/libaal.la \
				     $(top_builddir)/libaux/libaux-static.la
libsdext_symlink_la_SOURCES	   = $(sdext_symlink_sources)
libsdext_symlink_la_LDFLAGS	   = $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                         = libsdext_symlink-alone.la
else
ALONE_LIBS                         =
endif

noinst_LTLIBRARIES   	           = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libsdext_symlink_alone_la_SOURCES  = $(sdext_symlink_sources)
libsdext_symlink_alone_la_CFLAGS   = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

