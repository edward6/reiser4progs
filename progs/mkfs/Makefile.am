sbin_PROGRAMS 		= mkfs.reiser4
mkfs_reiser4_SOURCES 	= mkfs.c

mkfs_reiser4_LDADD 	= $(top_builddir)/libreiser4/libreiser4.la \
			  $(top_builddir)/libmisc/libmisc.la \
			  $(PROGS_LIBS) $(UUID_LIBS)

mkfs_reiser4_LDFLAGS    = @MKFS_LDFLAGS@
mkfs_reiser4_CFLAGS 	= @GENERIC_CFLAGS@

INCLUDES		= -I$(top_srcdir)/include

uninstall-make_reiser4:
	if test -f $(DESTDIR)$(sbindir)/make_reiser4; then \
	    rm -f $(DESTDIR)$(sbindir)/make_reiser4; \
	fi;

install-make_reiser4: mkfs.reiser4 uninstall-make_reiser4
	if test -f $(DESTDIR)$(sbindir)/mkfs.reiser4; then \
	    ln $(DESTDIR)$(sbindir)/mkfs.reiser4 $(DESTDIR)$(sbindir)/make_reiser4; \
	fi;

install-data-local: install-make_reiser4
uninstall-local: uninstall-make_reiser4

