includedir			   = -I$(top_srcdir)/include

sdext_symlink_sources		   = sdext_symlink.c sdext_symlink_repair.c

STATIC_LIBS                        = libsdext_symlink-static.la
libsdext_symlink_static_la_SOURCES = $(sdext_symlink_sources)
libsdext_symlink_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                         = libsdext_symlink-alone.la
else
ALONE_LIBS                         =
endif

noinst_LTLIBRARIES   	           = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libsdext_symlink_alone_la_SOURCES  = $(sdext_symlink_sources)
libsdext_symlink_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

