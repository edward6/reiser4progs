includedir		         = -I$(top_srcdir)/include

node_common_sources               = node_common.c node_common.h

libnode_common_static_la_SOURCES  = $(node_common_sources)
libnode_common_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                       = libnode_common-alone.la
else
ALONE_LIBS                       =
endif

noinst_LTLIBRARIES	         = libnode_common-static.la $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libnode_common_alone_la_SOURCES   = $(node_common_sources)
libnode_common_alone_la_CFLAGS    = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
