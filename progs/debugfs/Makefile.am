sbin_PROGRAMS 		= debugfs.reiser4
debugfs_reiser4_SOURCES	= debugfs.c

debugfs_reiser4_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
			  $(top_builddir)/progs/libmisc/libmisc.la \
			  $(PROGS_LIBS)
			  
debugfs_reiser4_LDFLAGS = $(PROGS_LDFLAGS)

INCLUDES		= -I$(top_srcdir)/include -I$(top_srcdir)/progs/include
