includedir			= -I$(top_srcdir)/include

key_large_sources               = key_large.c key_large.h key_large_repair.c

STATIC_LIBS                     = libkey_large-static.la
libkey_large_static_la_SOURCES	= $(key_large_sources)
libkey_large_static_la_CFLAGS	= @GENERIC_CFLAGS@

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
