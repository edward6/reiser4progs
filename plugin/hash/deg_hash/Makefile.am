includedir		      = -I$(top_srcdir)/include
libdir			      = $(plugindir)

deg_hash_sources	      = deg_hash.c

if ENABLE_MONOLITHIC
STATIC_LIBS	              = libdeg_hash-static.la
libdeg_hash_static_la_SOURCES = $(deg_hash_sources)
libdeg_hash_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libdeg_hash_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS	              =
lib_LTLIBRARIES		      = libdeg_hash.la
libdeg_hash_la_SOURCES	      = $(deg_hash_sources)
libdeg_hash_la_LDFLAGS	      = @PLUGIN_LDFLAGS@
libdeg_hash_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                    = libdeg_hash-alone.la
else
ALONE_LIBS                    =
endif

noinst_LTLIBRARIES	      = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libdeg_hash_alone_la_SOURCES  = $(deg_hash_sources)
libdeg_hash_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

