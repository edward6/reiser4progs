if ENABLE_ALONE
SUBDIRS         = libaal libaux plugin libreiser4 include doc debug
else
SUBDIRS         = libaal libaux plugin libreiser4 librepair progs demos include doc debug
endif

EXTRA_DIST	= AUTHORS COPYING TODO NEWS BUGS README CREDITS THANKS \
		  libreiser4.m4 ltconfig

aclocaldir 	= $(datadir)/aclocal
aclocal_DATA	= libreiser4.m4

define all-sources
	( find ./ -name '*.[ch]' -print | grep -v SCCS )
endef

TAGS:
	$(all-sources) | etags -
