includedir			 = -I$(top_srcdir)/include

sdext_plugid_sources		 = sdext_plugid.c sdext_plugid_repair.c sdext_plugid.h

STATIC_LIBS                      = libsdext_plugid-static.la
libsdext_plugid_static_la_SOURCES = $(sdext_plugid_sources)
libsdext_plugid_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                      = libsdext_plugid-alone.la
else
ALONE_LIBS                      =
endif

noinst_LTLIBRARIES   		= $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
libsdext_plugid_alone_la_SOURCES = $(sdext_plugid_sources)
libsdext_plugid_alone_la_CFLAGS  = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

