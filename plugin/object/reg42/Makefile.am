includedir		    = -I$(top_srcdir)/include

reg42_sources		    = reg42.c

STATIC_LIBS                 = libreg42-static.la
libreg42_static_la_SOURCES  = $(reg42_sources)
libreg42_static_la_CFLAGS   = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                   = libreg42-minimal.la
else
MINIMAL_LIBS                   =
endif

noinst_LTLIBRARIES	     = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libreg42_minimal_la_SOURCES    = $(reg42_sources)
libreg42_minimal_la_CFLAGS     = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
