aalincludedir 		 = -I$(top_srcdir)/include

libaal_sources  	 = device.c file.c exception.c list.c malloc.c \
			   print.c string.c math.c debug.c bitops.c \
			   gauge.c block.c ui.c stream.c lru.c

if ENABLE_STAND_ALONE
ALONE_LIBS               = libaal-alone.la 
else
ALONE_LIBS               = 
endif

lib_LTLIBRARIES		 = libaal.la $(ALONE_LIBS)
noinst_LTLIBRARIES	 = libaal-static.la 

libaal_la_LDFLAGS 	 = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
			   -release $(LT_RELEASE)

libaal_la_SOURCES  	 = $(libaal_sources)

libaal_la_CFLAGS         = @GENERIC_CFLAGS@

libaal_static_la_SOURCES = $(libaal_sources)

libaal_static_la_CFLAGS  = @GENERIC_CFLAGS@

if ENABLE_STAND_ALONE
libaal_alone_la_SOURCES  = $(libaal_sources)
libaal_alone_la_CFLAGS   = @ALONE_CFLAGS@
endif

INCLUDES = $(aalincludedir)

