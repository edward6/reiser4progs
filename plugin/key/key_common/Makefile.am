includedir		         = -I$(top_srcdir)/include

key_common_sources               = key_common.c key_common.h

libkey_common_static_la_SOURCES  = $(key_common_sources)
libkey_common_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                       = libkey_common-alone.la
else
ALONE_LIBS                       =
endif

noinst_LTLIBRARIES	         = libkey_common-static.la $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libkey_common_alone_la_SOURCES   = $(key_common_sources)
libkey_common_alone_la_CFLAGS    = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
