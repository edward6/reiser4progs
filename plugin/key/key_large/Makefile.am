includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

key_large_sources               = key_large.c key_large.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libkey_large-static.la
libkey_large_static_la_SOURCES	= $(key_large_sources)
libkey_large_static_la_LDFLAGS	= @PLUGIN_LDFLAGS@
libkey_large_static_la_CFLAGS	= @GENERIC_CFLAGS@
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libkey_large.la
libkey_large_la_LIBADD		= @AAL_LIBS@ \
				  $(top_builddir)/libaux/libaux-static.la

libkey_large_la_SOURCES		= $(key_large_sources)
libkey_large_la_LDFLAGS		= @PLUGIN_LDFLAGS@
libkey_large_la_CFLAGS          = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libkey_large-alone.la
else
ALONE_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libkey_large_alone_la_SOURCES  	= $(key_large_sources)
libkey_large_alone_la_CFLAGS   	= @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
