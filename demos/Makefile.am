noinst_PROGRAMS = list ls cat plugin
INCLUDES	= -I$(top_srcdir)/include -I$(top_srcdir)/progs/include

list_SOURCES	= list.c

list_LDFLAGS	= @PROGS_LDFLAGS@
list_CFLAGS	= @GENERIC_CFLAGS@

list_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(PROGS_LIBS)

ls_SOURCES	= ls.c

ls_LDFLAGS	= @PROGS_LDFLAGS@
ls_CFLAGS	= @GENERIC_CFLAGS@

ls_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/progs/libmisc/libmisc.la \
		  $(PROGS_LIBS)

cat_SOURCES	= cat.c

cat_LDFLAGS	= @PROGS_LDFLAGS@
cat_CFLAGS	= @GENERIC_CFLAGS@

cat_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/progs/libmisc/libmisc.la \
		  $(PROGS_LIBS)

plugin_SOURCES	= plugin.c

plugin_LDFLAGS	= @PROGS_LDFLAGS@
plugin_CFLAGS	= @GENERIC_CFLAGS@

plugin_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/progs/libmisc/libmisc.la \
		  $(PROGS_LIBS)
