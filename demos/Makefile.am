noinst_PROGRAMS = list hash ls cat plugin

INCLUDES	= -I$(top_srcdir)/include

list_SOURCES	= list.c

list_CFLAGS	= @GENERIC_CFLAGS@

list_LDADD 	= -laal

hash_SOURCES	= hash.c

hash_CFLAGS	= @GENERIC_CFLAGS@

hash_LDADD 	= -laal

ls_SOURCES	= ls.c

ls_CFLAGS	= @GENERIC_CFLAGS@

ls_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/libmisc/libmisc.la \
		  $(PROGS_LIBS)

cat_SOURCES	= cat.c

cat_CFLAGS	= @GENERIC_CFLAGS@

cat_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/libmisc/libmisc.la \
		  $(PROGS_LIBS)

plugin_SOURCES	= plugin.c

plugin_CFLAGS	= @GENERIC_CFLAGS@

plugin_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/libmisc/libmisc.la \
		  $(PROGS_LIBS)
