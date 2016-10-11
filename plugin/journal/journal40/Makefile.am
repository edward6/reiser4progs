includedir			= -I$(top_srcdir)/include

journal40_sources		= journal40.c journal40_repair.c \
				  journal40.h journal40_repair.h

STATIC_LIBS                     = libjournal40-static.la
libjournal40_static_la_SOURCES	= $(journal40_sources)
libjournal40_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libjournal40-minimal.la
else
MINIMAL_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libjournal40_minimal_la_SOURCES  	= $(journal40_sources)
libjournal40_minimal_la_CFLAGS   	= @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
