includedir			  = -I$(top_srcdir)/include

rupasov_hash_sources		  = rupasov_hash.c

STATIC_LIBS                       = librupasov_hash-static.la
librupasov_hash_static_la_SOURCES = $(rupasov_hash_sources)
librupasov_hash_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
ALONE_LIBS                         = librupasov_hash-alone.la
else
ALONE_LIBS                         =
endif

noinst_LTLIBRARIES		   = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_STAND_ALONE
librupasov_hash_alone_la_SOURCES   = $(rupasov_hash_sources)
librupasov_hash_alone_la_CFLAGS    = @ALONE_CFLAGS@
endif

INCLUDES = $(includedir)

