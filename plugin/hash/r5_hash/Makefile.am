includedir		     = -I$(top_srcdir)/include
libdir			     = $(plugindir)

r5_hash_sources		     = r5_hash.c

if ENABLE_MONOLITHIC
STATIC_LIBS                  = libr5_hash-static.la
libr5_hash_static_la_SOURCES = $(r5_hash_sources)
libr5_hash_static_la_LDFLAGS = @PLUGIN_LDFLAGS@
libr5_hash_static_la_CFLAGS  = @GENERIC_CFLAGS@
else
STATIC_LIBS                  = 
lib_LTLIBRARIES		     = libr5_hash.la
libr5_hash_la_SOURCES	     = $(r5_hash_sources)
libr5_hash_la_LDFLAGS	     = @PLUGIN_LDFLAGS@
libr5_hash_la_CFLAGS         = @GENERIC_CFLAGS@
endif

if ENABLE_STAND_ALONE
ALONE_LIBS                   = libr5_hash-alone.la
else
ALONE_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libr5_hash_alone_la_SOURCES  = $(r5_hash_sources)
libr5_hash_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

