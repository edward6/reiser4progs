noinst_SCRIPTS  = detect

noinst_PROGRAMS = busy

INCLUDES	= -I$(top_srcdir)/include

busy_SOURCES	= busy.c

busy_LDFLAGS    = @PROGS_LDFLAGS@

busy_CFLAGS	= @GENERIC_CFLAGS@

busy_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
	          $(top_builddir)/libmisc/libmisc.la \
		  $(PROGS_LIBS)
