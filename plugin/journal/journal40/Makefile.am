includedir			= -I$(top_srcdir)/include

journal40_sources		= journal40.c journal40_repair.c \
				  journal40.h journal40_repair.h

STATIC_LIBS                     = libjournal40-static.la
libjournal40_static_la_SOURCES	= $(journal40_sources)
libjournal40_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libjournal40-alone.la
else
ALONE_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libjournal40_alone_la_SOURCES  	= $(journal40_sources)
libjournal40_alone_la_CFLAGS   	= @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)
