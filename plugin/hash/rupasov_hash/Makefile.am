includedir			  = -I$(top_srcdir)/include
libdir				  = $(plugindir)

rupasov_hash_sources		  = rupasov_hash.c

if ENABLE_MONOLITHIC
STATIC_LIBS                       = librupasov_hash-static.la
librupasov_hash_static_la_SOURCES = $(rupasov_hash_sources)
librupasov_hash_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
else
STATIC_LIBS                       = 
lib_LTLIBRARIES			  = librupasov_hash.la
librupasov_hash_la_SOURCES	  = $(rupasov_hash_sources)
librupasov_hash_la_LDFLAGS	  = $(PLUGIN_LDFLAGS)
endif

if ENABLE_ALONE
ALONE_LIBS                         = librupasov_hash-alone.la
else
ALONE_LIBS                         =
endif

noinst_LTLIBRARIES		   = $(STATIC_LIBS) $(ALONE_LIBS)

if ENABLE_ALONE
librupasov_hash_alone_la_SOURCES   = $(rupasov_hash_sources)
librupasov_hash_alone_la_CFLAGS    = -DENABLE_ALONE -DENABLE_MONOLITHIC -O9 -fno-builtin
endif

INCLUDES = $(includedir)

