noinst_PROGRAMS = list ls cat

list_SOURCES	= list.c

list_LDFLAGS	= $(PROGS_LDFLAGS)

list_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la

ls_SOURCES	= ls.c

ls_LDFLAGS	= $(PROGS_LDFLAGS)

ls_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/progs/libmisc/libmisc.la

cat_SOURCES	= cat.c

cat_LDFLAGS	= $(PROGS_LDFLAGS)

cat_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/progs/libmisc/libmisc.la

INCLUDES	= -I$(top_srcdir)/include -I$(top_srcdir)/progs/include

