includedir		        = -I$(top_srcdir)/include
libdir			        = $(plugindir)

node_large_sources              = node_large.c node_large_repair.c node_large.h

if ENABLE_MONOLITHIC
STATIC_LIBS		        = libnode_large-static.la
libnode_large_static_la_SOURCES = $(node_large_sources)
libnode_large_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libnode_large_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS		        =
lib_LTLIBRARIES		        = libnode_large.la
libnode_large_la_LIBADD	        = @AAL_LIBS@ $(top_builddir)/libaux/libaux-static.la \
				  $(top_builddir)/plugin/node/node_common/libnode_common-static.la

libnode_large_la_SOURCES	= $(node_large_sources)
libnode_large_la_LDFLAGS	= @PLUGIN_LDFLAGS@
libnode_large_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libnode_large-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   	        = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libnode_large_alone_la_SOURCES  = $(node_large_sources)
libnode_large_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

