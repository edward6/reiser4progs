includedir			= -I$(top_srcdir)/include

key_short_sources               = key_short.c key_short.h key_short_repair.c

STATIC_LIBS                     = libkey_short-static.la
libkey_short_static_la_SOURCES	= $(key_short_sources)
libkey_short_static_la_CFLAGS	= @GENERIC_CFLAGS@

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
