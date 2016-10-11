includedir			  = -I$(top_srcdir)/include

rupasov_hash_sources		  = rupasov_hash.c

STATIC_LIBS                       = librupasov_hash-static.la
librupasov_hash_static_la_SOURCES = $(rupasov_hash_sources)
librupasov_hash_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_MINIMAL
MINIMAL_LIBS                         = librupasov_hash-minimal.la
else
MINIMAL_LIBS                         =
endif

noinst_LTLIBRARIES		   = $(STATIC_LIBS) $(MINIMAL_LIBS)

if ENABLE_MINIMAL
librupasov_hash_minimal_la_SOURCES   = $(rupasov_hash_sources)
librupasov_hash_minimal_la_CFLAGS    = @MINIMAL_CFLAGS@
endif

AM_CPPFLAGS = $(includedir)

