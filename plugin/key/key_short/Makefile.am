includedir			= -I$(top_srcdir)/include
libdir				= $(plugindir)

key_short_sources               = key_short.c key_short.h

if ENABLE_MONOLITHIC
STATIC_LIBS                     = libkey_short-static.la
libkey_short_static_la_SOURCES	= $(key_short_sources)
libkey_short_static_la_LDFLAGS	= @PLUGIN_LDFLAGS@
libkey_short_static_la_CFLAGS	= @GENERIC_CFLAGS@
else
STATIC_LIBS                     = 
lib_LTLIBRARIES			= libkey_short.la
libkey_short_la_LIBADD		= @AAL_LIBS@ \
				  $(top_builddir)/libaux/libaux-static.la \
				  $(top_builddir)/plugin/key/key_common/libkey_common-static.la

libkey_short_la_SOURCES		= $(key_short_sources)
libkey_short_la_LDFLAGS		= @PLUGIN_LDFLAGS@
libkey_short_la_CFLAGS          = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libkey_short-alone.la
else
ALONE_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libkey_short_alone_la_SOURCES  	= $(key_short_sources)
libkey_short_alone_la_CFLAGS   	= @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
