includedir			= -I$(top_srcdir)/include

key_alloc_sources               = key_alloc.c

STATIC_LIBS                     = libkey_alloc-static.la
libkey_alloc_static_la_SOURCES	= $(key_alloc_sources)
libkey_alloc_static_la_CFLAGS	= @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libkey_alloc-minimal.la
else
MINIMAL_LIBS                  	=
endif

noinst_LTLIBRARIES          	= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libkey_alloc_minimal_la_SOURCES  	= $(key_alloc_sources)
libkey_alloc_minimal_la_CFLAGS   	= @MINIMAL_CFLAGS@
endif

INCLUDES = $(includedir)
