includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

sdext_lw_sources		= sdext_lw.c sdext_lw_repair.c sdext_lw.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libsdext_lw-static.la
libsdext_lw_static_la_SOURCES 	= $(sdext_lw_sources)
libsdext_lw_static_la_LDFLAGS 	= @PLUGIN_LDFLAGS@
libsdext_lw_static_la_CFLAGS 	= @GENERIC_CFLAGS@
else
STATIC_LIBS                     =
lib_LTLIBRARIES			= libsdext_lw.la
libsdext_lw_la_LIBADD		= $(top_builddir)/libaal/libaal.la \
				  $(top_builddir)/libaux/libaux-static.la
libsdext_lw_la_SOURCES		= $(sdext_lw_sources)
libsdext_lw_la_LDFLAGS		= @PLUGIN_LDFLAGS@
libsdext_lw_la_CFLAGS           = @GENERIC_CFLAGS@
endif

if ENABLE_ALONE
ALONE_LIBS                      = libsdext_lw-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
libsdext_lw_alone_la_SOURCES    = $(sdext_lw_sources)
libsdext_lw_alone_la_CFLAGS     = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

