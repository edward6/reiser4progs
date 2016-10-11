includedir			   = -I$(top_srcdir)/include

sdext_symlink_sources		   = sdext_symlink.c sdext_symlink_repair.c

STATIC_LIBS                        = libsdext_symlink-static.la
libsdext_symlink_static_la_SOURCES = $(sdext_symlink_sources)
libsdext_symlink_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                         = libsdext_symlink-minimal.la
else
MINIMAL_LIBS                         =
endif

noinst_LTLIBRARIES   	           = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsdext_symlink_minimal_la_SOURCES  = $(sdext_symlink_sources)
libsdext_symlink_minimal_la_CFLAGS   = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)

