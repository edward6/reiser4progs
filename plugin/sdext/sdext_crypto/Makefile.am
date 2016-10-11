includedir			 = -I$(top_srcdir)/include

sdext_crypto_sources		 = sdext_crypto.c sdext_crypto.h sdext_crypto_repair.c

STATIC_LIBS                      = libsdext_crypto-static.la
libsdext_crypto_static_la_SOURCES = $(sdext_crypto_sources)
libsdext_crypto_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                      = libsdext_crypro-minimal.la
else
MINIMAL_LIBS                      =
endif

noinst_LTLIBRARIES   		= $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
libsdext_crypro_minimal_la_SOURCES = $(sdext_crypro_sources)
libsdext_crypro_minimal_la_CFLAGS  = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)
