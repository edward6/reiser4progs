includedir			  = -I$(top_srcdir)/include
libdir				  = $(plugindir)

rupasov_hash_sources		  = rupasov_hash.c

noinst_LTLIBRARIES		  = librupasov_hash-static.la
lib_LTLIBRARIES			  = librupasov_hash.la

librupasov_hash_static_la_LDFLAGS = $(PLUGIN_LDFLAGS)
librupasov_hash_la_LDFLAGS	  = $(PLUGIN_LDFLAGS)

if ENABLE_MONOLITHIC
librupasov_hash_static_la_SOURCES = $(rupasov_hash_sources)
else
librupasov_hash_la_SOURCES	  = $(rupasov_hash_sources)
endif

INCLUDES = $(includedir)

