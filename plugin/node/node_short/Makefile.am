includedir		        = -I$(top_srcdir)/include
libdir			        = $(plugindir)

node_short_sources              = node_short.c node_short_repair.c node_short.h

if ENABLE_MONOLITHIC
STATIC_LIBS		        = libnode_short-static.la
libnode_short_static_la_SOURCES = $(node_short_sources)
libnode_short_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libnode_short_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS		        =
lib_LTLIBRARIES		        = libnode_short.la
libnode_short_la_LIBADD	        = @AAL_LIBS@ $(top_builddir)/libaux/libaux-static.la \
				  $(top_builddir)/plugin/node/node_common/libnode_common-static.la

libnode_short_la_SOURCES	= $(node_short_sources)
libnode_short_la_LDFLAGS	= @PLUGIN_LDFLAGS@
libnode_short_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libnode_short-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libnode_short_alone_la_SOURCES  = $(node_short_sources)
libnode_short_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

